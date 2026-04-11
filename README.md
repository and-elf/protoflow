# Protoflow

A modern C++23 framework for building service-oriented applications with strict isolation, message-passing, and protocol-driven logic.

## Overview

Protoflow is designed for embedded/Linux-friendly, single-process, single-threaded runtime with deterministic execution. Applications are isolated services that communicate exclusively through message passing, with all behavior validated by finite state machines.

**See**: [ARCHITECTURE.md](ARCHITECTURE.md) for detailed design documentation.

## Features

- ✅ **Service isolation** - no direct calls, no peer awareness
- ✅ **Message-first runtime** - router + scheduler
- ✅ **Protocol-driven logic** - FSMs with compile-time validation
- ✅ **Observable behavior** - misbehavior logged by construction
- ✅ **Policy injection** - logging/tracing/metrics via sinks
- ✅ **Transport-agnostic** - TCP, MQTT, UNIX sockets
- ✅ **Deterministic execution** - one message per service per cycle
- ✅ **Hardware arbitration** - centralized access control

## Quick Start

### Prerequisites

- C++23 compiler (GCC 13+, Clang 16+)
- CMake 3.25+
- Linux (tested on Ubuntu 22.04+)

### Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
cd build && ctest

# Generate packages
cpack
```

### Build Options

```bash
# Enable sanitizers
cmake -B build -DENABLE_SANITIZER_ADDRESS=ON

# Enable code coverage
cmake -B build -DENABLE_COVERAGE=ON

# Build with all warnings as errors
cmake -B build -DENABLE_WARNINGS_AS_ERRORS=ON
```

## Project Structure

```
protoflow/
├── ARCHITECTURE.md                       # System design and architecture
├── cmake/                                # CMake modules and macros
├── docs/                                 # Library documentation
├── libs/                                 # Protoflow libraries
│   ├── protoflow-runtime/                # Core runtime, router, scheduler
│   ├── protoflow-messaging/              # Message passing infrastructure
│   ├── protoflow-logging/                # Policy-injected logging with sinks
│   ├── protoflow-service/                # Service base class and mailbox
│   ├── protoflow-config/                 # Configuration management
│   ├── protoflow-fsm/                    # Compile-time finite state machines
│   ├── protoflow-fsm-table/              # FSM transition table definitions
│   ├── protoflow-rpc/                    # RPC framework core
│   ├── protoflow-rpc-client/             # RPC client implementation
│   ├── protoflow-rpc-server/             # RPC server implementation
│   ├── protoflow-rpc-service/            # RPC service layer
│   ├── protoflow-rpc-protocol/           # RPC protocol definitions
│   ├── protoflow-transport-tcp/          # TCP transport adapter
│   ├── protoflow-transport-mqtt/         # MQTT transport adapter
│   ├── protoflow-transport-unix/         # Unix socket transport adapter
│   ├── protoflow-http/                   # HTTP server and client
│   ├── protoflow-html-fragment/          # Compile-time HTML fragment generation
│   ├── protoflow-app-registration-client/   # App registration client
│   ├── protoflow-app-registration-protocol/ # App registration protocol definitions
│   ├── protoflow-hw-client/              # Hardware access client
│   └── protoflow-hw-protocol/            # Hardware protocol definitions
├── apps/                                 # Applications
│   └── main-app/                         # Main application (runs as root)
└── tests/                                # Test suites
    ├── unit/
    ├── integration/
    └── system/
```

## Libraries

### Core Runtime

- **protoflow-runtime** — Core single-threaded runtime, message router, and deterministic scheduler. The heart of the framework; all services run inside it.
- **protoflow-messaging** — Message passing infrastructure. Defines the envelope format, mailboxes, and delivery guarantees used across the framework.
- **protoflow-service** — Service base class and per-service mailbox. Every application-level service inherits from this and registers with the runtime.
- **protoflow-config** — Configuration management. Loads and validates runtime configuration consumed by the runtime and individual services.
- **protoflow-logging** — Policy-injected structured logging with pluggable sinks (file, stdout, network). Logging behavior is injected at startup, not hard-coded.

### Finite State Machines

- **protoflow-fsm** — Lightweight compile-time FSM primitives. States, events, and transitions are validated at compile time with no runtime overhead.
- **protoflow-fsm-table** — FSM transition table definitions and helpers. Provides higher-level table-driven FSM construction on top of `protoflow-fsm`.

### RPC Layer

- **protoflow-rpc** — RPC framework core. Defines the request/response contract and ties together the client, server, and protocol libraries.
- **protoflow-rpc-protocol** — Wire protocol definitions shared by client and server (message framing, serialization format).
- **protoflow-rpc-client** — RPC client implementation. Used by unprivileged apps to call services hosted in the main app.
- **protoflow-rpc-server** — RPC server implementation. Hosts callable services and dispatches incoming requests to registered handlers.
- **protoflow-rpc-service** — RPC service layer. Bridges the RPC server with the protoflow service model so RPC handlers run inside the runtime.

### Transports

- **protoflow-transport-tcp** — TCP transport adapter. Default transport for inter-process communication between registered apps and the main app.
- **protoflow-transport-mqtt** — MQTT transport adapter. Optional pub/sub transport for IoT or broker-based deployments.
- **protoflow-transport-unix** — Unix domain socket transport adapter. Low-latency IPC for co-located processes on the same host.

### HTTP and UI

- **protoflow-http** — Embedded HTTP server and client. Used by the main app to serve aggregated UI and expose JSON state endpoints.
- **protoflow-html-fragment** — Compile-time HTML fragment generation. Apps produce typed HTML fragments that are aggregated by the main app's HTTP service.

### App Registration

- **protoflow-app-registration-protocol** — Protocol definitions for the app registration handshake (message types, sequence, fields).
- **protoflow-app-registration-client** — Client-side app registration library. Registered apps use this to announce themselves to the main app over TCP.

### Hardware Arbitration

- **protoflow-hw-protocol** — Hardware protocol definitions. Describes hardware resource request/response messages shared by client and the arbitration service.
- **protoflow-hw-client** — Hardware access client. Unprivileged apps use this to request hardware I/O via the main app's `HardwareArbitrationService`, never touching hardware directly.

See [docs/](docs/) for detailed per-library documentation.

## Main Application

**Status**: ✅ **Implemented** (see [apps/main-app/](apps/main-app/))

The main application (`protoflow-main-app`):
- Runs as **root** with exclusive hardware access
- Listens on TCP port for app registration
- Serves HTTP for UI aggregation (default port 8080)
- Provides hardware arbitration service
- Aggregates JSON state from all apps
- Implements three core services:
  - **ApplicationRegistrationService** — App lifecycle and UI aggregation
  - **HardwareArbitrationService** — Hardware I/O proxy
  - **HTTPService** — HTTP endpoints and content negotiation

**Run**:
```bash
# Build
cmake --build build --target protoflow-main-app

# Run (requires root for hardware access)
sudo ./build/apps/main-app/protoflow-main-app

# Custom configuration
sudo ./protoflow-main-app --port 9000 --hw-config /path/to/hardware.conf

# Help
./protoflow-main-app --help
```

**Documentation**:
- [apps/main-app/README.md](apps/main-app/README.md) — Usage and configuration
- [apps/main-app/IMPLEMENTATION.md](apps/main-app/IMPLEMENTATION.md) — Implementation details
- [apps/main-app/ARCHITECTURE-DIAGRAM.txt](apps/main-app/ARCHITECTURE-DIAGRAM.txt) — Visual architecture
- [apps/main-app/README.md](apps/main-app/README.md) - Usage and configuration
- [apps/main-app/IMPLEMENTATION.md](apps/main-app/IMPLEMENTATION.md) - Implementation details
- [apps/main-app/ARCHITECTURE-DIAGRAM.txt](apps/main-app/ARCHITECTURE-DIAGRAM.txt) - Visual architecture

Main app:
- Runs as **root** with exclusive hardware access
- Listens on TCP port for app registration
- Serves HTTP for UI aggregation
- Provides hardware arbitration service
- Aggregates JSON state from all apps

Registered applications:
- Run as **unprivileged users**
- No direct hardware access
- Communicate via RPC over TCP
- Supply HTML fragments for UI
- Request hardware via main app

## Development

### Creating an Application

```cmake
protoflow_add_app(
    NAME my-app
    VERSION 1.0.0
    DESCRIPTION "My application"
    SOURCES src/main.cpp
    HW_REQUIREMENTS /dev/ttyUSB0
    LINK_LIBRARIES
        protoflow::runtime
        protoflow::rpc
    LOGGING_CONFIG logging.ini
)
```

See [docs/cmake-macros.md](docs/cmake-macros.md) for details.

### Creating a Library

```cmake
protoflow_add_library(
    NAME my-lib
    VERSION 1.0.0
    TYPE STATIC
    SOURCES src/lib.cpp
    PUBLIC_HEADERS include/my_lib.hpp
)
```

## Testing

```bash
# Run all tests
cd build && ctest

# Run specific test suite
ctest -R unit

# Verbose output
ctest -V

# With coverage
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
cd build && ctest
# View coverage report in coverage/index.html
```

## License

[To be determined]

## Contributing

[To be determined]

## 🧩 Components

### AppBase
[Paste Agent's description of AppBase here]

### Libraries
- **lib-name**: [Agent description]
- **lib-name**: [Agent description]

### Getting Started (Skeleton App)
You can find a reference implementation in `apps/skeleton-app`. Use this as a base for building new services...

---

## 🏗 Project Architecture & Components

### AppBase (`protoflow::runtime::AppBase`)
`AppBase` is the core base class for all Protoflow applications. It manages the boilerplate of application lifecycles, including:
* **Lifecycle & Signal Handling**: Graceful shutdown via `SIGINT`/`SIGTERM`.
* **Deterministic Execution**: A main loop that executes `.cycle()` at configurable tick rates.
* **Message Routing**: Acts as the central hub for isolated services and RPC transports.
* **Component Management**: Initializes logging, service lists, and RPC transport clients.

### 📚 Core Libraries (`/libs`)
Protoflow follows a strictly isolated, message-first architecture:
* **protoflow-runtime**: The "heart" containing the message router and service scheduler.
* **protoflow-service**: Provides base `Service` classes and thread-safe `Mailbox<T>`.
* **protoflow-messaging**: Abstractions for inbound/outbound message queues.
* **protoflow-fsm**: Compile-time Finite State Machine library for protocol-driven behavior.
* **protoflow-rpc**: Transport-agnostic client/server model for inter-app communication.
* **protoflow-logging**: Structured logging system for deep observability.

### 🚀 Skeleton App (`/apps/skeleton-app`)
The `skeleton-app` serves as the official template and reference implementation.
* **Purpose**: Demonstrates app registration flows, hardware access (`protoflow-hw-client`), and RPC server loops.
* **Usage**: Run it via CLI (e.g., `protoflow-skeleton-app -s <ip> -p <port>`) or use it as a template by duplicating the directory for new service development.

---