# FSM Table Library - Design Document

## Overview

The `protoflow-fsm-table` library provides a modern C++20 compile-time finite state machine with a declarative, table-based API. It uses template metaprogramming to resolve all transitions at compile time, resulting in zero runtime overhead.

## Design Principles

### 1. **Compile-Time Everything**

All state transitions are encoded in types and resolved at compile time. The compiler generates optimal code equivalent to hand-written switch statements.

### 2. **Type Safety**

- States and events are strongly typed using enums
- Transition types are checked at compile time
- Invalid state machine configurations cause compilation errors

### 3. **Separation of Concerns**

The design separates three independent concerns:

- **Structure** (table): Defines what transitions are valid
- **Behavior** (context): Implements action handlers
- **Observation** (sink): Monitors transitions and invalid events

### 4. **Zero Runtime Overhead**

Template metaprogramming eliminates all abstractions at compile time. The final code has no virtual calls, no dynamic dispatch, and no runtime lookups.

## Architecture

### Core Components

#### 1. Transition Descriptor

```cpp
template <auto From, auto On, auto To, auto Action>
struct transition {
    static constexpr auto from = From;
    static constexpr auto on   = On;
    static constexpr auto to   = To;
    static constexpr auto act  = Action;
};
```

A transition encodes:
- `From`: Source state (enum value)
- `On`: Triggering event (enum value)
- `To`: Destination state (enum value)
- `Action`: Member function pointer to invoke (or `no_action`)

#### 2. Transition Builder (`to_t`)

```cpp
template <auto To, auto Action>
struct to_t {
    static constexpr auto to_state = To;
    static constexpr auto action = Action;
};
```

Represents a partial transition (destination + action) that will be combined with source state and event.

#### 3. Event Binding (`on_t`)

```cpp
template <auto Event>
struct on_t {
    template <auto To, auto Action>
    constexpr auto operator=(to_t<To, Action>) const;
    
    constexpr auto operator=(reject_t) const;
};
```

The `on<Event>` object provides operator= to create event-to-transition mappings:

- `on<Event::start> = to_t<State::running, &Handler::start>{}` → valid transition
- `on<Event::stop> = reject` → explicitly rejected event

#### 4. State Row (`state_t`)

```cpp
template <auto State, typename... Cells>
struct state_t {
    static constexpr auto from = State;
    using cells = std::tuple<Cells...>;
};
```

Represents all outgoing transitions from a single state. Each "cell" is a pair of event and either a `to_t` or `reject_t`.

#### 5. Table (`table_t`)

```cpp
template <typename StateEnum, typename EventEnum, typename... States>
struct table_t {
    using state_type = StateEnum;
    using event_type = EventEnum;
    using states = std::tuple<States...>;
};
```

The complete state machine definition, containing all state rows.

#### 6. Machine

```cpp
template <typename StateEnum, typename EventEnum, 
          typename Context, typename Sink, typename Table>
class machine {
    StateEnum state_;
    Context& ctx_;
    Sink& sink_;
    
public:
    constexpr machine(StateEnum initial, Context& ctx, Sink& sink, Table);
    constexpr bool dispatch(EventEnum event);
    constexpr StateEnum current_state() const;
};
```

The runtime machine instance that:
- Holds current state
- References context (action handler)
- References sink (observer)
- Uses the table for dispatch

### Compile-Time Dispatch Generation

The `machine::dispatch` method uses template metaprogramming to generate an efficient dispatch mechanism:

```cpp
constexpr bool dispatch_impl(EventEnum event) {
    using all_trans = typename detail::all_transitions<Table>::type;
    constexpr size_t num_transitions = std::tuple_size_v<all_trans>;
    
    // Try each transition in sequence
    bool handled = try_transitions<all_trans>(
        event,
        std::make_index_sequence<num_transitions>{}
    );
    
    if (!handled) {
        sink_.on_invalid(state_, event);
    }
    
    return handled;
}
```

The `try_transitions` method generates a fold expression that tries each transition:

```cpp
template <typename TransitionList, size_t... Is>
constexpr bool try_transitions(EventEnum event, std::index_sequence<Is...>) {
    return (try_single_transition<Is, TransitionList>(event) || ...);
}
```

This expands to:
```cpp
return try_single_transition<0, ...>(event) ||
       try_single_transition<1, ...>(event) ||
       try_single_transition<2, ...>(event) ||
       ...;
```

Each `try_single_transition` is inlined and checks if the current state and event match that transition.

### Type Transformations

The library uses several compile-time transformations:

1. **Cell Expansion**: `std::pair<Event, to_t<...>>` → `transition<From, Event, To, Action>`
2. **Row Extraction**: `state_t<State, Cells...>` → `std::tuple<transition...>`
3. **Table Flattening**: `table_t<..., States...>` → flat `std::tuple` of all transitions
4. **Rejection Filtering**: `reject_t` cells are converted to `void` and filtered out

## Usage Patterns

### Basic Usage

```cpp
// 1. Define enums
enum class State { idle, running, stopped };
enum class Event { start, stop, tick };

// 2. Define context with action handlers
struct Controller {
    void start() { /* ... */ }
    void stop() { /* ... */ }
    void tick() { /* ... */ }
};

// 3. Define observer/sink
struct tracer {
    void on_transition(State from, Event ev, State to) { /* ... */ }
    void on_invalid(State from, Event ev) { /* ... */ }
};

// 4. Build the table
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
            on<Event::start> = to_t<State::running, &Controller::start>{}
        )
    );

// 5. Create machine
Controller ctrl;
tracer tr;
machine m{State::idle, ctrl, tr, state_table};

// 6. Dispatch events
m.dispatch(Event::start);  // Returns true, transitions idle → running
m.dispatch(Event::tick);   // Returns true, transitions running → running
m.dispatch(Event::stop);   // Returns true, transitions running → stopped
```

### Transitions Without Actions

For state changes that don't require side effects:

```cpp
state<State::paused>(
    on<Event::resume> = to_t<State::running, no_action>{}
)
```

### Explicit Rejection

To document that an event is invalid from a state:

```cpp
state<State::stopped>(
    on<Event::tick> = reject,
    on<Event::stop> = reject
)
```

Rejected events trigger `sink.on_invalid()` instead of `sink.on_transition()`.

### Self-Transitions

A state can transition to itself:

```cpp
state<State::running>(
    on<Event::tick> = to_t<State::running, &Controller::tick>{}
)
```

This still invokes the action handler and notifies the sink.

## Compile-Time Validation

The library provides optional validation helpers:

```cpp
// Check that table has the expected number of state rows
static_assert(validate_state_count<decltype(state_table), 3>());

// Check that a specific state has a row  
static_assert(validate_has_state<State::idle, decltype(state_table)>());
```

### Future Validation Enhancements

Potential additions (not yet implemented):

- Verify all enum states have rows
- Verify all events are handled or explicitly rejected in each state
- Check for unreachable states
- Validate that action signatures match context

## Performance Characteristics

### Compile Time

- Compilation time scales linearly with number of transitions
- Template instantiation depth is O(states × events)
- Modern compilers handle moderate FSMs (< 50 states) efficiently

### Runtime

- **State storage**: Single enum value
- **Dispatch**: Equivalent to optimal switch/if-chain
- **Transition**: Assignment + optional function call + observer notification
- **No allocations**: Everything is stack-based
- **No virtual calls**: All calls are direct or inlined

### Memory

- **Code size**: Comparable to hand-written FSM
- **Data size**: sizeof(StateEnum) + references to context and sink
- **Table**: Exists only at compile time, zero runtime footprint

## Comparison to Other Approaches

### vs. Table-Driven FSM (Runtime)

Traditional approach: Array of `[state][event] → (next_state, action_fn)`

**Advantages of FSM Table Library:**
- No runtime table lookups
- Better compiler optimization (inlining, constant propagation)
- Compile-time error checking
- Type-safe action handlers

**Disadvantages:**
- Longer compilation times
- Cannot modify FSM at runtime
- More complex implementation

### vs. State Pattern (OOP)

OOP approach: State interface with concrete state classes

**Advantages of FSM Table Library:**
- No virtual calls or dynamic dispatch
- More declarative specification
- Easier to see complete state machine structure
- Better performance (no indirection)

**Disadvantages:**
- Less flexible (can't add states at runtime)
- Requires C++20
- More template-heavy code

### vs. Boost.MSM / Boost.Statechart

**Advantages of FSM Table Library:**
- Simpler, more focused API
- Fewer dependencies
- Modern C++20 idioms
- More readable table syntax

**Disadvantages:**
- Fewer features (no sub-machines, orthogonal regions, etc.)
- Less mature
- Smaller community

## Extension Points

The library can be extended in several ways:

### 1. Guard Conditions

Add guard predicates to transitions:

```cpp
on<Event::start> = to_t<State::running, &Ctrl::start>{}
                   .when([](const Ctrl& c) { return c.ready(); })
```

### 2. Entry/Exit Actions

Execute actions when entering/leaving states:

```cpp
state<State::running>(
    /* ... transitions ... */
).on_entry(&Ctrl::enter_running)
 .on_exit(&Ctrl::exit_running)
```

### 3. Hierarchical States

Support parent/child state relationships for sharing transitions.

### 4. Deferred Events

Queue events for processing after a transition completes.

### 5. Compile-Time Visualization

Generate DOT graphs from the table for documentation:

```cpp
constexpr auto dot = generate_dot(state_table);
```

## Best Practices

### 1. Use Comprehensive Event Coverage

Either handle or explicitly reject every event in every state:

```cpp
state<State::idle>(
    on<Event::start> = to_t<State::running, &Ctrl::start>{},
    on<Event::stop>  = reject,  // Explicit: already stopped
    on<Event::tick>  = reject   // Explicit: not running
)
```

### 2. Keep Action Handlers Simple

Action functions should be fast and deterministic:

```cpp
void start() {
    // Good: Simple state change
    running_ = true;
    counter_ = 0;
}

void start() {
    // Avoid: Long-running operations
    network_connect_with_timeout(10000);  // Bad!
}
```

### 3. Use the Sink for Side Effects

Put logging, metrics, and notifications in the sink, not the actions:

```cpp
struct tracer {
    void on_transition(State from, Event ev, State to) {
        logger_.log("Transition: {} --[{}]--> {}", from, ev, to);
        metrics_.record_transition(from, to);
    }
};
```

### 4. Make Tables constexpr

Mark state tables as `constexpr` to enable compile-time computation:

```cpp
constexpr auto state_table = table<State, Event>(/* ... */);
```

### 5. Document State Machine Behavior

Add comments explaining the purpose of each state and transition:

```cpp
// IDLE: System is initialized but not processing
state<State::idle>(
    // Start processing: validate configuration and begin
    on<Event::start> = to_t<State::running, &Ctrl::start>{},
    // Stop is rejected: already stopped
    on<Event::stop>  = reject
)
```

## Troubleshooting

### Compilation Errors

**"couldn't deduce template parameter"**

Ensure you're using the correct table construction syntax:
```cpp
// Correct:
constexpr auto state_table = table<State, Event>(...);
machine m{State::idle, ctrl, tr, state_table};

// Wrong:
auto state_table = table(...);  // Missing template parameters
```

**"it must be a pointer-to-member of the form '&X::Y'"**

Member function pointers must be specified directly:
```cpp
// Correct:
to_t<State::running, &Controller::start>{}

// Wrong:
auto ptr = &Controller::start;
to_t<State::running, ptr>{}  // Can't use variable
```

### Runtime Issues

**Actions not being called**

Verify that:
1. The action is specified in the `to_t<>` template
2. The context object has the matching member function
3. The transition is actually being taken (check sink output)

**Events not handled**

Check that:
1. The current state has a row in the table
2. The event is mapped in that state's row
3. The event enum value matches exactly

## Future Work

- [ ] Compile-time exhaustiveness checking with better diagnostics
- [ ] Optional integration with magic_enum for automatic enum-to-string
- [ ] Graphviz DOT export for visualization
- [ ] Guard conditions support
- [ ] Entry/exit actions
- [ ] Hierarchical state machines
- [ ] Deferred event queues
- [ ] History states (shallow/deep)
- [ ] Orthogonal regions (parallel states)
- [ ] Better compiler error messages via concepts

## References

- C++20 Template Metaprogramming
- UML State Machine Specification
- Boost.MSM Documentation
- "Practical UML Statecharts in C/C++" by Miro Samek

## License

[Your license here]
