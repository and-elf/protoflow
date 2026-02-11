# App Registration Service FSM

The AppRegistrationService uses the protoflow FSM library to manage the lifecycle of registered applications.

## States

- **unregistered**: Initial state, app not yet registered
- **registered**: App has been registered but not yet sent a heartbeat
- **alive**: App is registered and sending regular heartbeats
- **dead**: App has timed out (missed heartbeat deadline)

## Events

- **register_app**: Register or re-register an application
- **heartbeat**: Keepalive signal from application
- **timeout**: Heartbeat timeout detected
- **disconnect**: Explicit disconnect request

## State Machine

```
┌──────────────┐
│ unregistered │◄────────┐
└──────┬───────┘         │
       │                 │
       │ register_app    │ disconnect
       │                 │
       ▼                 │
┌────────────┐           │
│ registered │◄──────────┼─────────────────┐
└──────┬─────┘           │                 │
       │                 │                 │
       │ heartbeat       │                 │
       │                 │                 │
       ▼                 │                 │
┌────────┐      timeout  │    disconnect   │
│ alive  ├───────────────┤                 │
└────┬───┘               │                 │
     │                   │                 │
     │ disconnect        │                 │
     └───────────────────┘                 │
     │ heartbeat (stay)                    │
     └───┐                                 │
         │                                 │
         ▼                 register_app    │
    ┌────────┐             /heartbeat      │
    │  dead  ├────────────────────────────┘
    └────────┘
```

## Transitions

| From State    | Event        | To State     | Action                     |
|---------------|--------------|--------------|----------------------------|
| unregistered  | register_app | registered   | on_register()              |
| registered    | heartbeat    | alive        | on_first_heartbeat()       |
| registered    | register_app | registered   | on_reregister()            |
| registered    | disconnect   | unregistered | on_disconnect()            |
| alive         | heartbeat    | alive        | on_heartbeat()             |
| alive         | timeout      | dead         | on_timeout()               |
| alive         | disconnect   | unregistered | on_disconnect()            |
| dead          | register_app | registered   | on_recovery()              |
| dead          | heartbeat    | alive        | on_recovery_heartbeat()    |
| dead          | disconnect   | unregistered | on_disconnect()            |

## Implementation

The FSM is implemented using the `protoflow::fsm::table` construct from the FSM table library. Each registered application gets its own `AppStateMachine` instance that manages its lifecycle independently.

Key benefits:
- **Type-safe**: All transitions are verified at compile-time
- **Declarative**: State table clearly shows all valid transitions
- **Maintainable**: Easy to add new states or transitions
- **Testable**: FSM behavior can be tested in isolation

## Usage Example

```cpp
// App registers
registration_service.register_app(app_registration);
// FSM: unregistered -> registered

// App sends first heartbeat
registration_service.update_keepalive(app_name);
// FSM: registered -> alive

// App sends periodic heartbeats
registration_service.update_keepalive(app_name);
// FSM: alive -> alive (stay)

// Timeout detected (no heartbeat)
// FSM: alive -> dead

// App re-registers after timeout
registration_service.register_app(app_registration);
// FSM: dead -> registered
```
