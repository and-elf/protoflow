#pragma once

#include <concepts>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace protoflow::fsm {

// Type-level tags for compile-time validation
struct HasOtherwise {};
struct NoOtherwise {};
struct OtherwiseTag {};

// Sentinel value for otherwise transitions (no specific event)
inline constexpr int OtherwiseEvent = -1;

// Tag type for transitions without actions
struct NoAction {
    constexpr void operator()() const noexcept {}
};

// Transition represents a state machine transition
// Action can be any invocable with no arguments returning void, or NoAction
template<typename FromState, auto EventValue, typename ToState, typename Action = NoAction>
struct Transition {
    Action action;

    constexpr Transition(Action act = {}) : action(std::move(act)) {}
    
    constexpr bool has_action() const {
        return !std::is_same_v<Action, NoAction>;
    }
};

// FsmBuilder accumulates transitions and tracks whether otherwise() is defined
template<typename OtherwiseState, typename... Transitions>
struct FsmBuilder {
    using otherwise_state = OtherwiseState;
    std::tuple<Transitions...> transitions;

    constexpr FsmBuilder(std::tuple<Transitions...> t)
        : transitions(std::move(t)) {}
};

// Helper to create destination state selector with action
template<typename State, auto EventValue, typename Action>
struct TransitionBuilder {
    Action action;

    template<auto ToStateValue>
    constexpr auto to() const {
        using ToState = std::integral_constant<decltype(ToStateValue), ToStateValue>;
        using T = Transition<State, EventValue, ToState, Action>;
        return FsmBuilder<NoOtherwise, T>{std::tuple{T{action}}};
    }

    constexpr auto stay() const {
        return to<State::value>();
    }
};

// Helper to create destination state selector without action
template<typename State, auto EventValue>
struct NoActionTransitionBuilder {
    template<auto ToStateValue>
    constexpr auto to() const {
        using ToState = std::integral_constant<decltype(ToStateValue), ToStateValue>;
        using T = Transition<State, EventValue, ToState, NoAction>;
        return FsmBuilder<NoOtherwise, T>{std::tuple{T{}}};
    }

    constexpr auto stay() const {
        return to<State::value>();
    }
};

// When clause builder - supports both .then().to() and direct .to()
template<typename State, auto EventValue>
struct When {
    // With action: .then(action).to<State>()
    template<std::invocable Callable>
    constexpr auto then(Callable&& action) const {
        return TransitionBuilder<State, EventValue, std::remove_cvref_t<Callable>>{std::forward<Callable>(action)};
    }
    
    // Without action: .to<State>() or .stay()
    template<auto ToStateValue>
    constexpr auto to() const {
        return NoActionTransitionBuilder<State, EventValue>{}.template to<ToStateValue>();
    }
    
    constexpr auto stay() const {
        return NoActionTransitionBuilder<State, EventValue>{}.stay();
    }
};

// Factory function for when clause
template<auto StateValue, auto EventValue>
constexpr auto when() {
    using State = std::integral_constant<decltype(StateValue), StateValue>;
    return When<State, EventValue>{};
}

// Otherwise clause builder - supports both .then().to() and direct .to()
struct OtherwiseBuilder {
    template<typename Action>
    struct OtherwiseTransitionBuilder {
        Action action;

        template<auto ToStateValue>
        constexpr auto to() const {
            using ToState = std::integral_constant<decltype(ToStateValue), ToStateValue>;
            using T = Transition<OtherwiseTag, OtherwiseEvent, ToState, Action>;
            return FsmBuilder<HasOtherwise, T>{std::tuple{T{action}}};
        }
    };

    template<std::invocable Callable>
    constexpr auto then(Callable&& action) const {
        return OtherwiseTransitionBuilder<std::remove_cvref_t<Callable>>{std::forward<Callable>(action)};
    }
    
    // Without action: .to<State>()
    template<auto ToStateValue>
    constexpr auto to() const {
        using ToState = std::integral_constant<decltype(ToStateValue), ToStateValue>;
        using T = Transition<OtherwiseTag, OtherwiseEvent, ToState, NoAction>;
        return FsmBuilder<HasOtherwise, T>{std::tuple{T{}}};
    }
};

// Factory function for otherwise clause
constexpr OtherwiseBuilder otherwise() {
    return {};
}

// Combine two FSM builders with operator|
template<typename O1, typename... T1, typename O2, typename... T2>
constexpr auto operator|(FsmBuilder<O1, T1...> a, FsmBuilder<O2, T2...> b) {
    using NewOtherwise = std::conditional_t<
        std::is_same_v<O1, HasOtherwise> || std::is_same_v<O2, HasOtherwise>,
        HasOtherwise,
        NoOtherwise>;

    return FsmBuilder<NewOtherwise, T1..., T2...>{
        std::tuple_cat(a.transitions, b.transitions)};
}

} // namespace protoflow::fsm
