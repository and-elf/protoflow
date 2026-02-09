# protoflow-service

Service base class and mailbox infrastructure for message-passing services.

## Features

- Base `Service` class with inbound/outbound mailboxes
- Thread-safe `Mailbox<T>` template
- Runtime polling interface
- One message per service per cycle enforcement
- Message routing abstraction

---

## Service Base Class

### Overview

All protoflow services inherit from the `Service` base class, which provides:
- Inbound message queue
- Outbound message queue
- `poll()` interface called by runtime scheduler
- Message routing abstraction

### Basic Structure

```cpp
namespace protoflow::service {

class Service {
public:
    virtual ~Service() = default;

    /// Called by runtime when service is started
    virtual void start() {}

    /// Called by runtime when service is stopped
    virtual void stop() {}

    /// Called by runtime scheduler
    /// One message per service per cycle
    virtual void poll() {
        if (auto msg = read(); msg) {
            handle(std::move(*msg));
        }

        auto out = generate_outbound();
        for (auto&& msg : out) {
            write(std::move(msg));
        }
    }

    /// Called by runtime when routing message to this service
    void on_message(Message&& msg) {
        inbound.push(std::move(msg));
    }

protected:
    /// Read next message from inbound queue (non-blocking)
    [[nodiscard]] std::optional<Message> read();

    /// Queue message for outbound routing
    void write(Message&& msg);

    /// Override to handle incoming messages
    virtual void handle(Message&& msg) = 0;

    /// Override to generate outbound messages
    virtual std::vector<Message> generate_outbound() {
        return {};
    }

private:
    Mailbox<Message> inbound;
    Mailbox<Message> outbound;
};

} // namespace protoflow::service
```

---

## Mailbox

### Thread-Safe Message Queue

```cpp
template<typename T>
class Mailbox {
public:
    /// Push message to queue (thread-safe)
    void push(T&& item);

    /// Pop message from queue (non-blocking, thread-safe)
    [[nodiscard]] std::optional<T> pop();

    /// Check if mailbox is empty
    [[nodiscard]] bool empty() const;

    /// Get number of messages in mailbox
    [[nodiscard]] std::size_t size() const;

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
};
```

**Features**:
- Thread-safe operations via `std::mutex`
- Non-blocking `pop()` returns `std::optional<T>`
- RAII-based locking
- Move-only semantics for messages

---

## Usage Examples

### Basic Service Implementation

```cpp
#include <protoflow/service/service.hpp>

class SensorService : public protoflow::service::Service {
public:
    void start() override {
        // Initialize hardware sensor
        sensor_handle_ = initialize_sensor();
    }

    void stop() override {
        // Cleanup hardware sensor
        if (sensor_handle_) {
            close_sensor(*sensor_handle_);
            sensor_handle_.reset();
        }
    }
protected:
    void handle(Message&& msg) override {
        if (auto* req = std::get_if<ReadSensorRequest>(&msg)) {
            // Handle sensor read request
            current_reading_ = read_hardware_sensor();
        }
    }

    std::vector<Message> generate_outbound() override {
        std::vector<Message> messages;
        
        // Send sensor reading if we have one
        if (current_reading_) {
            messages.push_back(
                SensorReading{*current_reading_}
            );
            current_reading_.reset();
        }
        
        return messages;
    }

private:
    std::optional<float> current_reading_;
    
    float read_hardware_sensor() {
        // Hardware I/O happens here
        return 23.5f;
    }
};
```

### Service with FSM Integration

```cpp
class AppRegistrationService : public protoflow::service::Service {
public:
    AppRegistrationService()
        : fsm_(make_fsm(
              "app_registration",
              when<AppState::Unregistered>(AppEvent::Register)
                  .then([this](auto&&) { register_app(); })
                  .to<AppState::Registered>()
              | when<AppState::Registered>(AppEvent::Heartbeat)
                  .then([this](auto&&) { update_heartbeat(); })
                  .to<AppState::Alive>()
              | otherwise()
                  .then([this](auto&&) { reset_app(); })
                  .to<AppState::Unregistered>(),
              LoggingSink{"app_registration"}
          ))
    {}

protected:
    void handle(Message&& msg) override {
        // Dispatch message to FSM
        if (auto* reg = std::get_if<RegisterMessage>(&msg)) {
            fsm_.process(AppEvent::Register, *reg);
        } else if (auto* hb = std::get_if<HeartbeatMessage>(&msg)) {
            fsm_.process(AppEvent::Heartbeat, *hb);
        }
    }

    std::vector<Message> generate_outbound() override {
        // Generate responses based on FSM state
        std::vector<Message> messages;
        
        if (pending_ack_) {
            messages.push_back(RegisterAck{});
            pending_ack_ = false;
        }
        
        return messages;
    }

private:
    Fsm fsm_;
    bool pending_ack_ = false;
    
    void register_app() {
        // Registration logic
        pending_ack_ = true;
    }
    
    void update_heartbeat() {
        // Update heartbeat timestamp
    }
    
    void reset_app() {
        // Reset logic
    }
};
```

### Service with RPC Integration

```cpp
class HardwareArbitrationService : public protoflow::service::Service {
protected:
    void handle(Message&& msg) override {
        if (auto* req = std::get_if<HwAccessRequest>(&msg)) {
            handle_hw_request(*req);
        } else if (auto* rel = std::get_if<HwReleaseRequest>(&msg)) {
            handle_hw_release(*rel);
        }
    }

    std::vector<Message> generate_outbound() override {
        std::vector<Message> messages;
        
        // Send pending responses
        for (auto&& resp : pending_responses_) {
            messages.push_back(std::move(resp));
        }
        pending_responses_.clear();
        
        return messages;
    }

private:
    std::vector<Message> pending_responses_;
    std::map<std::string, HwResource> resources_;
    
    void handle_hw_request(const HwAccessRequest& req) {
        auto& resource = resources_[req.resource_id];
        
        if (resource.can_grant(req.mode)) {
            auto handle = resource.grant(req.client_id, req.mode);
            pending_responses_.push_back(
                HwAccessGranted{handle, req.resource_id}
            );
        } else {
            pending_responses_.push_back(
                HwAccessDenied{req.resource_id}
            );
        }
    }
    
    void handle_hw_release(const HwReleaseRequest& req) {
        auto& resource = resources_[req.resource_id];
        resource.release(req.handle);
        
        pending_responses_.push_back(
            HwReleaseAck{req.handle}
        );
    }
};
```

---

## Runtime Integration

### Scheduler Interaction

```cpp
class Runtime {
public:
    void start() {
        // Start all services
        for (auto& service : services_) {
            service->start();
        }
    }

    void stop() {
        // Stop all services in reverse order
        for (auto it = services_.rbegin(); it != services_.rend(); ++it) {
            (*it)->stop();
        }
    }

    void run_cycle() {
        // One message per service per cycle (deterministic)
        for (auto& service : services_) {
            service->poll();
            
            // Collect outbound messages
            while (auto msg = service->outbound.pop()) {
                route_message(std::move(*msg));
            }
        }
    }

private:
    void route_message(Message&& msg) {
        // Route to destination service(s)
        auto destinations = router_.route(msg);
        
        for (auto* dest : destinations) {
            dest->on_message(Message{msg});
        }
    }

    std::vector<std::unique_ptr<Service>> services_;
    Router router_;
};
```

---

## Guarantees

✓ **One message per cycle** - `poll()` processes at most one inbound message  
✓ **Non-blocking** - `read()` returns immediately if queue is empty  
✓ **Thread-safe** - Mailbox operations are protected by mutex  
✓ **Deterministic** - Services don't know about each other  
✓ **Observable** - All message flow goes through runtime  
✓ **No direct calls** - Services communicate only via messages  
✓ **Strict isolation** - Services cannot access each other directly  

---

## Implementation Notes

- Services are single-threaded (called by runtime scheduler)
- Mailbox uses `std::mutex` for thread safety (runtime may be multi-threaded)
- Messages are moved, not copied (zero-copy where possible)
- `poll()` can be overridden for custom behavior
- Services should not block in `handle()` or `generate_outbound()`
- Long-running operations should be split across multiple cycles
- FSM integration ensures protocol compliance
- Observable behavior through message routing

---

## Best Practices

### Use `start()` and `stop()` for Lifecycle Management

```cpp
// ✓ Good - initialize resources in start()
class MyService : public Service {
public:
    void start() override {
        connection_ = connect_to_database();
        timer_.start();
    }

    void stop() override {
        timer_.stop();
        connection_.reset();
    }

private:
    std::unique_ptr<Connection> connection_;
    Timer timer_;
};

// ✗ Bad - initialization in constructor
class MyService : public Service {
public:
    MyService() {
        connection_ = connect_to_database(); // Runtime not ready yet!
    }
};
```

### Keep `handle()` Fast

```cpp
// ✓ Good - fast processing
void handle(Message&& msg) override {
    if (auto* req = std::get_if<Request>(&msg)) {
        process_request(*req);
    }
}

// ✗ Bad - blocking I/O
void handle(Message&& msg) override {
    auto result = blocking_network_call(); // Don't do this!
    // ...
}
```

### Use FSMs for State Management

```cpp
// ✓ Good - FSM enforces protocol
class MyService : public Service {
    Fsm fsm_;
    
    void handle(Message&& msg) override {
        fsm_.process(to_event(msg));
    }
};

// ✗ Bad - manual state management
class MyService : public Service {
    enum State { A, B, C };
    State state_;
    
    void handle(Message&& msg) override {
        // Complex state transitions - error prone
    }
};
```

### Batch Outbound Messages

```cpp
// ✓ Good - return all messages at once
std::vector<Message> generate_outbound() override {
    std::vector<Message> messages;
    messages.push_back(/* ... */);
    messages.push_back(/* ... */);
    return messages;
}

// ✗ Bad - multiple write() calls
void handle(Message&& msg) override {
    write(/* ... */);  // Avoid scattered writes
    // ... more logic
    write(/* ... */);
}
```
