# protoflow-hw-client

Hardware arbitration client library for registered applications.

## Purpose

Provides a type-safe, RPC-based interface for unprivileged applications to access hardware resources through the main application's Hardware Arbitration Service.

## Key Features

- **RAII resource management** with `scoped_hw_access`
- **Type-safe operations** using `std::expected` for error handling
- **Support for read, write, and ioctl** operations
- **Access modes**: exclusive and shared
- **Timeout management** for resource leases
- **Transport-agnostic** design (works over any RPC transport)

---

## Protocol

### Hardware Access Request

```cpp
auto result = client.request_access(
    "/dev/ttyUSB0",                          // Resource path
    protocol::access_mode::exclusive,         // Access mode
    std::chrono::milliseconds{5000}          // Timeout
);
```

**Response**:
- **Success**: Returns `hw_access_result` with handle, timeout, and capabilities
- **Failure**: Returns error code (resource_not_found, resource_busy, permission_denied, etc.)

### Access Modes

- **`exclusive`**: Only one client at a time
- **`shared`**: Multiple clients can access (read-only resources)

---

## Operations

### Read

```cpp
auto data = client.read(handle, 256);  // Read up to 256 bytes

if (data) {
    std::vector<std::byte>& bytes = *data;
    // Process data
}
```

### Write

```cpp
std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}};
auto bytes_written = client.write(handle, data);

if (bytes_written) {
    size_t count = *bytes_written;
}
```

### IOCTL

```cpp
std::vector<std::byte> arg = {...};
auto response = client.ioctl(handle, IOCTL_CMD, arg);

if (response) {
    std::vector<std::byte>& result = *response;
}
```

### Release

```cpp
auto result = client.release(handle);

if (!result) {
    // Handle error
}
```

---

## RAII Resource Management

```cpp
auto access = client.request_access("/dev/ttyUSB0");

if (access) {
    // Automatic release on scope exit
    scoped_hw_access scoped{client, access->handle};
    
    // Use handle
    client.write(scoped.handle(), data);
    
    // Released automatically here
}
```

---

## Error Handling

All operations return `std::expected<T, hw_error>`:

```cpp
auto result = client.request_access("/dev/ttyUSB0");

if (!result) {
    switch (result.error()) {
        case hw_error::resource_not_found:
            // Handle not found
            break;
        case hw_error::resource_busy:
            // Handle busy
            break;
        case hw_error::permission_denied:
            // Handle permission error
            break;
        default:
            std::cerr << to_string(result.error()) << '\n';
            break;
    }
}
```

---

## Capabilities

Resources report capabilities on access:

```cpp
auto access = client.request_access("/dev/sda");

if (access) {
    bool can_read = access->capabilities & 
                   static_cast<uint32_t>(protocol::capability::read);
    bool can_write = access->capabilities & 
                    static_cast<uint32_t>(protocol::capability::write);
    bool can_ioctl = access->capabilities & 
                    static_cast<uint32_t>(protocol::capability::ioctl);
    bool is_seekable = access->capabilities & 
                      static_cast<uint32_t>(protocol::capability::seekable);
}
```

---

## Wire Protocol

### Message Structures

All messages are binary-packed with fixed sizes:

- `request_hw_access_msg`: 268 bytes
- `hw_access_granted_msg`: 16 bytes
- `hw_access_denied_msg`: 256 bytes
- `hw_write_msg`: 16 bytes + data
- `hw_read_msg`: 16 bytes
- `hw_read_response_msg`: 16 bytes + data
- `hw_ioctl_msg`: 16 bytes + args
- `hw_release_msg`: 8 bytes

### Command Codes

Extends `protoflow::rpc::protocol::cmd`:

```cpp
enum class cmd : uint16_t {
    request_hw_access = 100,
    hw_access_granted = 101,
    hw_access_denied = 102,
    hw_release = 103,
    hw_release_ack = 104,
    hw_write = 110,
    hw_write_ack = 111,
    hw_read = 112,
    hw_read_response = 113,
    hw_ioctl = 120,
    hw_ioctl_response = 121,
    hw_timeout = 130,
    hw_error = 131
};
```

---

## Integration

### CMakeLists.txt

```cmake
find_package(protoflow-hw-client REQUIRED)

target_link_libraries(my_app
    PRIVATE
        protoflow::hw-client
)
```

### Application Code

```cpp
#include <protoflow/hw/hw.hpp>

using namespace protoflow::hw;

// Setup transport (TCP to main app)
// ...

hw_client client{transport};

auto access = client.request_access("/dev/ttyUSB0");
if (access) {
    scoped_hw_access scoped{client, access->handle};
    
    // Perform operations
    std::vector<std::byte> cmd = {std::byte{0xAA}};
    client.write(scoped.handle(), cmd);
    
    auto response = client.read(scoped.handle(), 256);
}
```

---

## Security Model

1. **Main app runs as root** - exclusive hardware access
2. **Registered apps run unprivileged** - no direct hardware access
3. **All I/O proxied** through main app via RPC
4. **Hardware requirements declared** in app manifest
5. **Arbitration enforced** by main app

---

## Limitations

- Maximum I/O buffer size: 1 MB
- Maximum resource path: 255 characters
- Not thread-safe (use one client per thread or synchronize)
- Requires active RPC connection to main app

---

## Dependencies

- `protoflow::rpc` - RPC protocol and transport
- C++23 with `std::expected`

---

## See Also

- [README.md](README.md) - Full documentation and examples
- [ARCHITECTURE.md](../../../ARCHITECTURE.md) - System architecture
- [library-rpc.md](../../../docs/library-rpc.md) - RPC protocol
- Examples: [hw_client_example.cpp](examples/hw_client_example.cpp)
