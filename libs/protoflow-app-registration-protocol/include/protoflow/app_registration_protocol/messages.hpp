#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <protoflow/messaging/message.hpp>

namespace protoflow::app_registration_protocol { /// Application registration protocol commands 
enum class request : protoflow::messaging::MessageType {
     hello = protoflow::messaging::MessageTypes::AppRegistrationRequests
    , register_app
    , heartbeat
    , unregister_app
    , error
};
enum class response: protoflow::messaging::MessageType {
     hello_ack = protoflow::messaging::MessageTypes::AppRegistrationResponses
    , register_ack
    , heartbeat_ack
    , unregister_app_ack
    , error
};
/// Convert request to string for logging
[[nodiscard]] constexpr std::string_view to_string(request cmd) noexcept {
    switch (cmd) {
        case request::hello: return "hello";
        case request::register_app: return "register_app";
        case request::heartbeat: return "heartbeat";
        case request::error: return "error";
        default: return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view to_string(response cmd) noexcept {
    switch (cmd) {
        case response::hello_ack: return "hello_ack";
        case response::register_ack: return "register_ack";
        case response::heartbeat_ack: return "heartbeat_ack";
        case response::unregister_app_ack: return "unregister_app_ack";
        case response::error: return "error";
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
