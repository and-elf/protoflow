# protoflow-transport-unix

Unix domain socket transport adapter for local inter-process communication.

## Overview

The `protoflow-transport-unix` library provides Unix domain socket transport functionality for the protoflow framework. It enables fast, reliable local IPC (Inter-Process Communication) between processes on the same host, with support for both filesystem-based and abstract namespace sockets.

## Features

- **Unix Domain Sockets**: AF_UNIX sockets for local IPC
- **Filesystem and Abstract Sockets**: Support both traditional filesystem sockets and Linux abstract namespace
- **Credential Passing**: Send and receive process credentials (PID, UID, GID) on Linux
- **Non-blocking Connect**: Connection attempts with configurable timeouts
- **Socket Permissions**: Configure filesystem socket permissions
- **Move-only Semantics**: Proper resource ownership following C++23 best practices
- **Error Handling**: std::expected-based error handling for robust operation
- **Zero-copy I/O**: std::span-based interface for efficient data transfer

## API

### Client API

```cpp
#include <protoflow/transport/unix.hpp>

using namespace protoflow::transport::unix;

// Create client with configuration
unix_config config{
    .socket_path = "/tmp/app.sock",
    .connect_timeout_ms = 5000,
    .abstract_namespace = false
};

unix_client client(config);

// Connect to server
if (auto result = client.connect(); !result) {
    // Handle error
    std::cerr << result.error().to_string() << '\n';
}

// Send data
std::vector<std::byte> data = {...};
if (auto sent = client.send(data); sent) {
    std::cout << "Sent " << *sent << " bytes\n";
}

// Receive data
if (auto received = client.receive(1024); received) {
    // Process received data
    process_data(*received);
}

// Send credentials (Linux only)
client.send_credentials();

// Disconnect
client.disconnect();
```

### Server API

```cpp
#include <protoflow/transport/unix.hpp>

using namespace protoflow::transport::unix;

// Create server
unix_config config{
    .socket_path = "/tmp/app.sock",
    .socket_permissions = 0660  // Owner and group read/write
};

unix_server server(config);

// Start listening
if (auto result = server.listen(); !result) {
    std::cerr << result.error().to_string() << '\n';
    return;
}

// Accept client connections
while (server.is_listening()) {
    auto client = server.accept();
    if (!client) {
        continue;
    }
    
    // Handle client in thread or async
    std::thread([c = std::move(*client)]() mutable {
        handle_client(std::move(c));
    }).detach();
}

server.stop();
```

### Configuration

```cpp
struct unix_config {
    std::string socket_path{"/tmp/protoflow.sock"};  // Socket path
    uint32_t connect_timeout_ms{5000};               // Connection timeout
    uint32_t read_timeout_ms{1000};                  // Read timeout
    uint32_t write_timeout_ms{1000};                 // Write timeout
    size_t buffer_size{8192};                        // Default buffer size
    mode_t socket_permissions{0600};                 // Owner only by default
    bool abstract_namespace{false};                  // Use abstract namespace (Linux)
};
```

## Filesystem vs Abstract Sockets

### Filesystem Sockets

Traditional Unix sockets that appear in the filesystem:

```cpp
unix_config config{
    .socket_path = "/tmp/app.sock",
    .abstract_namespace = false
};
```

- Visible in filesystem (`ls /tmp/app.sock`)
- Subject to filesystem permissions
- Must unlink() before rebinding
- Persists until explicitly removed

### Abstract Namespace (Linux)

Linux-specific abstract namespace that doesn't use filesystem:

```cpp
unix_config config{
    .socket_path = "my_app",  // No leading /
    .abstract_namespace = true
};
```

- Not visible in filesystem
- No filesystem permissions (uses Linux socket permissions)
- Automatically cleaned up when last reference closes
- Faster (no filesystem overhead)
- Linux-specific feature

## Credential Passing

Unix sockets support passing process credentials (Linux only):

```cpp
// Client sends credentials
if (auto result = client.send_credentials(); !result) {
    std::cerr << "Failed to send credentials\n";
}

// Server receives credentials
if (auto creds = client.receive_credentials(); creds) {
    std::cout << "Client PID: " << creds->pid << '\n';
    std::cout << "Client UID: " << creds->uid << '\n';
    std::cout << "Client GID: " << creds->gid << '\n';
}
```

This is useful for:
- Authentication
- Access control
- Auditing
- Process tracking

## Error Handling

All operations return `std::expected<T, unix_error>`:

```cpp
auto result = client.connect("/tmp/app.sock");
if (!result) {
    const auto& err = result.error();
    std::cerr << "Error code: " << err.error_code << '\n';
    std::cerr << "Message: " << err.message << '\n';
    std::cerr << "Full: " << err.to_string() << '\n';
}
```

Common error scenarios:
- Connection refused (server not listening)
- Socket path doesn't exist
- Permission denied
- Timeout during connect
- Connection closed by peer

## Building

```bash
cmake -DBUILD_TRANSPORT_UNIX=ON ..
make
```

## Testing

The library includes comprehensive tests covering:
- Configuration and error types
- Client construction and lifecycle
- Server construction and lifecycle  
- Connection establishment
- Data send/receive
- Bidirectional communication
- Abstract namespace sockets
- Credential passing
- File permissions

```bash
ctest -R transport_unix
```

## Performance Considerations

Unix domain sockets provide:
- **Low latency**: No network stack overhead
- **High throughput**: Kernel-optimized IPC
- **Zero-copy**: Kernel moves data directly between processes
- **Reliability**: Stream-oriented, no message loss

Typical performance:
- Latency: 5-10 microseconds
- Throughput: 10-20 GB/sec
- Much faster than TCP loopback

Use Unix sockets when:
- Communicating between processes on same host
- Need lowest latency IPC
- Want simplicity of socket API
- Need credential passing

## Integration with RPC

Works seamlessly with protoflow-rpc:

```cpp
#include <protoflow/rpc/rpc_base.hpp>
#include <protoflow/transport/unix.hpp>

class unix_rpc_client : public rpc_client_base {
    unix_client transport_;
    
public:
    bool connect_transport(const unix_config& config) {
        return transport_.connect(config.socket_path).has_value();
    }
    
protected:
    std::expected<void, rpc_error> send_bytes(std::span<const std::byte> data) override {
        auto result = transport_.send(data);
        if (!result) {
            return std::unexpected(rpc_error{.message = result.error().message});
        }
        return {};
    }
    
    std::expected<std::vector<std::byte>, rpc_error> receive_bytes(size_t size) override {
        auto result = transport_.receive(size);
        if (!result) {
            return std::unexpected(rpc_error{.message = result.error().message});
        }
        return *result;
    }
};
```

## Platform Support

- **Linux**: Full support including abstract namespace and credential passing
- **macOS**: Filesystem sockets only (no abstract namespace or SO_PASSCRED)
- **BSD**: Filesystem sockets only

Platform-specific features are guarded with `#ifdef __linux__`.

## See Also

- [protoflow-transport-tcp](../protoflow-transport-tcp/README.md) - TCP transport
- [protoflow-rpc](../protoflow-rpc/README.md) - RPC protocol
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - Framework architecture
