#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>

namespace protoflow::rpc::protocol {

// Protocol version
inline constexpr uint32_t wire_version = 1;

// Protocol magic number
inline constexpr uint32_t magic = 0x30435052; // 'RPC0' in little-endian

// Protocol commands
enum class cmd : uint16_t {
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

// RPC wire header
struct rpc_header {
    uint32_t magic;         // Protocol identifier ('RPC0')
    uint32_t version;       // Protocol version
    uint16_t cmd;           // Command type
    uint16_t reserved;      // Reserved for future use
    uint32_t payload_size;  // Size of payload following header

    static constexpr size_t wire_size = 16;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return magic == protocol::magic;
    }

    [[nodiscard]] constexpr bool version_compatible() const noexcept {
        return version == wire_version;
    }
} __attribute__((packed));

static_assert(sizeof(rpc_header) == rpc_header::wire_size, 
              "rpc_header must be 16 bytes");

// Protocol messages
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

// Helper to create a header
[[nodiscard]] constexpr rpc_header make_header(cmd command, uint32_t payload_size) noexcept {
    return rpc_header{
        .magic = magic,
        .version = wire_version,
        .cmd = static_cast<uint16_t>(command),
        .reserved = 0,
        .payload_size = payload_size
    };
}

} // namespace protoflow::rpc::protocol
