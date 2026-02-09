#pragma once

#include <concepts>
#include <string>

namespace protoflow::fsm {

// FSM event structure for sink emission
struct FsmEvent {
    enum class Kind {
        Transition,
        Otherwise
    };

    std::string fsm_name;
    std::string from_state;
    std::string to_state;
    std::string event;
    Kind kind;
};

// Concept for FSM sinks
template<typename S>
concept FsmSink = requires(S s, const FsmEvent& e) {
    { s.emit(e) } -> std::same_as<void>;
};

// Null sink that does nothing (for testing or when no observation needed)
struct NullSink {
    void emit(const FsmEvent&) const noexcept {}
};

} // namespace protoflow::fsm
