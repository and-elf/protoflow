# Protoflow Architecture

## Overview

A modern C++23 framework for building service-oriented applications with strict isolation, message-passing, and protocol-driven logic.

**Target**: Embedded/Linux-friendly, single-process, single-threaded runtime with deterministic execution.

---

## Core Principles

### Service Isolation
- **Service-oriented, not microservices** — all services in a single process
- **Single-threaded** execution model
- **Strict isolation** — services do not know about each other
- **No direct calls**, no destinations, no peer awareness

### Message-First Runtime
- Services read from **inbound queue**
- Services write to **outbound queue**
- Runtime acts as **router + scheduler**
- **One message per service per cycle** (deterministic execution)
- **Deadline-driven polling**

### Protocol-Driven Logic
- **FSMs describe valid behavior**
- **Misbehavior is observable and logged** by construction
- Transport-agnostic design

### Policy Injection
- Logging, tracing, metrics injected via **sinks**
- **No global state**
- **No hidden side effects**

### Transport Agnostic
- TCP, UNIX sockets, MQTT, peripherals
- **Transport services are adapters**, not logic

---

## Libraries

### Core Libraries

#### `protoflow-runtime`
Core runtime system providing:
- Message router
- Service scheduler
- Queue management
- Deterministic execution guarantees

#### `protoflow-messaging`
Message passing infrastructure:
- Inbound/outbound queue abstractions
- Message routing
- Service isolation enforcement

#### `protoflow-logging`
Logging infrastructure:
- Policy-injected logging sinks
- Observable behavior tracking
- FSM state logging

### Protocol & State Management

#### `protoflow-fsm`
Finite State Machine library:
- Protocol-driven behavior definitions
- Compile-time verification of FSM completeness
- Required `otherwise()` clause enforced at compile-time
- Observable transitions via pluggable sinks
- State validation and misbehavior detection

**See**: [docs/library-fsm.md](docs/library-fsm.md) for detailed API and implementation

#### `protoflow-rpc`
RPC framework for inter-app communication:
- Wire protocol definitions
- Client/server base classes
- Handshake and framing enforcement
- Version negotiation
- Transport-agnostic design

**See**: [docs/library-rpc.md](docs/library-rpc.md) for detailed protocol and API

### Transport Adapters

#### `protoflow-transport-tcp`
TCP transport adapter for external communication

#### `protoflow-transport-mqtt`
MQTT transport adapter for pub/sub messaging

### Presentation Layer

#### `protoflow-html-fragment`
Compile-time HTML generation:
- Type-safe HTML construction
- Constexpr tag building
- Fragment composition
- Compile-time validation

**See**: [docs/library-html-fragment.md](docs/library-html-fragment.md) for detailed API and examples

---

## System-of-Systems Architecture

### Main Application
- **Listens on specific TCP port**
- **Runs as root** - exclusive hardware access
- Apps register themselves via RPC
- Hosts HTTP service for UI aggregation
- Pulls HTML fragments from registered apps
- Builds composite pages with navigation
- **Provides hardware arbitration service**
- **Proxies all hardware I/O for registered apps**

### Registered Applications
- **Connect via TCP-based RPC**
- **Run as unprivileged users** - no direct hardware access
- Supply their own HTML fragments
- Respond to RPC calls
- Maintain connection health
- **Request hardware access via RPC**
- **Declare hardware requirements in manifest**

### Communication Flow
```
[App 1] ──RPC/TCP──┐
[App 2] ──RPC/TCP──┼──> [Main App] <──HTTP/JSON── [Browser/Client]
[App N] ──RPC/TCP──┘
```

**Main App Responsibilities**:
- HTTP/HTML: Aggregate fragments, render pages with navigation
- HTTP/JSON: Proxy to app endpoints, aggregate state from all apps

---

## Services

### ApplicationRegistrationService
**Location**: Main app  
**Purpose**: App lifecycle management, UI aggregation, and state API

**Responsibilities**:
- Uses `protoflow-fsm` to register apps
- Maintains keep-alive state/health monitoring
- Proxies HTML requests over `protoflow-rpc`
- Renders `protoflow-html-fragments` from registered apps
- Serves JSON API endpoints for state queries
- Aggregates state from all registered apps

**App Registration Format** (JSON):
```json
{
  "name": "app-name",
  "endpoints": ["/path1", "/path2"],
  "... additional metadata ..."
}
```

**JSON API Endpoints**:
- Each registered app endpoint responds to JSON requests
- Main app aggregates current state from all registered apps
- Apps implement state query via RPC

### HardwareArbitrationService
**Location**: Main app  
**Purpose**: Centralized hardware resource management and arbitration

**Hardware Access Model**:
- **Main app runs as root** - exclusive hardware access
- **Registered apps run as unprivileged users** - NO direct hardware access
- **All hardware operations** go through HardwareArbitrationService via RPC
- **Hardware requirements** declared in app configuration, enforced by main app

**Responsibilities**:
- Manages exclusive/shared access to hardware resources
- Arbitrates hardware requests from registered apps
- Enforces hardware access policies via `protoflow-fsm`
- Communicates with apps via `protoflow-rpc` over TCP
- Tracks hardware resource state and ownership
- Handles resource acquisition, release, and timeouts
- **Performs all actual hardware I/O operations**
- Returns results to requesting apps

**Technology Stack**:
- `protoflow-service` - Service framework
- `protoflow-transport-tcp` - TCP communication
- `protoflow-rpc` - RPC protocol for hardware requests
- `protoflow-fsm` - State machines for resource allocation policies

**Configuration**:
- Hardware resource definitions (config file)
- Access policies (exclusive vs. shared)
- Timeout settings
- Resource capabilities and constraints

**Use Cases**:
- Serial port arbitration (e.g., `/dev/ttyUSB0`)
- GPIO pin allocation
- I2C/SPI bus access control
- Camera or sensor exclusivity
- Peripheral device locking

**RPC Protocol**:
```cpp
// App requests hardware access
app -> main: REQUEST_HW_ACCESS {resource: "/dev/ttyUSB0", mode: "exclusive"}
main -> app: HW_ACCESS_GRANTED {handle: 42, timeout: 30s}

// App performs I/O via main app
app -> main: HW_WRITE {handle: 42, data: [...]}
main -> app: HW_WRITE_ACK {bytes_written: 10}

app -> main: HW_READ {handle: 42, count: 256}
main -> app: HW_READ_RESPONSE {data: [...]}

// App releases hardware
app -> main: HW_RELEASE {handle: 42}
main -> app: HW_RELEASE_ACK
```

### HTTP Request Flow
1. Browser requests main app
2. Main app uses registration info to render navigation bar
3. Links point to registered app endpoints
4. Main app proxies fragment rendering via RPC
5. Composite page assembled and returned

### JSON API Flow

**Registered App Endpoints**:
```
GET /api/app1/endpoint1
→ Main app proxies to registered app via RPC
→ App returns JSON state
→ Main app forwards response
```

**Main App State Aggregation**:
```
GET /api/state
→ Main app queries all registered apps via RPC
→ Each app returns current state as JSON
→ Main app aggregates and returns:
```

```json
{
  "timestamp": "2026-02-09T12:34:56Z",
  "apps": [
    {
      "name": "app1",
      "status": "alive",
      "state": { /* app-specific state */ }
    },
    {
      "name": "app2",
      "status": "alive",
      "state": { /* app-specific state */ }
    }
  ]
}
```

**App-Specific State Example**:
```json
{
  "name": "sensor-monitor",
  "status": "alive",
  "state": {
    "sensors": [
      {"id": "temp1", "value": 23.5, "unit": "C"},
      {"id": "hum1", "value": 45.2, "unit": "%"}
    ],
    "last_reading": "2026-02-09T12:34:50Z"
  }
}
```

---

## Implementation Roadmap

### Phase 0: Build Infrastructure
- [ ] CMake project structure
- [ ] Compiler warnings and flags configuration
- [ ] Sanitizer integration
- [ ] Code coverage setup
- [ ] Static analyzer integration (clang-tidy, cppcheck)
- [ ] gtest integration
- [ ] `protoflow_add_library` macro implementation
- [ ] `protoflow_add_app` macro implementation
- [ ] Debian packaging integration (CPack)

### Phase 1: Core Runtime
- [ ] `protoflow-runtime` - message router and scheduler
- [ ] `protoflow-messaging` - queue abstractions
- [ ] `protoflow-logging` - logging infrastructure

### Phase 2: Protocol & State
- [ ] `protoflow-fsm` - state machine framework
- [ ] `protoflow-rpc` - RPC protocol implementation

### Phase 3: Transport Adapters
- [ ] `protoflow-transport-tcp` - TCP adapter
- [ ] `protoflow-transport-mqtt` - MQTT adapter

### Phase 4: Presentation
- [ ] `protoflow-html-fragment` - HTML generation library

### Phase 5: Main Application
- [ ] ApplicationRegistrationService
- [ ] HardwareArbitrationService
- [ ] HTTP server integration
- [ ] Fragment aggregation and rendering
- [ ] JSON API endpoints
- [ ] State aggregation from registered apps

### Phase 6: Testing Infrastructure
- [ ] Unit test suite (gtest)
- [ ] Integration test suite (gtest)
- [ ] Full-system test runner
- [ ] Message tracking and verification tools
- [ ] CI/CD pipeline integration

---

## Build System

### CMake-Based Build
Follows best practices from [cpp-best-practices/cmake_template](https://github.com/cpp-best-practices/cmake_template/)

### Key Features

#### Compiler Flags
- **Warning levels**: `-Wall -Wextra -Wpedantic -Werror`
- **Sanitizers**: Address, UndefinedBehavior, Thread, Memory (configurable)
- **Optimization**: Debug (`-O0 -g3`), Release (`-O3`), RelWithDebInfo
- **C++23 standard**: `-std=c++23`
- **Platform-specific flags** for embedded/Linux targets

#### Code Coverage
- **gcov/lcov** integration for coverage reporting
- **Per-library coverage** tracking
- **HTML reports** generated automatically
- **CI integration** with coverage thresholds
- **Exclude patterns** for generated code and tests

#### Project Structure
```
protoflow/
├── CMakeLists.txt              # Root project
├── cmake/
│   ├── CompilerWarnings.cmake  # Warning configurations
│   ├── Sanitizers.cmake        # Sanitizer options
│   ├── CodeCoverage.cmake      # Coverage setup
│   └── StaticAnalyzers.cmake   # clang-tidy, cppcheck
├── libs/
│   ├── protoflow-runtime/
│   ├── protoflow-messaging/
│   ├── protoflow-logging/
│   ├── protoflow-fsm/
│   ├── protoflow-rpc/
│   ├── protoflow-transport-tcp/
│   ├── protoflow-transport-mqtt/
│   └── protoflow-html-fragment/
├── apps/
│   └── main-app/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── system/
└── docs/
```

#### CMake Options
```cmake
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
option(ENABLE_SANITIZER_UB "Enable UB sanitizer" OFF)
option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
option(ENABLE_TESTING "Enable test builds" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
```

#### Protoflow Build Macros

Custom CMake macros for building applications and libraries with integrated Debian packaging:

**`protoflow_add_app`**: Build applications with systemd service generation
- App name, version, description
- Hardware requirements (peripherals: `/dev/ttyUSB0`, GPIO, I2C, etc.)
- PKI path for security (used by RPC, MQTT, etc.)
- Runtime user for isolation
- Library dependencies
- Debian package generation with postinst scripts
- Systemd service with hardware access control

**`protoflow_add_library`**: Build libraries with proper CMake targets
- Library name, version, description
- Type (STATIC, SHARED, INTERFACE)
- Public/private headers and dependencies
- CMake config file generation
- pkg-config support
- Development and runtime Debian packages

**See**: [docs/cmake-macros.md](docs/cmake-macros.md) for detailed macro API and examples

#### Library Targets
- Each library is a **modern CMake target**
- **Interface libraries** for header-only components
- **STATIC libraries** for embedded deployment
- **Proper dependency management** via `target_link_libraries`
- **Include directories** exported via `target_include_directories`

#### Testing Integration
- **CTest** for test discovery and execution
- **gtest** fetched via FetchContent or find_package
- **Test executables** linked against library targets
- **Parallel test execution** support

---

## Testing Strategy

### Framework
**Google Test (gtest)** for all testing levels

### Unit Tests
- Test individual components in isolation
- Mock message queues and runtime interfaces
- Validate FSM state transitions
- Test RPC protocol encoding/decoding
- Verify HTML fragment generation

### Integration Tests
- Test service interactions via message passing
- Validate RPC client/server handshakes
- Test transport adapter behavior
- Verify FSM + logging integration
- Test fragment composition and rendering

### Full-System Tests
**Custom test runner** for end-to-end validation:
- Launches apps in controlled environment
- Sends/receives messages to/from services
- Tracks message flow through the runtime
- Validates deterministic execution
- Monitors FSM state progressions
- Captures and verifies logging output

**Test Runner Capabilities**:
- Message injection at specific points
- Message interception and verification
- Timing control (deadline simulation)
- Hardware resource mocking
- RPC conversation recording/replay
- Multi-app orchestration

---

## Design Guarantees

✓ **Deterministic execution** - one message per service per cycle  
✓ **Observable behavior** - FSM violations logged automatically  
✓ **Transport independence** - logic decoupled from transport  
✓ **Zero hidden state** - all state explicit in FSM  
✓ **Zero direct coupling** - services communicate only via messages  
✓ **Compile-time safety** - extensive use of C++23 concepts  
✓ **Embedded-friendly** - single process, predictable resource usage  

---

## Next Steps

1. Define message queue interface (`protoflow-messaging`)
2. Implement runtime scheduler (`protoflow-runtime`)
3. Design FSM DSL (`protoflow-fsm`)
4. Build RPC wire protocol (`protoflow-rpc`)
5. Create HTML fragment DSL (`protoflow-html-fragment`)
6. Implement transport adapters
7. Build main application and registration service
