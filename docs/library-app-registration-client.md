# protoflow-app-registration-client

Service library for registering applications with a central main server via RPC protocol over TCP.

## Features

- **Service-based architecture** - Inherits from `Service` base class
- **FSM-driven state management** - Uses compile-time validated state machine
- **RPC protocol integration** - Implements handshake, registration, and heartbeat
- **TCP transport** - Reliable connection management with reconnection support
- **Configuration-driven** - Simple API: provide config, start, stop
- **Observable behavior** - All state transitions logged via FSM sinks
- **Thread-safe** - Suitable for use in multi-threaded runtime

---

## Architecture

### Component Stack

```
AppRegistrationClient (Service)
    │
    ├─> FSM (State Management)
    │     └─> LoggingSink (Observable Transitions)
    │
    ├─> RPC Client Base (Protocol)
    │     └─> Protocol Messages (HELLO, REGISTER, HEARTBEAT)
    │
    └─> TCP Client (Transport)
          └─> Socket I/O
```

### State Machine

```
┌─────────────┐  Connect   ┌────────────┐  Connected  ┌──────────────┐
│ Disconnected├────────────>│ Connecting ├─────────────>│ Registering │
└─────────────┘             └──────┬─────┘             └──────┬───────┘
       ▲                           │                          │
       │                           │ Disconnected             │ RegistrationAck
       │                           ▼                          ▼
       │                    ┌──────────────┐            ┌────────────┐
       │                    │ Reconnecting │            │ Registered │
       │                    └──────┬───────┘            └─────┬──────┘
       │                           │                          │
       │                           │ Reconnect                │ HeartbeatTick
       │                           │                          │ HeartbeatAck
       │                           ▼                          │
       │                    ┌─────────────┐                   │
       └────────────────────┤   Failed    │<──────────────────┘
          Shutdown          └─────────────┘      FatalError
```

**States**:
- `Disconnected` - Initial state, not connected to server
- `Connecting` - TCP connection in progress, awaiting handshake
- `Registering` - Handshake complete, awaiting registration acknowledgment
- `Registered` - Successfully registered, sending periodic heartbeats
- `Reconnecting` - Connection lost, retrying connection
- `Failed` - Fatal error, requires manual intervention

**Events**:
- `Connect` - Initiate connection to server
- `Connected` - TCP connection established
- `HandshakeComplete` - HELLO/HELLO_ACK exchange successful
- `RegistrationAck` - Server acknowledged registration
- `HeartbeatTick` - Time to send heartbeat
- `HeartbeatAck` - Server acknowledged heartbeat
- `Disconnected` - Connection lost
- `Reconnect` - Retry connection
- `FatalError` - Unrecoverable error
- `Shutdown` - Manual shutdown requested

---

## Protocol Flow

### Connection & Registration

```
Client                          Server
  |                               |
  |--- TCP Connect ------------->|
  |                               |
  |--- HELLO (version) --------->|
  |                               | (validate version)
  |<-- HELLO_ACK ----------------|
  |                               |
  |--- REGISTER_APP ------------>|
  |    (name, version, endpoints)|
  |                               | (store registration)
  |<-- REGISTER_ACK (app_id) ----|
  |                               |
  |--- HEARTBEAT --------------->|
  |    (timestamp)                |
  |<-- HEARTBEAT_ACK ------------|
  |                               |
  |    ... periodic heartbeats ...|
  |                               |
```

### Reconnection on Failure

```
Client                          Server
  |                               |
  |===== Connection Lost =====   X
  |                               
  | (transition to Reconnecting)
  | (wait reconnect_delay)
  |
  |--- TCP Connect ------------->|
  |                               |
  |--- HELLO (version) --------->|
  |                               |
  |    ... repeat registration ...|
```

---

## API Reference

### Configuration

```cpp
namespace protoflow::app_registration_client {

struct Config {
    // Required
    std::string app_name;                       // Unique app identifier
    std::vector<std::string> endpoints;         // Endpoints this app provides
    
    // Connection
    std::string server_address = "localhost";   // Server hostname/IP
    uint16_t server_port = 8080;                // Server port
    
    // Timing
    std::chrono::seconds heartbeat_interval{30};      // Heartbeat frequency
    std::chrono::seconds connection_timeout{10};      // Connection timeout
    std::chrono::seconds reconnect_delay{5};          // Reconnect delay
    
    // Optional
    std::string version = "1.0.0";              // App version
    uint32_t max_reconnect_attempts = 0;        // 0 = infinite
    
    [[nodiscard]] bool is_valid() const noexcept;
};

} // namespace protoflow::app_registration_client
```

### Client Service

```cpp
class AppRegistrationClient : public service::Service {
public:
    /// Construct with configuration
    explicit AppRegistrationClient(Config config);
    
    /// Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;
    
    /// Query state
    [[nodiscard]] State current_state() const noexcept;
    [[nodiscard]] bool is_registered() const noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] const Config& config() const noexcept;
};
```

---

## Usage Examples

### Basic Usage

```cpp
#include <protoflow/app_registration_client.hpp>
#include <protoflow/runtime/runtime.hpp>

using namespace protoflow;

int main() {
    // Configure the client
    app_registration_client::Config config{
        .app_name = "sensor-aggregator",
        .version = "1.2.3",
        .endpoints = {
            "/api/sensors",
            "/api/readings"
        },
        .server_address = "localhost",
        .server_port = 8080,
        .heartbeat_interval = std::chrono::seconds{30},
        .connection_timeout = std::chrono::seconds{10},
        .reconnect_delay = std::chrono::seconds{5}
    };
    
    // Create client service
    auto client = std::make_unique<arc::AppRegistrationClient>(config);
    
    // Add to runtime
    runtime::Runtime rt;
    rt.add_service(std::move(client));
    
    // Run
    rt.start();
    while (rt.is_running()) {
        rt.run_cycle();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    rt.stop();
    
    return 0;
}
```

### With Custom Server

```cpp
// Production server configuration
app_registration_client::Config prod_config{
    .app_name = "analytics-worker",
    .version = "2.0.0",
    .endpoints = {
        "/api/analytics/process",
        "/api/analytics/status"
    },
    .server_address = "production.example.com",
    .server_port = 9000,
    .heartbeat_interval = std::chrono::seconds{60},
    .max_reconnect_attempts = 10  // Give up after 10 attempts
};

auto client = std::make_unique<arc::AppRegistrationClient>(prod_config);
```

### Monitoring State

```cpp
// Create client
auto client = std::make_unique<arc::AppRegistrationClient>(config);

// Check state periodically
void check_status() {
    if (client->is_registered()) {
        std::cout << "✓ Registered with server\n";
    } else {
        std::cout << "✗ Not registered (state: " 
                  << to_string(client->current_state()) << ")\n";
    }
}

// In main loop
while (true) {
    rt.run_cycle();
    check_status();
    std::this_thread::sleep_for(std::chrono::seconds{5});
}
```

### Multiple Apps in One Process

```cpp
// Different apps can run in the same process
auto sensor_client = std::make_unique<arc::AppRegistrationClient>(
    app_registration_client::Config{
        .app_name = "sensor-app",
        .endpoints = {"/api/sensors"}
    }
);

auto analytics_client = std::make_unique<arc::AppRegistrationClient>(
    app_registration_client::Config{
        .app_name = "analytics-app",
        .endpoints = {"/api/analytics"}
    }
);

runtime::Runtime rt;
rt.add_service(std::move(sensor_client));
rt.add_service(std::move(analytics_client));
rt.start();
```

### Graceful Shutdown

```cpp
#include <csignal>
#include <atomic>

std::atomic<bool> shutdown_requested{false};

void signal_handler(int) {
    shutdown_requested = true;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    runtime::Runtime rt;
    auto client = std::make_unique<arc::AppRegistrationClient>(config);
    rt.add_service(std::move(client));
    
    rt.start();
    while (!shutdown_requested && rt.is_running()) {
        rt.run_cycle();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    
    // Clean shutdown
    rt.stop();  // Calls stop() on all services, including AppRegistrationClient
    
    return 0;
}
```

---

## Integration with Other Libraries

### With FSM Observability

The client uses an FSM internally with a `LoggingSink`, so all state transitions are automatically logged:

```
[INFO] AppRegistrationClient[my-app]: Starting app registration client
[DEBUG] AppRegistrationClient[my-app]: State transition: Disconnected -> Connecting
[DEBUG] AppRegistrationClient[my-app]: TCP connection successful
[DEBUG] AppRegistrationClient[my-app]: HELLO_ACK received, protocol version 1
[INFO] AppRegistrationClient[my-app]: REGISTER_ACK received, app_id 42
[INFO] AppRegistrationClient[my-app]: Registration acknowledged
```

### With Service Runtime

```cpp
class MyApplication {
public:
    MyApplication(const std::string& app_name) {
        // Create registration client
        arc::Config config{
            .app_name = app_name,
            .endpoints = {"/api/data", "/ui/dashboard"}
        };
        
        registration_client_ = 
            std::make_unique<arc::AppRegistrationClient>(config);
        
        // Add to runtime
        runtime_.add_service(std::move(registration_client_));
        
        // Add other services
        runtime_.add_service(std::make_unique<MyDataService>());
        runtime_.add_service(std::make_unique<MyUIService>());
    }
    
    void run() {
        runtime_.start();
        while (runtime_.is_running()) {
            runtime_.run_cycle();
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        runtime_.stop();
    }

private:
    runtime::Runtime runtime_;
    std::unique_ptr<arc::AppRegistrationClient> registration_client_;
};
```

### With RPC Message Handlers

```cpp
class MyRpcService : public service::Service {
protected:
    void handle(service::Message&& msg) override {
        // Handle RPC requests from main server
        if (auto* fragment_req = std::get_if<RenderFragmentRequest>(&msg)) {
            auto html = render_fragment(fragment_req->fragment_id);
            write(FragmentResponse{html});
        }
    }
};

// Both services can coexist
runtime::Runtime rt;
rt.add_service(std::make_unique<arc::AppRegistrationClient>(config));
rt.add_service(std::make_unique<MyRpcService>());
```

---

## CMake Integration

### Enable in Build

```bash
cmake -B build -S . \
    -DBUILD_APP_REGISTRATION_CLIENT=ON \
    -DBUILD_SERVICE=ON \
    -DBUILD_FSM=ON \
    -DBUILD_RPC=ON \
    -DBUILD_TRANSPORT_TCP=ON \
    -DBUILD_LOGGING=ON

cmake --build build
```

### Link in Your Project

```cmake
# CMakeLists.txt
find_package(protoflow-app-registration-client REQUIRED)

add_executable(my_app main.cpp)

target_link_libraries(my_app
    PRIVATE
        protoflow::app-registration-client
)
```

---

## Error Handling

### Connection Failures

The client automatically handles connection failures via the FSM:

1. **Immediate failure** → Transition to `Reconnecting`
2. **Wait `reconnect_delay`** → Transition to `Connecting`
3. **Retry connection** → Repeat up to `max_reconnect_attempts`
4. **Max attempts reached** → Transition to `Failed`

**Observability**: All transitions logged at appropriate levels (ERROR for failures, INFO for reconnects).

### Version Mismatch

If the server responds with an incompatible protocol version:

```cpp
[ERROR] AppRegistrationClient[my-app]: Protocol version mismatch: 2 != 1
[WARN] AppRegistrationClient[my-app]: Connection lost
[DEBUG] State transition: Registering -> Reconnecting
```

The client will **not** retry in this case, as protocol mismatch is considered a fatal configuration error.

### Timeout Handling

If the server doesn't respond within `connection_timeout`:

```cpp
[WARN] AppRegistrationClient[my-app]: Connection timeout
[DEBUG] State transition: Connecting -> Reconnecting
```

---

## Best Practices

### 1. Use Meaningful App Names

```cpp
// ✓ Good - descriptive, unique
Config config{
    .app_name = "sensor-aggregator-v2",
    .endpoints = {"/api/sensors"}
};

// ✗ Bad - generic, collision-prone
Config config{
    .app_name = "app",
    .endpoints = {"/api"}
};
```

### 2. Set Appropriate Heartbeat Intervals

```cpp
// For critical services - frequent heartbeats
Config config{
    .heartbeat_interval = std::chrono::seconds{10}
};

// For batch/background services - infrequent heartbeats
Config config{
    .heartbeat_interval = std::chrono::seconds{120}
};
```

### 3. Configure Reconnection Strategy

```cpp
// Production - limit reconnection attempts
Config config{
    .max_reconnect_attempts = 10,
    .reconnect_delay = std::chrono::seconds{5}
};

// Development - infinite reconnections
Config config{
    .max_reconnect_attempts = 0,  // Infinite
    .reconnect_delay = std::chrono::seconds{2}
};
```

### 4. Validate Configuration

```cpp
Config config = load_from_file("config.json");

if (!config.is_valid()) {
    std::cerr << "Invalid configuration\n";
    return 1;
}

auto client = std::make_unique<arc::AppRegistrationClient>(config);
```

---

## Troubleshooting

### Client stuck in `Connecting`

**Cause**: Server not reachable or not listening on specified port.

**Solution**: Verify server is running and firewall allows connections.

```bash
# Test connectivity
nc -zv localhost 8080
```

### Client stuck in `Reconnecting`

**Cause**: Repeated connection failures.

**Solution**: Check `max_reconnect_attempts` and server availability.

### Client transitions to `Failed`

**Cause**: Fatal error (version mismatch, max reconnect attempts).

**Solution**: Check logs for specific error, fix configuration, restart service.

### No heartbeats sent

**Cause**: Client not in `Registered` state.

**Solution**: Ensure registration succeeds first. Check server logs for registration errors.

---

## Implementation Notes

- **Thread-safe**: Service methods called by runtime scheduler, TCP client synchronized internally
- **Non-blocking**: All operations return immediately; timeouts checked in `poll()`
- **Observable**: FSM transitions emit events to `LoggingSink`
- **Deterministic**: One message per cycle via `Service::poll()`
- **Testable**: FSM logic can be unit tested independently of network I/O

---

## Dependencies

- `protoflow::service` - Base service class and mailbox infrastructure
- `protoflow::fsm` - Finite state machine with compile-time validation
- `protoflow::rpc` - RPC protocol definitions and base classes
- `protoflow::transport-tcp` - TCP client for network I/O
- `protoflow::logging` - Logging macros and infrastructure

---

## See Also

- [library-service.md](library-service.md) - Service base class documentation
- [library-fsm.md](library-fsm.md) - FSM library documentation
- [library-rpc.md](library-rpc.md) - RPC protocol documentation
- [library-logging.md](library-logging.md) - Logging framework documentation
