#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstdint>
#include <cstddef>

namespace protoflow::rpc {

// Application registration information
struct app_registration {
    std::string name;
    std::vector<std::string> endpoints;
    std::string description;
};

// Abstract interface for RPC-enabled applications
class rpc_app {
public:
    virtual ~rpc_app() = default;

    // Return application registration information
    [[nodiscard]] virtual app_registration registration() const = 0;

    // Render an HTML fragment by ID
    // Returns empty string if fragment_id is not recognized
    [[nodiscard]] virtual std::string render_fragment(std::string_view fragment_id) = 0;

    // Return current application state as JSON string
    [[nodiscard]] virtual std::string get_state_json() const = 0;

    // Handle custom RPC commands (optional override)
    // Returns true if command was handled, false otherwise
    [[nodiscard]] virtual bool handle_custom_command(
        uint16_t command,
        std::span<const std::byte> payload,
        std::vector<std::byte>& response) {
        (void)command;
        (void)payload;
        (void)response;
        return false;
    }
};

} // namespace protoflow::rpc
