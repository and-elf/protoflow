# protoflow-app-registration-client

Service library for registering applications with a central main server via RPC protocol over TCP.

## Quick Start

```cpp
#include <protoflow/app_registration_client.hpp>

using namespace protoflow;

// Configure the client
app_registration_client::Config config{
    .app_name = "my-app",
    .version = "1.0.0",
    .endpoints = {"/api/data", "/ui/dashboard"},
    .server_address = "localhost",
    .server_port = 8080
};

// Create and start the service
auto client = std::make_unique<arc::AppRegistrationClient>(config);
client->start();

// In your main loop
while (running) {
    client->poll();
    // ... other work
}

client->stop();
```

## Features

- ✓ Service-based architecture (inherits from `Service`)
- ✓ FSM-driven state management with compile-time validation
- ✓ Automatic reconnection with configurable retry policy
- ✓ Heartbeat mechanism to maintain registration
- ✓ Observable behavior via FSM logging sinks
- ✓ Thread-safe for multi-threaded runtimes
- ✓ Configuration-driven API (no hard-coded values)

## Documentation

See [docs/library-app-registration-client.md](../../docs/library-app-registration-client.md) for comprehensive documentation, usage examples, and API reference.

## Dependencies

- `protoflow::service` - Service base class
- `protoflow::fsm` - Finite state machine
- `protoflow::rpc` - RPC protocol
- `protoflow::transport-tcp` - TCP transport
- `protoflow::logging` - Logging framework

## Build

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

## License

Part of the Protoflow project.
