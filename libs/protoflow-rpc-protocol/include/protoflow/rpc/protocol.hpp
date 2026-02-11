#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>
#include <expected>
#include <string>

namespace protoflow::rpc {

/// RPC wire protocol version
inline constexpr uint32_t wire_version = 1;

/// Protocol magic number
inline constexpr uint32_t magic = 0x30435052; // 'RPC0' in little-endian

/// Maximum payload size (16MB)
inline constexpr uint32_t max_payload_size = 16 * 1024 * 1024;

/// RPC command types
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
    error = 255,
};

/// RPC wire header - fixed 16 bytes
struct rpc_header {
    uint32_t magic;         // Protocol identifier ('RPC0')
    uint32_t version;       // Wire protocol version
    uint16_t cmd;           // Command type
    uint16_t reserved;      // Reserved for future use
    uint32_t payload_size;  // Size of payload following header

    static constexpr size_t wire_size = 16;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return magic == protoflow::rpc::magic;
    }

    [[nodiscard]] constexpr bool version_compatible() const noexcept {
        return version == wire_version;
    }
} __attribute__((packed));

static_assert(sizeof(rpc_header) == rpc_header::wire_size, 
              "rpc_header must be 16 bytes");

/// Helper to create a header
[[nodiscard]] constexpr rpc_header make_header(cmd command, uint32_t payload_size) noexcept {
    return rpc_header{
        .magic = magic,
        .version = wire_version,
        .cmd = static_cast<uint16_t>(command),
        .reserved = 0,
        .payload_size = payload_size
    };
}

/// Hello message - sent by client to initiate connection
struct hello_msg {
    uint32_t version;       // Client protocol version
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

static_assert(sizeof(hello_msg) == hello_msg::wire_size,
              "hello_msg must be 4 bytes");

/// Hello acknowledgment - sent by server in response to hello
struct hello_ack {
    uint32_t server_version; // Server protocol version
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

static_assert(sizeof(hello_ack) == hello_ack::wire_size,
              "hello_ack must be 4 bytes");

/// Register app message - app registers with server
struct register_app_msg {
    char name[64];          // App name (null-terminated)
    uint32_t endpoint_count; // Number of endpoints
    
    static constexpr size_t wire_size = 68;
} __attribute__((packed));

static_assert(sizeof(register_app_msg) == register_app_msg::wire_size,
              "register_app_msg must be 68 bytes");

/// Register acknowledgment - server confirms registration
struct register_ack {
    uint32_t app_id;        // Assigned app ID
    uint32_t status;        // Registration status (0 = success)
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(register_ack) == register_ack::wire_size,
              "register_ack must be 8 bytes");

/// Heartbeat message
struct heartbeat_msg {
    uint64_t timestamp;     // Timestamp of heartbeat
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(heartbeat_msg) == heartbeat_msg::wire_size,
              "heartbeat_msg must be 8 bytes");

/// Heartbeat acknowledgment
struct heartbeat_ack {
    uint64_t timestamp;     // Server timestamp
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(heartbeat_ack) == heartbeat_ack::wire_size,
              "heartbeat_ack must be 8 bytes");

/// Render fragment message - request to render HTML fragment
struct render_fragment_msg {
    char fragment_id[64];   // Fragment identifier (null-terminated)
    char data[64];          // Fragment data (null-terminated)
    
    static constexpr size_t wire_size = 128;
} __attribute__((packed));

static_assert(sizeof(render_fragment_msg) == render_fragment_msg::wire_size,
              "render_fragment_msg must be 128 bytes");

/// Get state message - request app state
struct get_state_msg {
    uint32_t reserved;      // Reserved for future use
    
    static constexpr size_t wire_size = 4;
} __attribute__((packed));

static_assert(sizeof(get_state_msg) == get_state_msg::wire_size,
              "get_state_msg must be 4 bytes");

/// Error message
struct error_msg {
    uint32_t error_code;    // Error code
    char message[256];      // Error message (null-terminated)
    
    static constexpr size_t wire_size = 260;
} __attribute__((packed));

static_assert(sizeof(error_msg) == error_msg::wire_size,
              "error_msg must be 260 bytes");

/// Generic transport interface
class transport_interface {
public:
    virtual ~transport_interface() = default;
    
    /// Send data over the transport
    virtual bool send(std::span<const std::byte> data) = 0;
    
    /// Receive data from the transport
    virtual std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) = 0;
    
    /// Close the transport
    virtual void close() = 0;
    
    /// Check if transport is connected
    virtual bool is_connected() const = 0;

    virtual std::unique_ptr<rpc::transport_interface> accept() = 0;
};

} // namespace protoflow::rpc
