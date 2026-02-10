#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace protoflow::app_registration_protocol {

/// Application registration protocol commands
enum class command : uint16_t {
    hello = 1,
    hello_ack = 2,
    register_app = 3,
    register_ack = 4,
    heartbeat = 5,
    heartbeat_ack = 6,
    render_fragment = 7,
    fragment_data = 8,
    get_state = 9,
    state_json = 10,
    error = 255
};

/// Convert command to string for logging
[[nodiscard]] constexpr std::string_view to_string(command cmd) noexcept {
    switch (cmd) {
        case command::hello: return "hello";
        case command::hello_ack: return "hello_ack";
        case command::register_app: return "register_app";
        case command::register_ack: return "register_ack";
        case command::heartbeat: return "heartbeat";
        case command::heartbeat_ack: return "heartbeat_ack";
        case command::render_fragment: return "render_fragment";
        case command::fragment_data: return "fragment_data";
        case command::get_state: return "get_state";
        case command::state_json: return "state_json";
        case command::error: return "error";
        default: return "unknown";
    }
}

// Protocol wire messages
struct hello_msg {
    uint32_t version;
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

struct hello_ack {
    uint32_t version;
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

struct register_app_msg {
    char name[64];
    uint32_t endpoint_count;
    // Followed by null-terminated endpoint strings
    
    static constexpr size_t wire_size = 68;
} __attribute__((packed));

struct register_ack {
    uint32_t app_id;
    uint32_t status; // 0 = success, non-zero = error code
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

struct heartbeat_msg {
    uint64_t timestamp;
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

struct heartbeat_ack {
    uint64_t timestamp;
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

struct render_fragment_msg {
    char fragment_id[128];
    
    static constexpr size_t wire_size = 128;
} __attribute__((packed));

struct get_state_msg {
    uint32_t flags; // Reserved for filtering options
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

struct error_msg {
    uint32_t error_code;
    char message[256];
    
    static constexpr size_t wire_size = 260;
} __attribute__((packed));

} // namespace protoflow::app_registration_protocol
