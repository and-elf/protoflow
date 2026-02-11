# FSM API Comparison Guide

Protoflow provides two FSM (Finite State Machine) APIs with different design philosophies. This guide helps you choose the right one for your use case.

## Quick Comparison

| Aspect | when-then-to (fsm) | table (fsm_table) |
|--------|-------------------|-------------------|
| **Style** | Functional, fluent | Declarative, tabular |
| **Learning curve** | Gentle | Moderate |
| **Best for** | Prototypes, small FSMs | Production, large FSMs |
| **Boilerplate** | Minimal | Requires class structure |
| **C++ Level** | C++14-17 friendly | Modern C++20-23 |
| **Visual match** | Linear flow | State diagrams |
| **Member functions** | Lambdas or free functions | Member function pointers |

## The Same FSM in Both Styles

Let's implement a simple door controller FSM that has states: `Closed`, `Open`, `Locked` and events: `OpenDoor`, `CloseDoor`, `Lock`, `Unlock`.

### Option 1: when-then-to API

**Best for:** Learning, prototyping, functional programming style

```cpp
#include <protoflow/fsm.hpp>

enum class DoorState { Closed, Open, Locked };
enum class DoorEvent { OpenDoor, CloseDoor, Lock, Unlock };

// String conversion for logging
namespace protoflow::fsm {
    template<>
    inline std::string to_string(const DoorState& state) {
        switch (state) {
            case DoorState::Closed: return "Closed";
            case DoorState::Open: return "Open";
            case DoorState::Locked: return "Locked";
        }
        return "Unknown";
    }

    template<>
    inline std::string to_string(const DoorEvent& event) {
        switch (event) {
            case DoorEvent::OpenDoor: return "OpenDoor";
            case DoorEvent::CloseDoor: return "CloseDoor";
            case DoorEvent::Lock: return "Lock";
            case DoorEvent::Unlock: return "Unlock";
        }
        return "Unknown";
    }
}

void example_when_then_to() {
    using namespace protoflow::fsm;
    
    NullSink sink;
    int open_count = 0;
    int close_count = 0;
    int lock_count = 0;
    
    auto door_fsm = make_fsm(
        "door_controller",
        // Closed state transitions
        when<DoorState::Closed, DoorEvent::OpenDoor>()
            .then([&](){ 
                open_count++; 
                std::cout << "Opening door\n";
            })
            .to<DoorState::Open>()
        | when<DoorState::Closed, DoorEvent::Lock>()
            .then([&](){ 
                lock_count++; 
                std::cout << "Locking door\n";
            })
            .to<DoorState::Locked>()
        
        // Open state transitions
        | when<DoorState::Open, DoorEvent::CloseDoor>()
            .then([&](){ 
                close_count++; 
                std::cout << "Closing door\n";
            })
            .to<DoorState::Closed>()
        
        // Locked state transitions
        | when<DoorState::Locked, DoorEvent::Unlock>()
            .then([&](){ 
                std::cout << "Unlocking door\n";
            })
            .to<DoorState::Closed>()
        
        // Catch-all for invalid transitions
        | otherwise()
            .then([&](){ 
                std::cout << "Invalid operation\n";
            })
            .to<DoorState::Closed>(),
        
        sink,
        DoorState::Closed  // Initial state
    );
    
    // Use the FSM
    door_fsm.process(DoorEvent::OpenDoor);   // Closed -> Open
    door_fsm.process(DoorEvent::CloseDoor);  // Open -> Closed
    door_fsm.process(DoorEvent::Lock);       // Closed -> Locked
    door_fsm.process(DoorEvent::OpenDoor);   // Invalid! -> triggers otherwise
}
```

**Advantages:**
- Reads naturally: "when in state X and event Y happens, then do Z, go to state W"
- Can be written inline in a single function
- Lambdas capture local variables easily
- Great for learning and experimenting
- Less template syntax to understand

**When to use:**
- Learning FSMs for the first time
- Quick prototypes and experiments
- State machine logic is simple (<10 states)
- You prefer functional programming style
- Working with developers new to modern C++

---

### Option 2: table API

**Best for:** Production code, large FSMs, class-based designs

```cpp
#include <protoflow/fsm_table.hpp>
#include <iostream>

enum class DoorState { Closed, Open, Locked };
enum class DoorEvent { OpenDoor, CloseDoor, Lock, Unlock };

class DoorController {
public:
    // FSM state table - reads like a state transition diagram
    static constexpr auto state_table = 
        protoflow::fsm::table<DoorState, DoorEvent>(
            // Closed state: can open or lock
            protoflow::fsm::state<DoorState::Closed>(
                protoflow::fsm::on<DoorEvent::OpenDoor> = 
                    protoflow::fsm::to<DoorState::Open>(&DoorController::on_open),
                protoflow::fsm::on<DoorEvent::Lock> = 
                    protoflow::fsm::to<DoorState::Locked>(&DoorController::on_lock)
            ),
            // Open state: can only close
            protoflow::fsm::state<DoorState::Open>(
                protoflow::fsm::on<DoorEvent::CloseDoor> = 
                    protoflow::fsm::to<DoorState::Closed>(&DoorController::on_close)
            ),
            // Locked state: can only unlock
            protoflow::fsm::state<DoorState::Locked>(
                protoflow::fsm::on<DoorEvent::Unlock> = 
                    protoflow::fsm::to<DoorState::Closed>(&DoorController::on_unlock)
            )
        );

    DoorController() 
        : fsm_{DoorState::Closed, *this, sink_, state_table}
    {}

    void process(DoorEvent event) {
        fsm_.dispatch(event);
    }

    DoorState current_state() const {
        return fsm_.current_state();
    }

    int get_open_count() const { return open_count_; }
    int get_close_count() const { return close_count_; }
    int get_lock_count() const { return lock_count_; }

private:
    void on_open() {
        open_count_++;
        std::cout << "Opening door\n";
    }

    void on_close() {
        close_count_++;
        std::cout << "Closing door\n";
    }

    void on_lock() {
        lock_count_++;
        std::cout << "Locking door\n";
    }

    void on_unlock() {
        std::cout << "Unlocking door\n";
    }

    struct Sink {
        void on_transition(DoorState from, DoorEvent event, DoorState to) {
            // Optional: log transitions
        }
        void on_invalid(DoorState state, DoorEvent event) {
            std::cout << "Invalid operation\n";
        }
    };

    Sink sink_;
    int open_count_ = 0;
    int close_count_ = 0;
    int lock_count_ = 0;
    protoflow::fsm::machine<DoorState, DoorEvent, DoorController, Sink, 
                            decltype(state_table)> fsm_;
};

void example_table() {
    DoorController door;
    
    door.process(DoorEvent::OpenDoor);   // Closed -> Open
    door.process(DoorEvent::CloseDoor);  // Open -> Closed
    door.process(DoorEvent::Lock);       // Closed -> Locked
    door.process(DoorEvent::OpenDoor);   // Invalid! -> triggers on_invalid
}
```

**Advantages:**
- State table visually matches state diagrams
- All transitions for a state are grouped together
- Member functions are explicitly named (better for debugging)
- Better for large FSMs (easier to see the complete picture)
- `static constexpr` table enables compile-time validation
- Natural fit for class-based designs

**When to use:**
- Production code with complex state machines
- Large FSMs (>10 states) that need clear organization
- Class-based architectures where FSM is a member
- You want the code structure to match state diagrams
- Need compile-time validation of completeness

---

## Key Differences Explained

### 1. Lambda Capture vs Member Functions

**when-then-to:**
```cpp
int count = 0;
when<State::A, Event::X>()
    .then([&](){ count++; })  // Lambda captures local variable
    .to<State::B>()
```

**table:**
```cpp
class MyClass {
    void on_event() { count_++; }  // Member function accesses member variable
    int count_ = 0;
    static constexpr auto table = ...
        on<Event::X> = to<State::B>(&MyClass::on_event);
};
```

### 2. Chaining vs Grouping

**when-then-to** chains transitions linearly:
```cpp
when<S1, E1>().then(...).to<S2>()
| when<S1, E2>().then(...).to<S3>()
| when<S2, E1>().then(...).to<S4>()
```

**table** groups by source state:
```cpp
state<S1>(
    on<E1> = to<S2>(...),
    on<E2> = to<S3>(...)
),
state<S2>(
    on<E1> = to<S4>(...)
)
```

### 3. Invalid Transitions

**when-then-to** uses explicit `otherwise()`:
```cpp
| otherwise()
    .then([](){ /* handle invalid */ })
    .to<DefaultState>()
```

**table** uses Sink's `on_invalid()`:
```cpp
struct Sink {
    void on_invalid(State s, Event e) {
        // Handle invalid transition
    }
};
```

---

## Migration Between APIs

### From when-then-to to table

1. Create a class to hold the FSM
2. Convert lambdas to member functions
3. Group transitions by source state
4. Use `static constexpr auto state_table`
5. Replace `make_fsm()` with `machine<>` member

### From table to when-then-to

1. Extract member function bodies to lambdas
2. Flatten the state groupings into linear chain
3. Add explicit `otherwise()` for invalid transitions
4. Use `make_fsm()` instead of class member

---

## Real-World Examples

### when-then-to: Simple Protocol Parser
```cpp
auto parser = make_fsm(
    "parser",
    when<State::WaitHeader, Event::ByteReceived>()
        .then([&](){ parse_header(); })
        .to<State::WaitPayload>()
    | when<State::WaitPayload, Event::ByteReceived>()
        .then([&](){ parse_payload(); })
        .to<State::Complete>()
    | otherwise().then([](){}).to<State::Error>(),
    sink, State::WaitHeader
);
```

### table: Application Lifecycle Manager
See the `AppRegistrationService` in `apps/main-app/` for a complete example managing application registration, heartbeat, timeout, and recovery states.

---

## Recommendation

**Start with when-then-to** if:
- You're new to FSMs
- Building a prototype
- Your FSM is relatively simple
- You prefer seeing the complete flow in one place

**Use table** if:
- You're building production code
- Your FSM is complex (many states/transitions)
- You want code structure matching your state diagrams
- You prefer class-based design with member functions

**Both are valid choices** - use what fits your team's style and project needs!
