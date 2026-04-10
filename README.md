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
├── ARCHITECTURE.md         # System design and architecture
├── cmake/                  # CMake modules and macros
├── docs/                   # Library documentation
├── libs/                   # Protoflow libraries
│   ├── protoflow-runtime/
│   ├── protoflow-messaging/
│   ├── protoflow-logging/
│   ├── protoflow-service/
│   ├── protoflow-fsm/
│   ├── protoflow-rpc/
│   ├── protoflow-transport-tcp/
│   ├── protoflow-transport-mqtt/
│   └── protoflow-html-fragment/
├── apps/                   # Applications
│   └── main-app/          # Main application (runs as root)
└── tests/                  # Test suites
    ├── unit/
    ├── integration/
    └── system/
```

## Libraries

- **protoflow-runtime**: Core runtime, message router, and scheduler
- **protoflow-messaging**: Message passing infrastructure
- **protoflow-logging**: Policy-injected logging with sinks
- **protoflow-service**: Service base class and mailbox infrastructure
- **protoflow-fsm**: Compile-time validated finite state machines
- **protoflow-rpc**: TCP-based RPC framework
- **protoflow-transport-tcp**: TCP transport adapter
- **protoflow-transport-mqtt**: MQTT transport adapter
- **protoflow-transport-unix**: Unix transport adapter
- **protoflow-html-fragment**: Compile-time HTML generation

See [docs/](docs/) for detailed library documentation.

## Main Application

**Status**: ✅ **Implemented** (see [apps/main-app/](apps/main-app/))

The main application (`protoflow-main-app`):
- Runs as **root** with exclusive hardware access
- Listens on TCP port for app registration
- Serves HTTP for UI aggregation (default port 8080)
- Provides hardware arbitration service
- Aggregates JSON state from all apps
- Implements three core services:
  - **ApplicationRegistrationService** - App lifecycle and UI aggregation
  - **HardwareArbitrationService** - Hardware I/O proxy
  - **HTTPService** - HTTP endpoints and content negotiation

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
