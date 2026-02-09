#pragma once

#include "sink.hpp"
#include "types.hpp"
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace protoflow::fsm {

// Helper to convert states and events to strings
template<typename T>
std::string to_string(const T& value) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(value);
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<int>(value));
    } else {
        return "unknown";
    }
}

// Main FSM class
template<typename Builder, FsmSink Sink, typename State>
class Fsm {
public:
    using CurrentState = State;

    constexpr Fsm(std::string name, Builder builder, Sink sink, State initial_state)
        : name_(std::move(name))
        , transitions_(builder.transitions)
        , sink_(std::move(sink))
        , current_state_(initial_state) {
        
        static_assert(
            std::is_same_v<typename Builder::otherwise_state, HasOtherwise>,
            "FSM must define an otherwise() transition"
        );
    }

    // Process an event and transition to new state
    template<typename Event>
    void process(const Event& event) {
        State old_state = current_state_;
        bool transitioned = try_transition(event);

        if (!transitioned) {
            // Execute otherwise transition
            execute_otherwise_transition(old_state);
        }
    }

    // Get current state
    State state() const {
        return current_state_;
    }

private:
    template<typename Event>
    bool try_transition(const Event& event) {
        bool found = false;
        
        std::apply([&](const auto&... transitions) {
            (try_single_transition(transitions, event, found) || ...);
        }, transitions_);

        return found;
    }

    template<typename Transition, typename Event>
    bool try_single_transition(const Transition& transition, const Event& event, bool& found) {
        if (found) return false;

        using FromState = typename std::remove_cvref_t<decltype(transition)>::template param<0>;
        using TransEvent = typename std::remove_cvref_t<decltype(transition)>::template param<1>;
        using ToState = typename std::remove_cvref_t<decltype(transition)>::template param<2>;

        // Skip otherwise transitions
        if constexpr (std::is_same_v<FromState, OtherwiseTag>) {
            return false;
        }

        // Check if this transition matches current state and event
        if constexpr (std::is_same_v<Event, TransEvent>) {
            if (matches_state<FromState>()) {
                State old_state = current_state_;
                
                // Execute action
                transition.action();
                
                // Transition to new state
                current_state_ = ToState{};
                
                // Emit event
                sink_.emit(FsmEvent{
                    .fsm_name = name_,
                    .from_state = to_string(old_state),
                    .to_state = to_string(current_state_),
                    .event = to_string(event),
                    .kind = FsmEvent::Kind::Transition
                });
                
                found = true;
                return true;
            }
        }
        
        return false;
    }

    void execute_otherwise_transition(State old_state) {
        std::apply([&](const auto&... transitions) {
            (execute_otherwise_if_matches(transitions, old_state) || ...);
        }, transitions_);
    }

    template<typename Transition>
    bool execute_otherwise_if_matches(const Transition& transition, State old_state) {
        using FromState = typename std::remove_cvref_t<decltype(transition)>::template param<0>;
        using ToState = typename std::remove_cvref_t<decltype(transition)>::template param<2>;

        if constexpr (std::is_same_v<FromState, OtherwiseTag>) {
            // Execute action
            transition.action();
            
            // Transition to new state
            current_state_ = ToState{};
            
            // Emit event
            sink_.emit(FsmEvent{
                .fsm_name = name_,
                .from_state = to_string(old_state),
                .to_state = to_string(current_state_),
                .event = "otherwise",
                .kind = FsmEvent::Kind::Otherwise
            });
            
            return true;
        }
        
        return false;
    }

    template<typename S>
    bool matches_state() const {
        return std::is_same_v<State, S> || 
               (std::is_convertible_v<State, S> && current_state_ == S{});
    }

    std::string name_;
    typename Builder::transitions_type transitions_;
    Sink sink_;
    State current_state_;
};

// Alternative implementation with better type extraction
// This version uses a cleaner approach to access transition template parameters
namespace detail {
    template<typename FromState, auto EventValue, typename ToState, typename Action>
    struct TransitionTraits {
        using from_state = FromState;
        static constexpr auto event_value = EventValue;
        using to_state = ToState;
        using action_type = Action;
    };

    template<typename T>
    struct GetTraits;

    template<typename FromState, auto EventValue, typename ToState, typename Action>
    struct GetTraits<Transition<FromState, EventValue, ToState, Action>> {
        using type = TransitionTraits<FromState, EventValue, ToState, Action>;
    };

    template<typename T>
    using get_traits = typename GetTraits<std::remove_cvref_t<T>>::type;
}

// Improved FSM implementation using traits
template<typename Builder, FsmSink Sink, typename StateEnum>
class FsmImpl {
public:
    using CurrentState = StateEnum;

    constexpr FsmImpl(std::string name, Builder builder, Sink sink, StateEnum initial_state)
        : name_(std::move(name))
        , transitions_(builder.transitions)
        , sink_(std::move(sink))
        , current_state_(initial_state) {
        
        static_assert(
            std::is_same_v<typename Builder::otherwise_state, HasOtherwise>,
            "FSM must define an otherwise() transition"
        );
    }

    template<typename Event>
    void process(const Event& event) {
        StateEnum old_state = current_state_;
        bool transitioned = try_transition(event);

        if (!transitioned) {
            execute_otherwise_transition(old_state, event);
        }
    }

    StateEnum state() const {
        return current_state_;
    }

private:
    template<typename Event>
    bool try_transition(const Event& event) {
        bool found = false;
        
        std::apply([&](const auto&... transitions) {
            (try_single_transition(transitions, event, found) || ...);
        }, transitions_);

        return found;
    }

    template<typename Transition, typename Event>
    bool try_single_transition(const Transition& transition, const Event& event, bool& found) {
        if (found) return false;

        using Traits = detail::get_traits<Transition>;
        using FromState = typename Traits::from_state;
        using ToState = typename Traits::to_state;

        // Skip otherwise transitions
        if constexpr (std::is_same_v<FromState, OtherwiseTag>) {
            return false;
        } else {
            // Check if this transition matches current state and event
            if (matches_state<FromState>() && event == Traits::event_value) {
                StateEnum old_state = current_state_;
                
                // Execute action
                transition.action();
                
                // Extract state value from integral_constant
                if constexpr (requires { ToState::value; }) {
                    current_state_ = ToState::value;
                } else {
                    current_state_ = ToState{};
                }
                
                // Emit event
                sink_.emit(FsmEvent{
                    .fsm_name = name_,
                    .from_state = to_string(old_state),
                    .to_state = to_string(current_state_),
                    .event = to_string(event),
                    .kind = FsmEvent::Kind::Transition
                });
                
                found = true;
                return true;
            }
        }
        
        return false;
    }

    template<typename Event>
    void execute_otherwise_transition(StateEnum old_state, const Event& event) {
        std::apply([&](const auto&... transitions) {
            (execute_otherwise_if_matches(transitions, old_state, event) || ...);
        }, transitions_);
    }

    template<typename Transition, typename Event>
    bool execute_otherwise_if_matches(const Transition& transition, StateEnum old_state, const Event& event) {
        using Traits = detail::get_traits<Transition>;
        using FromState = typename Traits::from_state;
        using ToState = typename Traits::to_state;

        if constexpr (std::is_same_v<FromState, OtherwiseTag>) {
            // Execute action
            transition.action();
            
            // Extract state value from integral_constant
            if constexpr (requires { ToState::value; }) {
                current_state_ = ToState::value;
            } else {
                current_state_ = ToState{};
            }
            
            // Emit event
            sink_.emit(FsmEvent{
                .fsm_name = name_,
                .from_state = to_string(old_state),
                .to_state = to_string(current_state_),
                .event = to_string(event),
                .kind = FsmEvent::Kind::Otherwise
            });
            
            return true;
        }
        
        return false;
    }

    template<typename S>
    bool matches_state() const {
        if constexpr (requires { S::value; }) {
            // S is std::integral_constant
            return current_state_ == S::value;
        } else if constexpr (std::is_same_v<StateEnum, S>) {
            return true;
        } else {
            return false;
        }
    }

    std::string name_;
    std::remove_cvref_t<decltype(std::declval<Builder>().transitions)> transitions_;
    Sink sink_;
    StateEnum current_state_;
};

// Factory function for creating FSMs
template<typename Builder, FsmSink Sink, typename StateEnum>
auto make_fsm(std::string name, Builder builder, Sink sink, StateEnum initial_state) {
    return FsmImpl<Builder, Sink, StateEnum>{
        std::move(name),
        std::move(builder),
        std::move(sink),
        initial_state
    };
}

} // namespace protoflow::fsm
