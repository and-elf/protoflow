# protoflow-hw-client

Hardware arbitration client library for registered applications to request and use hardware resources through the main application's Hardware Arbitration Service.

## Overview

This library provides a client-side RPC interface for applications running as unprivileged users to safely access hardware resources. All hardware I/O operations are proxied through the main application running as root, ensuring proper isolation and arbitration.

## Features

- **Type-safe hardware access** with RAII resource management
- **RPC-based protocol** for hardware requests
- **Support for various operations**: read, write, ioctl
- **Access modes**: exclusive and shared resource access
- **Timeout management** for resource leases
- **Error handling** with `std::expected`
- **Transport-agnostic** design

## Architecture

```
[Registered App (unprivileged)]
         |
         | RPC over TCP
         v
[Main App (root) - Hardware Arbitration Service]
         |
         | Direct hardware access
         v
[Hardware Resource: /dev/ttyUSB0, GPIO, I2C, etc.]
```

## API Reference

### Hardware Client

```cpp
#include <protoflow/hw/hw_client.hpp>

using namespace protoflow::hw;

// Create client with transport
hw_client client{transport};

// Request hardware access
auto result = client.request_access(
    "/dev/ttyUSB0",
    protocol::access_mode::exclusive,
    std::chrono::milliseconds{5000}
);

if (result) {
    hw_handle handle = result->handle;
    // Use handle for I/O operations
}
```

### RAII Resource Management

```cpp
#include <protoflow/hw/hw_client.hpp>

using namespace protoflow::hw;

hw_client client{transport};

// Automatic release on scope exit
{
    auto access = client.request_access("/dev/ttyUSB0");
    if (access) {
        scoped_hw_access scoped{client, access->handle};
        
        // Perform operations
        std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}};
        auto write_result = client.write(scoped.handle(), data);
        
        // Handle automatically released here
    }
}
```

### Read Operations

```cpp
// Read up to 256 bytes
auto result = client.read(handle, 256);

if (result) {
    std::vector<std::byte>& data = *result;
    // Process data
}
```

### Write Operations

```cpp
std::vector<std::byte> data = {
    std::byte{0x48}, // 'H'
    std::byte{0x65}, // 'e'
    std::byte{0x6C}, // 'l'
    std::byte{0x6C}, // 'l'
    std::byte{0x6F}  // 'o'
};

auto result = client.write(handle, data);

if (result) {
    size_t bytes_written = *result;
    // Check bytes_written
}
```

### IOCTL Operations

```cpp
// Prepare ioctl argument
std::vector<std::byte> arg = {...};

auto result = client.ioctl(handle, IOCTL_REQUEST_CODE, arg);

if (result) {
    std::vector<std::byte>& response_data = *result;
    // Process response
}
```

### Resource Release

```cpp
// Manual release
auto result = client.release(handle);

if (!result) {
    // Handle error
    std::cerr << "Release failed: " << to_string(result.error()) << '\n';
}
```

## Protocol

### Message Flow

#### Request Access
```
App                             Main App (HW Service)
 |                                      |
 |--- REQUEST_HW_ACCESS --------------→ |
 |    (resource, mode, timeout)         |
 |                                      | [Check availability]
 | ←-- HW_ACCESS_GRANTED --------------|
 |     (handle, capabilities)           |
```

#### Write Operation
```
App                             Main App (HW Service)
 |                                      |
 |--- HW_WRITE ----------------------→ |
 |    (handle, data)                    |
 |                                      | [Perform write]
 | ←-- HW_WRITE_ACK -------------------|
 |     (bytes_written)                  |
```

#### Read Operation
```
App                             Main App (HW Service)
 |                                      |
 |--- HW_READ -----------------------→ |
 |    (handle, length)                  |
 |                                      | [Perform read]
 | ←-- HW_READ_RESPONSE ---------------|
 |     (data)                           |
```

#### Release Resource
```
App                             Main App (HW Service)
 |                                      |
 |--- HW_RELEASE --------------------→ |
 |    (handle)                          |
 |                                      | [Release resource]
 | ←-- HW_RELEASE_ACK -----------------|
```

### Access Modes

- **`exclusive`**: Only one client can access the resource at a time
- **`shared`**: Multiple clients can access simultaneously (for read-only resources)

### Error Codes

- `resource_not_found`: Hardware resource does not exist
- `resource_busy`: Resource already in use (exclusive mode)
- `permission_denied`: Application lacks permission for resource
- `invalid_handle`: Handle is invalid or expired
- `timeout`: Resource lease expired
- `io_error`: Hardware I/O operation failed
- `buffer_too_large`: Data buffer exceeds maximum size (1 MB)

### Capabilities

Resources report their capabilities when access is granted:

- `read`: Supports read operations
- `write`: Supports write operations
- `ioctl`: Supports ioctl operations
- `seekable`: Supports offset-based I/O

## Integration

### CMake

```cmake
find_package(protoflow-hw-client REQUIRED)

target_link_libraries(my_app
    PRIVATE
        protoflow::hw-client
)
```

### Example Application

```cpp
#include <protoflow/hw/hw.hpp>
#include <protoflow/rpc/rpc_base.hpp>
#include <iostream>

using namespace protoflow;

int main() {
    // Setup transport (TCP connection to main app)
    // ... transport setup code ...
    
    hw::hw_client hw_client{transport};
    
    // Request serial port access
    auto access = hw_client.request_access(
        "/dev/ttyUSB0",
        hw::protocol::access_mode::exclusive,
        std::chrono::seconds{30}
    );
    
    if (!access) {
        std::cerr << "Failed to access hardware: " 
                  << hw::to_string(access.error()) << '\n';
        return 1;
    }
    
    hw::scoped_hw_access scoped{hw_client, access->handle};
    
    // Send data
    std::vector<std::byte> cmd = {std::byte{0xAA}, std::byte{0x55}};
    auto write_result = hw_client.write(scoped.handle(), cmd);
    
    if (!write_result) {
        std::cerr << "Write failed\n";
        return 1;
    }
    
    // Read response
    auto read_result = hw_client.read(scoped.handle(), 256);
    
    if (!read_result) {
        std::cerr << "Read failed\n";
        return 1;
    }
    
    std::cout << "Read " << read_result->size() << " bytes\n";
    
    // scoped destructor automatically releases hardware
    return 0;
}
```

## Security Model

1. **Main application runs as root** - exclusive hardware access
2. **Registered apps run as unprivileged users** - no direct hardware access
3. **All hardware I/O proxied** through main app via RPC
4. **Hardware requirements declared** in app manifest, enforced by main app
5. **Resource arbitration** prevents conflicts and ensures fairness

## Thread Safety

The hardware client is **not thread-safe**. If using from multiple threads:
- Create one client per thread, or
- Synchronize access with mutexes

## Limitations

- Maximum I/O buffer size: 1 MB
- Maximum resource path: 255 characters
- Hardware access requires prior RPC connection to main app
- Timeout precision depends on main app's scheduling

## Dependencies

- `protoflow-rpc`: Base RPC protocol and transport abstraction
- C++23 compiler with `std::expected` support

## See Also

- [ARCHITECTURE.md](../../../ARCHITECTURE.md) - System architecture
- [library-rpc.md](../../../docs/library-rpc.md) - RPC protocol details
- Main application Hardware Arbitration Service implementation
