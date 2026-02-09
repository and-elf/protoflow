# protoflow-rpc

RPC framework for inter-app communication over TCP.

## Features

- Wire protocol definitions
- Client/server base classes
- Handshake and framing enforcement
- Version negotiation
- Transport-agnostic design

---

## Wire Protocol

### RPC Header

```cpp
struct rpc_header {
    uint32_t magic;        // e.g. 'RPC0'
    uint32_t version;      // rpc::protocol::wire_version
    uint16_t cmd;
    uint16_t reserved;
    uint32_t payload_size;
};
```

**Fields**:
- `magic`: Protocol identifier (e.g., `'RPC0'`)
- `version`: Protocol version for compatibility checking
- `cmd`: Command/message type identifier
- `reserved`: Reserved for future use
- `payload_size`: Size of payload following header

---

## Server-Side Base Class

```cpp
class rpc_server_base {
protected:
    static constexpr uint32_t wire_version =
        rpc::protocol::wire_version;

    void handle_hello(const rpc_header& hdr,
                      span<const std::byte> payload,
                      tcp_client& client)
    {
        if (hdr.version != wire_version) {
            client.close(); // hard fail
            return;
        }

        send_hello_ack(client);
    }

private:
    void send_hello_ack(tcp_client& client) {
        rpc::protocol::hello_ack ack{ wire_version };
        send(client, rpc::protocol::cmd::hello, ack);
    }
};
```

**Responsibilities**:
- Enforce handshake protocol
- Validate protocol version
- Handle connection framing
- Hard fail on version mismatch

---

## Client-Side Base Class

```cpp
class rpc_client_base {
protected:
    static constexpr uint32_t wire_version =
        rpc::protocol::wire_version;

    void send_hello(tcp_client& client) {
        rpc::protocol::hello_msg msg{ wire_version };
        send(client, rpc::protocol::cmd::hello, msg);
    }
};
```

**Responsibilities**:
- Initiate handshake
- Send protocol version
- Wait for acknowledgment

---

## Application Interface

```cpp
class rpc_app {
public:
    virtual app_registration registration() const = 0;
    virtual html::fragment render_fragment(std::string_view id) = 0;
    virtual std::string get_state_json() const = 0;
};
```

**Methods**:
- `registration()`: Return app metadata (name, endpoints, etc.)
- `render_fragment(id)`: Generate HTML fragment for given ID
- `get_state_json()`: Return current app state as JSON string

---

## Component Organization

```
rpc_protocol.hpp
 └─ constexpr wire_version
 └─ rpc_header definition
 └─ protocol command enums

rpc_server_base
 └─ enforces handshake + framing
 └─ version validation

rpc_client_base
 └─ initiates connection
 └─ sends hello message

rpc_app
 └─ pure domain interface
 └─ fragments rendering
```

---

## Usage Example

### Server

```cpp
class MyServer : public rpc_server_base {
public:
    void handle_message(const rpc_header& hdr,
                       span<const std::byte> payload,
                       tcp_client& client) {
        switch (hdr.cmd) {
            case rpc::protocol::cmd::hello:
                handle_hello(hdr, payload, client);
                break;
            case rpc::protocol::cmd::register_app:
                handle_register(hdr, payload, client);
                break;
            // ... other commands
        }
    }
};
```

### Client

```cpp
class MyClient : public rpc_client_base {
public:
    void connect(tcp_client& client) {
        send_hello(client);
        // wait for hello_ack
        
        // send registration
        send_registration(client);
    }
};
```

### App Implementation

```cpp
class MyApp : public rpc_app {
public:
    app_registration registration() const override {
        return {
            .name = "my-app",
            .endpoints = {"/api/data", "/ui/dashboard"}
        };
    }

    html::fragment render_fragment(std::string_view id) override {
        if (id == "dashboard") {
            return build_dashboard();
        }

    std::string get_state_json() const override {
        return R"({
            "status": "operational",
            "data_count": 42,
            "last_update": "2026-02-09T12:34:56Z"
        })";
    }
        return html::empty();
    }
};
```

---

## Protocol Flow

### Connection Handshake

```
Client                          Server
  |                               |
  |------ HELLO (version) ------> |
  |                               | (validate version)
  | <----- HELLO_ACK ------------ |
  |                               |
  |------ REGISTER_APP ---------> |
  |                               | (store registration)
  | <----- REGISTER_ACK --------- |
  |                               |
  |------ HEARTBEAT ------------> |
  | <----- HEARTBEAT_ACK -------- |
  |                               |
```

### Fragment Request

```
Mai

### State Query

```
Main App                        Registered App
  |                               |
  |------ GET_STATE ------------> |
  |                               | (gather current state)
  | <----- STATE_JSON ----------- |
  |         (JSON payload)        |
  |                               |
```

### State Aggregation (All Apps)

```
Client/Browser                  Main App
  |                               |
  |------ GET /api/state -------> |
  |                               | (query all apps via RPC)
  |                               |---> [App 1: GET_STATE]
  |                               |---> [App 2: GET_STATE]
  |                               |---> [App N: GET_STATE]
  |                               |<--- [App 1: STATE_JSON]
  |                               |<--- [App 2: STATE_JSON]
  |                               |<--- [App N: STATE_JSON]
  |                               | (aggregate responses)
  | <----- Aggregated JSON ------ |
  |                               |
```n App                        Registered App
  |                               |
  |------ RENDER_FRAGMENT ------> |
  |         (fragment_id)         |
  |                               | (generate HTML)
  | <----- FRAGMENT_DATA -------- |
  |         (HTML payload)        |
  |                               |
```

---

## Implementation Notes

- Uses TCP as underlying transport (via `protoflow-transport-tcp`)
- Stateless protocol design
- Binary framing for efficiency
- Version negotiation at connection time
- Hard failure on version mismatch (no fallback)
- Suitable for both local and network connections
