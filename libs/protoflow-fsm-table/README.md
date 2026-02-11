# protoflow-fsm-table

A modern C++20 compile-time finite state machine library with a table-based API.

## Features

- **Compile-time state machine**: All transitions are resolved at compile time
- **Type-safe**: States and events are strongly typed using enums
- **Table-based DSL**: Declarative, readable syntax for defining state machines
- **Action handlers**: Associate member functions with transitions
- **Rejection support**: Explicitly mark invalid transitions
- **Observer pattern**: Trace transitions and invalid events via sink/tracer
- **Header-only**: Easy integration into any project
- **Zero runtime overhead**: Template metaprogramming eliminates abstraction cost

## Quick Example

```cpp
#include <protoflow/fsm_table.hpp>

enum class State { idle, running, stopped };
enum class Event { start, stop, tick };

struct Controller {
    void start();
    void stop();
    void tick();
};

struct tracer {
    void on_transition(State from, Event ev, State to);
    void on_invalid(State from, Event ev);
};

int main() {
    using namespace protoflow::fsm;
    
    // Define the state machine table
    constexpr auto state_table =
        table<State, Event>(
            state<State::idle>(
                on<Event::start> = to_t<State::running, &Controller::start>{},
                on<Event::stop>  = reject
            ),
            state<State::running>(
                on<Event::tick>  = to_t<State::running, &Controller::tick>{},
                on<Event::stop>  = to_t<State::stopped, &Controller::stop>{}
            ),
            state<State::stopped>(
                on<Event::start> = to_t<State::running, &Controller::start>{},
                on<Event::tick>  = reject,
                on<Event::stop>  = reject
            )
        );
    
    Controller ctrl;
    tracer tr;
    
    machine m{State::idle, ctrl, tr, state_table};
    
    m.dispatch(Event::start);  // idle -> running
    m.dispatch(Event::tick);   // running -> running
    m.dispatch(Event::stop);   // running -> stopped
    
    return 0;
}
```

## API Reference

### State Definition

Use `state<StateValue>(...)` to define transitions from a state:

```cpp
state<State::idle>(
    on<Event::start> = to_t<State::running, &Controller::start>{},
    on<Event::stop>  = reject
)
```

### Transition Specification

**With action handler:**
```cpp
on<Event::start> = to<State::running>(&Controller::start)
```

**Without action handler:**
```cpp
on<Event::pause> = to<State::paused>()
```

**Reject transition:**
```cpp
on<Event::stop> = reject
```

### Table Construction

```cpp
constexpr auto state_table = table<StateEnum, EventEnum>(
    state<State1>(...),
    state<State2>(...),
    // ... one state() for each enum value
);
```

**As a class member (no complex types!):**
```cpp
struct MyClass {
    const auto state_table = 
        table<State, Event>(
            state<State::idle>(
                on<Event::start> = to<State::running>(&MyClass::start)
            )
        );
};
```

### Machine Creation

```cpp
machine m{
    initial_state,  // Starting state
    context,        // Object with action handlers
    sink,           // Tracer/observer object
    state_table     // State machine definition
};
```

The machine type is deduced via CTAD (Class Template Argument Deduction).

### Event Dispatching

```cpp
bool handled = m.dispatch(Event::start);
```

Returns `true` if the event was handled, `false` if rejected/invalid.

### Tracer Interface

The tracer/sink object must implement:

```cpp
struct tracer {
    void on_transition(State from, Event ev, State to);
    void on_invalid(State from, Event ev);
};
```

## Design Philosophy

This library uses modern C++ template metaprogramming to create zero-overhead state machines. The entire state transition table is encoded in types and resolved at compile time, resulting in code that's as efficient as hand-written switch statements but much more maintainable.

Key design choices:

- **Compile-time validation**: Invalid state machines are caught at compile time
- **Value semantics**: States and events are passed by value (enums)
- **Separation of concerns**: Context (actions), sink (observation), and table (structure) are independent
- **Explicit over implicit**: All transitions must be explicitly declared or rejected

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20+ (for building examples)

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(libs/protoflow-fsm-table)
target_link_libraries(your_target PRIVATE protoflow::fsm-table)
```

### Header-only

Simply add `include/` to your include path and:

```cpp
#include <protoflow/fsm_table.hpp>
```

## Examples

See the `examples/` directory for complete working examples:

- `controller_example.cpp`: Basic controller with start/stop/tick events

## Future Enhancements

Potential future additions:

- [ ] Compile-time exhaustiveness checking (verify all states have rows, all events are covered)
- [ ] Guard conditions on transitions
- [ ] Entry/exit actions for states
- [ ] Hierarchical state machines
- [ ] Logging/debugging utilities
- [ ] Graphviz visualization export

## License

[Your license here]

## Contributing

[Your contribution guidelines here]
