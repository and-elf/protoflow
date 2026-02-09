# protoflow-fsm

Finite State Machine library with compile-time validation and observable behavior.

## Features

- Protocol-driven behavior definitions
- State validation
- Misbehavior detection and logging
- **Compile-time verification** of FSM completeness
- **Required `otherwise()` clause** enforced at compile-time
- **Observable transitions** via pluggable sinks

---

## API Design

### Basic Usage

```cpp
auto fsm = make_fsm(
    "app_registration",
    when<AppState::Unregistered, AppEvent::Register>()
        .then(register_app)
        .to<AppState::Registered>()

    | when<AppState::Registered, AppEvent::Heartbeat>()
        .then(update_heartbeat)
        .to<AppState::Alive>()

    | when<AppState::Alive, AppEvent::Heartbeat>()
        .then(update_heartbeat)
        .stay()

    | when<AppState::Alive, AppEvent::Timeout>()
        .then(mark_dead)
        .to<AppState::Dead>()

    | otherwise()
        .then(reset_app)
        .to<AppState::Unregistered>(),
    
    sink,
    AppState::Unregistered  // initial state
);
```

### Compile-Time Validation

FSMs **must** define an `otherwise()` transition.

❌ **Compile Error**:
```cpp
auto fsm = make_fsm(
    "incomplete",
    when<Unregistered, Register>().then(reg).to<Registered>()
  | when<Registered, Heartbeat>().then(hb).to<Alive>(),
    sink,
    Unregistered
);
```

**Error message**:
```
static assertion failed: FSM must define an otherwise() transition
```

✅ **Valid FSM**:
```cpp
auto fsm = make_fsm(
    "complete",
    when<Unregistered, Register>().then(reg).to<Registered>()
  | when<Registered, Heartbeat>().then(hb).to<Alive>()
  | otherwise().then(reset).to<Unregistered>(),
    sink,
    Unregistered
);
```

### Type-Level Enforcement

Enforced via type-level tags (`HasOtherwise`, `NoOtherwise`) and `std::expected` internally.

---

## Core Building Blocks

### When Clause

```cpp
template<typename State, typename Event>
struct When {
    template<typename Action>
    auto then(Action action) const {
        return [action]<typename To>() {
            using T = Transition<State, Event, To, Action>;
            return FsmBuilder<NoOtherwise, T>{ std::tuple{T{action}} };
        };
    }
};

template<typename State, typename Event>
constexpr When<State, Event> when(Event) {
    return {};
}
```

### Otherwise Clause

```cpp
struct Otherwise {
    template<typename Action>
    auto then(Action action) const {
        return [action]<typename To>() {
            using T = Transition<OtherwiseTag, void, To, Action>;
            return FsmBuilder<HasOtherwise, T>{ std::tuple{T{action}} };
        };
    }
};

constexpr Otherwise otherwise() {
    return {};
}
```

### Combining Transitions

```cpp
template<typename O1, typename... T1,
         typename O2, typename... T2>
constexpr auto operator|(
    FsmBuilder<O1, T1...> a,
    FsmBuilder<O2, T2...> b)
{
    using NewOtherwise =
        std::conditional_t<
            std::is_same_v<O1, HasOtherwise> ||
            std::is_same_v<O2, HasOtherwise>,
            HasOtherwise,
            NoOtherwise>;

    return FsmBuilder<NewOtherwise, T1..., T2...>{
        std::tuple_cat(a.transitions, b.transitions)
    };
}
```

### FSM Construction

```cpp
template<typename Builder, typename Sink>
auto make_fsm(std::string name, Builder b, Sink sink) {
    static_assert(
        std::is_same_v<typename Builder::otherwise_state, HasOtherwise>,
        "FSM must define otherwise()"
    );

    return Fsm<Builder, Sink>{
        .name = std::move(name),
        .transitions = b.transitions,
        .sink = std::move(sink)
    };
}
```

---

## FSM Sinks

### Concept

```cpp
template<typename S>
concept FsmSink =
    requires(S s, FsmEvent e) {
        s.emit(e);
    };

struct FsmEvent {
    std::string fsm;
    std::string from;
    std::string to;
    std::string event;
    enum class Kind { Transition, Otherwise } kind;
};
```

### Emission Point

```cpp
fsm.sink.emit(FsmEvent{
    .fsm = fsm.name,
    .from = to_string(old_state),
    .to = to_string(state),
    .event = to_string(event),
    .kind = FsmEvent::Kind::Transition
});
```

### Sink Implementations

#### Logging Sink

```cpp
struct LoggingSink {
    std::string fsm_name;
    LogLevel min_level = LogLevel::Warn;

    void emit(const FsmEvent& e) {
        if (e.kind == FsmEvent::Kind::Otherwise)
            log(LogLevel::Error, e);
        else
            log(LogLevel::Debug, e);
    }
};
```

**Behavior**:
- `otherwise()` transitions → logged as **Error**
- Regular transitions → logged as **Debug**

#### Metrics Sink

```cpp
struct MetricsSink {
    void emit(const FsmEvent& e) {
        counters[e.fsm][e.kind]++;
    }
};
```

**Behavior**:
- Tracks transition counts per FSM
- Separate counters for normal vs. otherwise transitions

#### Composite Sink

```cpp
template<typename... Sinks>
struct FanoutSink {
    std::tuple<Sinks...> sinks;

    void emit(const FsmEvent& e) {
        std::apply([&](auto&... s) {
            (s.emit(e), ...);
        }, sinks);
    }
};
```

**Usage**:
```cpp
auto sink = FanoutSink{
    std::make_tuple(
        LoggingSink{"my_fsm"},
        MetricsSink{},
        TracingSink{}
    )
};
```

---

## Benefits

✓ **Observable behavior by construction**  
✓ **`otherwise()` transitions automatically logged as errors**  
✓ **Policy injection** without coupling FSM logic to logging  
✓ **Composable sinks** for multi-target emission  
✓ **Zero runtime overhead** for compile-time FSM construction  
✓ **Type-safe** state and event handling  
✓ **Compile-time completeness** verification  

---

## Implementation Notes

- Uses C++23 concepts and template metaprogramming
- State transitions tracked at compile time
- `std::expected` for internal error handling
- No dynamic allocation during FSM execution
- Suitable for embedded/real-time systems
