# Hardware Arbitration Client Library - Implementation Summary

## Overview

Implemented `protoflow-hw-client` - a header-only C++23 library providing RPC-based hardware arbitration for unprivileged applications.

## Components Created

### Library Structure
```
libs/protoflow-hw-client/
├── CMakeLists.txt                              # Interface library definition
├── README.md                                    # Complete usage documentation
├── include/protoflow/hw/
│   ├── hw.hpp                                  # Main header
│   ├── hw_client.hpp                           # Client implementation
│   └── protocol.hpp                            # Wire protocol definitions
└── examples/
    └── hw_client_example.cpp                   # Usage examples
```

### Test Suite
```
tests/unit/libs/protoflow-hw-client/
├── CMakeLists.txt
├── test_hw_client.cpp                          # Client functionality tests
└── test_protocol.cpp                           # Protocol structure tests
```

### Documentation
```
docs/
└── library-hw-client.md                        # API reference
```

## Key Features

### Protocol Definition ([protocol.hpp](../libs/protoflow-hw-client/include/protoflow/hw/protocol.hpp))
- **Command codes** (100-131): Hardware operations extending RPC protocol
- **Binary message structures**: All fixed-size, packed, validated with static_assert
- **Access modes**: Exclusive and shared resource access
- **Capabilities**: Read, write, ioctl, seekable flags
- **Error codes**: Comprehensive error handling

### Client Implementation ([hw_client.hpp](../libs/protoflow-hw-client/include/protoflow/hw/hw_client.hpp))
- **`hw_client` class**: Type-safe hardware operations
- **`hw_handle`**: Resource handle wrapper
- **`scoped_hw_access`**: RAII resource management
- **Operations**: request_access, release, read, write, ioctl
- **Error handling**: `std::expected` for all operations
- **Buffer limits**: 1 MB maximum I/O size

### Wire Protocol

#### Hardware Access Flow
```
App → Main: REQUEST_HW_ACCESS (resource, mode, timeout)
Main → App: HW_ACCESS_GRANTED (handle, capabilities)
           or HW_ACCESS_DENIED (reason)
```

#### I/O Operations
```
App → Main: HW_WRITE (handle, data)
Main → App: HW_WRITE_ACK (bytes_written)

App → Main: HW_READ (handle, max_length)
Main → App: HW_READ_RESPONSE (data)

App → Main: HW_IOCTL (handle, request, args)
Main → App: HW_IOCTL_RESPONSE (result, data)
```

#### Resource Release
```
App → Main: HW_RELEASE (handle)
Main → App: HW_RELEASE_ACK
```

## Security Model

1. **Main app runs as root** - exclusive hardware access
2. **Registered apps unprivileged** - no direct hardware access
3. **All I/O proxied** via RPC to main app
4. **Hardware requirements** declared in app manifest
5. **Arbitration enforced** by main app's Hardware Arbitration Service

## Integration

### CMake
```cmake
find_package(protoflow-hw-client REQUIRED)
target_link_libraries(my_app PRIVATE protoflow::hw-client)
```

### Usage Example
```cpp
#include <protoflow/hw/hw.hpp>

hw::hw_client client{transport};

// Request access with RAII
auto access = client.request_access("/dev/ttyUSB0");
if (access) {
    hw::scoped_hw_access scoped{client, access->handle};
    
    // Write
    std::vector<std::byte> cmd = {std::byte{0xAA}};
    auto written = client.write(scoped.handle(), cmd);
    
    // Read
    auto data = client.read(scoped.handle(), 256);
    
    // Auto-released on scope exit
}
```

## Test Results

**All 19 tests passing:**
- 11 client functionality tests
- 8 protocol structure tests

### Test Coverage
- Handle validity and RAII
- Error string conversion
- Successful access requests
- Access denial handling
- Resource release
- Read/write operations
- IOCTL operations
- Transport error handling
- Buffer size limits
- Message structure validation
- Error codes and capability flags

## Build Configuration

Added to root CMakeLists.txt:
- `BUILD_HW_CLIENT` option
- Library subdirectory inclusion
- Test subdirectory inclusion
- Status reporting

## Documentation

1. **[README.md](../libs/protoflow-hw-client/README.md)** - Full library documentation
2. **[library-hw-client.md](../docs/library-hw-client.md)** - API reference
3. **[hw_client_example.cpp](../libs/protoflow-hw-client/examples/hw_client_example.cpp)** - 6 usage examples

## Next Steps

The client library is complete and tested. The corresponding server-side implementation will be strictly in the main app as:

1. **HardwareArbitrationService** - Service handling requests
2. **Hardware resource management** - State machines for arbitration
3. **Actual hardware I/O** - Platform-specific implementations

## Dependencies

- `protoflow-rpc` - Base RPC protocol
- C++23 compiler with `std::expected`
- No runtime dependencies (header-only)
