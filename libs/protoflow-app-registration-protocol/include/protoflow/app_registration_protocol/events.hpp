#pragma once

#include <string_view>

namespace protoflow::app_registration_protocol {

/// FSM events for app registration lifecycle
/// Used by both client and server implementations
enum class Event {
    /// Start connection attempt
    Connect,
    
    /// TCP connection established
    Connected,
    
    /// Handshake (HELLO) successful
    HandshakeComplete,
    
    /// Registration acknowledgment received
    RegistrationAck,
    
    /// Heartbeat timer expired
    HeartbeatTick,
    
    /// Heartbeat acknowledgment received
    HeartbeatAck,
    
    /// Connection lost
    Disconnected,
    
    /// Reconnection attempt scheduled
    Reconnect,
    
    /// Maximum reconnect attempts reached or fatal error
    FatalError,
    
    /// Manual shutdown requested
    Shutdown
};

/// Convert event to string for logging
[[nodiscard]] inline std::string to_string(Event event) noexcept {
    switch (event) {
        case Event::Connect: return "Connect";
        case Event::Connected: return "Connected";
        case Event::HandshakeComplete: return "HandshakeComplete";
        case Event::RegistrationAck: return "RegistrationAck";
        case Event::HeartbeatTick: return "HeartbeatTick";
        case Event::HeartbeatAck: return "HeartbeatAck";
        case Event::Disconnected: return "Disconnected";
        case Event::Reconnect: return "Reconnect";
        case Event::FatalError: return "FatalError";
        case Event::Shutdown: return "Shutdown";
    }
    return "Unknown";
}

} // namespace protoflow::app_registration_protocol
