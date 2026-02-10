#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace protoflow::app_registration_client {

/// Configuration for app registration client
struct Config {
    /// Application name (unique identifier)
    std::string app_name;
    
    /// Protocol version number (must match server)
    uint32_t version = 1;
    
    /// List of endpoints this app provides
    std::vector<std::string> endpoints;
    
    /// Server address to connect to
    std::string server_address = "localhost";
    
    /// Server port to connect to
    uint16_t server_port = 8080;
    
    /// Heartbeat interval (how often to send heartbeat messages)
    std::chrono::seconds heartbeat_interval{30};
    
    /// Connection timeout
    std::chrono::milliseconds connection_timeout{10000};
    
    /// Read timeout for RPC operations
    std::chrono::milliseconds read_timeout{5000};
    
    /// Write timeout for RPC operations
    std::chrono::milliseconds write_timeout{5000};
    
    /// Maximum reconnection attempts (0 = infinite)
    uint32_t max_reconnect_attempts = 0;
    
    /// Delay between reconnection attempts
    std::chrono::seconds reconnect_delay{5};
    
    /// Validate configuration
    [[nodiscard]] bool is_valid() const noexcept {
        return !app_name.empty() && 
               !endpoints.empty() && 
               !server_address.empty() &&
               server_port > 0;
    }
};

} // namespace protoflow::app_registration_client
