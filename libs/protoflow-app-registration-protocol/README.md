# protoflow-app-registration-protocol

Header-only library providing shared protocol definitions for application registration.

## Overview

This library defines the common FSM states, events, and wire protocol messages used by both the app registration client and server implementations. By extracting these definitions into a separate library, we ensure consistency between client and server while avoiding code duplication.

## Contents

- **States** (`states.hpp`): FSM states for the registration lifecycle
  - Disconnected
  - Connecting
  - Registering
  - Registered
  - Reconnecting
  - Failed

- **Events** (`events.hpp`): FSM events for state transitions
  - Connect, Connected
  - HandshakeComplete
  - RegistrationAck
  - HeartbeatTick, HeartbeatAck
  - Disconnected, Reconnect
  - FatalError, Shutdown

- **Messages** (`messages.hpp`): Wire protocol definitions
  - Commands: hello, register_app, heartbeat, render_fragment, etc.
  - Message structures: hello_msg, register_app_msg, heartbeat_msg, etc.
  - Packed binary layout for network transmission

## Usage

```cpp
#include <protoflow/app_registration_protocol.hpp>

using namespace protoflow::app_registration_protocol;

// Use states and events in your FSM
State current = State::Disconnected;
Event event = Event::Connect;

// Use protocol messages
hello_msg hello{.version = 1};
auto cmd = static_cast<uint16_t>(command::hello);
```

## Dependencies

None - this is a header-only library with no external dependencies.

## Integration

### CMake

```cmake
find_package(protoflow-app-registration-protocol REQUIRED)
target_link_libraries(your_target 
    PRIVATE protoflow::app-registration-protocol
)
```
