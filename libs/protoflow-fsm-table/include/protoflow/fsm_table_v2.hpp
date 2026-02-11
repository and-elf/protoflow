#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <functional>

namespace protoflow::fsm {

// ============================================================================
// Core types
// ============================================================================

struct reject_t {};
inline constexpr reject_t reject{};

struct no_action_t {};
inline constexpr no_action_t no_action{};

// ============================================================================
// to<State> - creates a transition target
// ============================================================================

// Forward declare
template <auto To, typename Action>
struct to_with_action;

template <auto To>
struct to_t {
    static constexpr auto target = To;
    
    // With action
    template <typename MemberFn>
    constexpr auto operator()(MemberFn action) const {
        return to_with_action<To, MemberFn>{action};
    }
    
    // Without action
    constexpr auto operator()() const {
        return to_with_action<To, void*>{nullptr};
    }
};

template <auto To, typename Action>
struct to_with_action {
    static constexpr auto target = To;
    Action action;
};

template <auto S>
inline constexpr to_t<S> to{};

// ============================================================================
// on<Event> - creates event bindings
// ============================================================================

// Forward declare cell
template <auto Event, auto To, typename Action>
struct cell;

template <auto Event>
struct on_t {
    static constexpr auto event = Event;
    
    template <auto To, typename Action>
    constexpr auto operator=(to_with_action<To, Action> t) const {
        return cell<Event, To, Action>{t.action};
    }
    
    constexpr auto operator=(reject_t) const {
        return cell<Event, Event, no_action_t>{};  // Sentinel for reject
    }
};

template <auto Event, auto To, typename Action>
struct cell {
    static constexpr auto event = Event;
    static constexpr auto target = To;
    static constexpr bool is_reject = std::is_same_v<Action, no_action_t>;
    Action action;
};

template <auto E>
inline constexpr on_t<E> on{};

// ============================================================================
// state<S> - defines a state with its transitions
// ============================================================================

template <auto State, typename... Cells>
struct state_t {
    static constexpr auto state = State;
    std::tuple<Cells...> cells;
};

template <auto State, typename... Cells>
constexpr auto state(Cells... c) {
    return state_t<State, Cells...>{std::make_tuple(c...)};
}

// ============================================================================
// table - combines all states
// ============================================================================

template <typename StateEnum, typename EventEnum, typename... States>
struct table_t {
    using state_type = StateEnum;
    using event_type = EventEnum;
    std::tuple<States...> states;
};

template <typename StateEnum, typename EventEnum, typename... States>
constexpr auto table(States... s) {
    return table_t<StateEnum, EventEnum, States...>{std::make_tuple(s...)};
}

// ============================================================================
// machine - the FSM runtime
// ============================================================================

template <typename StateEnum, typename EventEnum, typename Context, typename Sink, typename Table>
class machine {
public:
    using state_type = StateEnum;
    using event_type = EventEnum;

    constexpr machine(StateEnum initial, Context& ctx, Sink& sink, Table table)
        : state_(initial), ctx_(ctx), sink_(sink), table_(table) {}

    constexpr StateEnum current_state() const {
        return state_;
    }

    constexpr bool dispatch(EventEnum event) {
        bool handled = dispatch_to_state<0>(event);
        if (!handled) {
            sink_.on_invalid(state_, event);
        }
        return handled;
    }

private:
    StateEnum state_;
    Context& ctx_;
    Sink& sink_;
    Table table_;

    // Try to dispatch to state at index Idx
    template <size_t Idx>
    constexpr bool dispatch_to_state(EventEnum event) {
        if constexpr (Idx < std::tuple_size_v<decltype(table_.states)>) {
            auto& state_row = std::get<Idx>(table_.states);
            if (state_row.state == state_) {
                return dispatch_in_state(state_row, event);
            }
            return dispatch_to_state<Idx + 1>(event);
        }
        return false;
    }

    // Try to dispatch within a state row
    template <typename StateRow>
    constexpr bool dispatch_in_state(StateRow& row, EventEnum event) {
        return dispatch_cell<StateRow, 0>(row, event);
    }

    // Try to dispatch to cell at index Idx
    template <typename StateRow, size_t Idx>
    constexpr bool dispatch_cell(StateRow& row, EventEnum event) {
        if constexpr (Idx < std::tuple_size_v<decltype(row.cells)>) {
            auto& c = std::get<Idx>(row.cells);
            if (c.event == event) {
                if constexpr (c.is_reject) {
                    return false;  // Explicitly rejected
                } else {
                    // Take the transition
                    auto old_state = state_;
                    state_ = c.target;
                    
                    // Invoke action if present
                    if constexpr (!std::is_same_v<decltype(c.action), void*>) {
                        if (c.action != nullptr) {
                            std::invoke(c.action, ctx_);
                        }
                    }
                    
                    sink_.on_transition(old_state, event, c.target);
                    return true;
                }
            }
            return dispatch_cell<StateRow, Idx + 1>(row, event);
        }
        return false;
    }
};

// Deduction guide
template <typename StateEnum, typename Context, typename Sink,  
          typename S2, typename EventEnum, typename... States>
machine(StateEnum, Context&, Sink&, table_t<S2, EventEnum, States...>) 
    -> machine<StateEnum, EventEnum, Context, Sink, table_t<S2, EventEnum, States...>>;

} // namespace protoflow::fsm
