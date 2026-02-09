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

// Transition represents a state machine transition
template<typename FromState, auto EventValue, typename ToState, typename Action>
struct Transition {
    Action action;

    constexpr Transition(Action act) : action(std::move(act)) {}
};

// FsmBuilder accumulates transitions and tracks whether otherwise() is defined
template<typename OtherwiseState, typename... Transitions>
struct FsmBuilder {
    using otherwise_state = OtherwiseState;
    std::tuple<Transitions...> transitions;

    constexpr FsmBuilder(std::tuple<Transitions...> t)
        : transitions(std::move(t)) {}
};

// Helper to create destination state selector
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

// When clause builder
template<typename State, auto EventValue>
struct When {
    template<typename Action>
    constexpr auto then(Action action) const {
        return TransitionBuilder<State, EventValue, Action>{std::move(action)};
    }
};

// Factory function for when clause
template<auto StateValue, auto EventValue>
constexpr auto when() {
    using State = std::integral_constant<decltype(StateValue), StateValue>;
    return When<State, EventValue>{};
}

// Otherwise clause builder
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

    template<typename Action>
    constexpr auto then(Action action) const {
        return OtherwiseTransitionBuilder<Action>{std::move(action)};
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
