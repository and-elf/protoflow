# Protoflow Main Application

The main application that runs as root and provides hardware arbitration and app registration services.

## Overview

This is the central component of the Protoflow system. It:
- Runs as **root** with exclusive hardware access
- Provides **hardware arbitration** for registered applications
- Manages **app registration** and lifecycle
- Serves **HTTP endpoints** for UI aggregation and JSON API
- Routes messages between services using deterministic scheduling

## Architecture

```
main-app/
├── include/
│   ├── runtime.hpp                 # Main runtime orchestrator
│   ├── services/                    # Service implementations
│   │   ├── app_registration_service.hpp
│   │   ├── hardware_arbitration_service.hpp
│   │   └── http_service.hpp
│   └── protoflow/                   # Convenience headers
│       ├── service.hpp
│       ├── messaging.hpp
│       ├── logging.hpp
│       ├── rpc.hpp
│       ├── html.hpp
│       └── http.hpp
├── src/
│   ├── main.cpp                     # CLI entry point
│   ├── runtime.cpp                  # Runtime implementation
│   └── services/                    # Service implementations
│       ├── app_registration_service.cpp
│       ├── hardware_arbitration_service.cpp
│       └── http_service.cpp
├── config/
│   └── hardware.conf                # Hardware resource definitions
└── CMakeLists.txt
```

## Components

### Runtime

The `Runtime` class orchestrates all services and implements:
- **Service management**: Creates and manages all services
- **Message routing**: Routes messages between services  
- **Deterministic scheduling**: One message per service per cycle (100Hz)
- **Signal handling**: Graceful shutdown on SIGINT/SIGTERM

### Services

#### ApplicationRegistrationService
- Manages registered application lifecycle
- Tracks app keepalives and health
- Aggregates UI fragments from apps via RPC
- Provides JSON state API
- Renders navigation bar for HTTP service

#### HardwareArbitrationService
- Loads hardware configuration from `hardware.conf`
- Manages exclusive/shared access to hardware resources
- Proxies all hardware I/O operations (read, write, ioctl)
- Enforces timeouts and access policies
- Platform-specific device handling (Linux)

#### HTTPService
- Serves HTTP endpoints on configured port
- Content negotiation (HTML vs JSON)
- Integrates with ApplicationRegistrationService for UI
- Serves home page with navigation
- Proxies requests to registered apps
- Provides `/api/state` endpoint

## Usage

### Build

```bash
cmake -B build
cmake --build build
```

### Run

```bash
# Default (requires root for hardware access)
sudo ./build/apps/main-app/protoflow-main-app

# Custom configuration
sudo ./protoflow-main-app --address 127.0.0.1 --port 9000 --hw-config /path/to/hw.conf

# Help
./protoflow-main-app --help
```

### Command Line Options

```
-h, --help                   Show help message
-v, --version                Show version information
-a, --address ADDRESS        Listen address (default: 0.0.0.0)
-p, --port PORT              Listen port (default: 8080)
-c, --hw-config PATH         Hardware config file (default: /etc/protoflow/hardware.conf)
-l, --log-config PATH        Logging config file (default: /etc/protoflow/logging.conf)
--no-http                    Disable HTTP service
--no-registration            Disable app registration service
--no-hardware                Disable hardware arbitration service
```

## Configuration

### Hardware Configuration (`hardware.conf`)

INI-style configuration defining hardware resources:

```ini
[serial.usb0]
device = /dev/ttyUSB0
mode = exclusive
timeout = 30s

[i2c.bus1]
device = /dev/i2c-1
mode = shared
max_clients = 4

[gpio.pin23]
pin = 23
mode = exclusive
direction = output
```

**Fields**:
- `device`: Device path (e.g., `/dev/ttyUSB0`)
- `mode`: `exclusive` or `shared`
- `max_clients`: Maximum simultaneous clients (shared mode only)
- `timeout`: Access timeout (e.g., `30s`)

## Service Communication

Services communicate via message passing through the runtime:

1. Service reads one message from inbound queue
2. Service processes message
3. Service generates outbound messages
4. Runtime routes outbound messages to destinations

**Deterministic execution**: One message per service per cycle ensures predictable behavior.

## HTTP Endpoints

### Built-in Endpoints

- `GET /` - Home page with navigation to all registered apps
- `GET /api/state` - JSON state aggregation from all apps
- `GET /app/{name}/{endpoint}` - Proxy to registered app endpoint

### Response Format

**HTML** (when `Accept: text/html`):
- Full page with navigation bar
- App fragments embedded

**JSON** (when `Accept: application/json`):
- Raw JSON response from app or state aggregation

## Hardware Arbitration Flow

```
1. App → Main: REQUEST_HW_ACCESS {resource, mode, timeout}
2. Main: Check if resource available
3. Main: Open device file (platform-specific)
4. Main → App: HW_ACCESS_GRANTED {handle, capabilities}
5. App → Main: HW_READ/HW_WRITE/HW_IOCTL {handle, ...}
6. Main: Perform actual I/O on device
7. Main → App: Response with data/result
8. App → Main: HW_RELEASE {handle}
9. Main: Close device file
10. Main → App: HW_RELEASE_ACK
```

## Dependencies

- `protoflow::runtime` - Core runtime
- `protoflow::messaging` - Message passing
- `protoflow::logging` - Logging infrastructure
- `protoflow::service` - Service base class
- `protoflow::fsm` - State machine framework
- `protoflow::rpc` - RPC protocol
- `protoflow::hw-client` - Hardware client protocol definitions
- `protoflow::transport-tcp` - TCP transport
- `protoflow::html-fragment` - HTML generation
- `protoflow::http` - HTTP service library
- `nlohmann_json` - JSON serialization

## Security Model

1. **Main app runs as root** - required for hardware access
2. **Registered apps unprivileged** - cannot access hardware directly
3. **All hardware I/O proxied** - enforced through RPC
4. **Resource arbitration** - prevents conflicts
5. **Timeout enforcement** - prevents resource hogging

## Development

### Adding a New Service

1. Create service header in `include/services/`
2. Implement service in `src/services/`
3. Inherit from `protoflow::service::Service`
4. Override `start()`, `stop()`, `poll()`, `handle()`
5. Add to `Runtime::initialize()` in `runtime.cpp`
6. Add source file to `CMakeLists.txt`

### Testing

The main app can be tested with mock registered applications or standalone:

```bash
# Run with all services
sudo ./protoflow-main-app

# Run with only HTTP (no hardware)
./protoflow-main-app --no-hardware

# Run with only hardware arbitration (no HTTP)
sudo ./protoflow-main-app --no-http --no-registration
```

## Future Enhancements

- [ ] Actual HTTP server integration (e.g., cpp-httplib, Boost.Beast)
- [ ] Full RPC client connection management
- [ ] Persistent state storage
- [ ] Metrics and monitoring
- [ ] TLS/PKI support for RPC connections
- [ ] WebSocket support for real-time updates
- [ ] Plugin system for dynamic service loading
