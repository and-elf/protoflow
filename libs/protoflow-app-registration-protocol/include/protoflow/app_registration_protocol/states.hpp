#pragma once

#include <string_view>

namespace protoflow::app_registration_protocol {

/// FSM states for app registration lifecycle
/// Used by both client and server implementations
enum class State {
    /// Initial state - disconnected from peer
    Disconnected,
    
    /// TCP connection established, waiting for handshake
    Connecting,
    
    /// Handshake complete, waiting for registration acknowledgment
    Registering,
    
    /// Successfully registered, sending heartbeats
    Registered,
    
    /// Connection lost, attempting to reconnect
    Reconnecting,
    
    /// Fatal error, requires manual intervention
    Failed
};

/// Convert state to string for logging
[[nodiscard]] inline std::string to_string(State state) noexcept {
    switch (state) {
        case State::Disconnected: return "Disconnected";
        case State::Connecting: return "Connecting";
        case State::Registering: return "Registering";
        case State::Registered: return "Registered";
        case State::Reconnecting: return "Reconnecting";
        case State::Failed: return "Failed";
    }
    return "Unknown";
}

} // namespace protoflow::app_registration_protocol
