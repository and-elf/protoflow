#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <cstring>
#include <vector>

namespace protoflow::hw_protocol {

// Hardware arbitration RPC commands (extends rpc::protocol::cmd)
enum class cmd : uint16_t {
    // Hardware access lifecycle
    request_hw_access = 100,
    hw_access_granted = 101,
    hw_access_denied = 102,
    hw_release = 103,
    hw_release_ack = 104,
    
    // Hardware I/O operations
    hw_write = 110,
    hw_write_ack = 111,
    hw_read = 112,
    hw_read_response = 113,
    
    // Hardware control operations
    hw_ioctl = 120,
    hw_ioctl_response = 121,
    
    // Status/error
    hw_timeout = 130,
    hw_error = 131
};

// Type aliases for compatibility
using hw_handle = uint32_t;

// Access modes
enum class access_mode : uint32_t {
    exclusive = 0,  // Only one client at a time
    shared = 1      // Multiple clients allowed
};

// Hardware access request
struct request_hw_access_msg {
    char resource[256];     // Hardware resource path (e.g., "/dev/ttyUSB0")
    uint32_t mode;          // access_mode
    uint32_t timeout_ms;    // Requested timeout in milliseconds (0 = no timeout)
    uint32_t flags;         // Reserved for future use
    
    static constexpr size_t wire_size = 268;
} __attribute__((packed));

static_assert(sizeof(request_hw_access_msg) == request_hw_access_msg::wire_size,
              "request_hw_access_msg size mismatch");

// Hardware access granted response
struct hw_access_granted_msg {
    uint32_t handle;        // Hardware resource handle for subsequent operations
    uint32_t timeout_ms;    // Granted timeout (may differ from requested)
    uint32_t capabilities;  // Bitmask of supported operations
    uint32_t reserved;
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_access_granted_msg) == hw_access_granted_msg::wire_size,
              "hw_access_granted_msg size mismatch");

// Hardware access denied response
struct hw_access_denied_msg {
    uint32_t reason_code;   // Reason for denial
    char reason[252];       // Human-readable reason
    
    static constexpr size_t wire_size = 256;
} __attribute__((packed));

static_assert(sizeof(hw_access_denied_msg) == hw_access_denied_msg::wire_size,
              "hw_access_denied_msg size mismatch");

// Hardware release request
struct hw_release_msg {
    uint32_t handle;        // Hardware resource handle to release
    uint32_t flags;         // Reserved
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(hw_release_msg) == hw_release_msg::wire_size,
              "hw_release_msg size mismatch");

// Hardware release acknowledgment
struct hw_release_ack_msg {
    uint32_t handle;        // Released handle
    uint32_t status;        // 0 = success
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(hw_release_ack_msg) == hw_release_ack_msg::wire_size,
              "hw_release_ack_msg size mismatch");

// Hardware write request
struct hw_write_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t offset;        // Offset for seekable devices (0 for non-seekable)
    uint32_t length;        // Number of bytes to write
    uint32_t flags;         // Reserved
    // Followed by data bytes
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_write_msg) == hw_write_msg::wire_size,
              "hw_write_msg size mismatch");

// Hardware write acknowledgment
struct hw_write_ack_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t bytes_written; // Number of bytes actually written
    uint32_t status;        // 0 = success
    uint32_t reserved;
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_write_ack_msg) == hw_write_ack_msg::wire_size,
              "hw_write_ack_msg size mismatch");

// Hardware read request
struct hw_read_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t offset;        // Offset for seekable devices (0 for non-seekable)
    uint32_t length;        // Maximum number of bytes to read
    uint32_t flags;         // Reserved
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_read_msg) == hw_read_msg::wire_size,
              "hw_read_msg size mismatch");

// Hardware read response
struct hw_read_response_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t bytes_read;    // Number of bytes actually read
    uint32_t status;        // 0 = success
    uint32_t reserved;
    // Followed by data bytes
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_read_response_msg) == hw_read_response_msg::wire_size,
              "hw_read_response_msg size mismatch");

// Hardware ioctl request
struct hw_ioctl_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t request;       // ioctl request code
    uint32_t arg_length;    // Length of argument data
    uint32_t flags;         // Reserved
    // Followed by argument data
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_ioctl_msg) == hw_ioctl_msg::wire_size,
              "hw_ioctl_msg size mismatch");

// Hardware ioctl response
struct hw_ioctl_response_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t result;        // ioctl result code
    uint32_t data_length;   // Length of response data
    uint32_t status;        // 0 = success
    // Followed by response data
    
    static constexpr size_t wire_size = 16;
} __attribute__((packed));

static_assert(sizeof(hw_ioctl_response_msg) == hw_ioctl_response_msg::wire_size,
              "hw_ioctl_response_msg size mismatch");

// Hardware error message
struct hw_error_msg {
    uint32_t handle;        // Hardware resource handle (0 if not applicable)
    uint32_t error_code;    // Error code
    char message[248];      // Error message
    
    static constexpr size_t wire_size = 256;
} __attribute__((packed));

static_assert(sizeof(hw_error_msg) == hw_error_msg::wire_size,
              "hw_error_msg size mismatch");

// Hardware timeout notification
struct hw_timeout_msg {
    uint32_t handle;        // Hardware resource handle
    uint32_t reserved;
    
    static constexpr size_t wire_size = 8;
} __attribute__((packed));

static_assert(sizeof(hw_timeout_msg) == hw_timeout_msg::wire_size,
              "hw_timeout_msg size mismatch");

// Error codes
enum class error_code : uint32_t {
    success = 0,
    resource_not_found = 1,
    resource_busy = 2,
    permission_denied = 3,
    invalid_handle = 4,
    timeout = 5,
    io_error = 6,
    invalid_operation = 7,
    buffer_too_large = 8,
    resource_released = 9
};

// Capability flags
enum class capability : uint32_t {
    read = 1 << 0,
    write = 1 << 1,
    ioctl = 1 << 2,
    seekable = 1 << 3
};

// Capability flags for bitwise operations
namespace capability_flags {
    constexpr uint32_t can_read = static_cast<uint32_t>(capability::read);
    constexpr uint32_t can_write = static_cast<uint32_t>(capability::write);
    constexpr uint32_t can_ioctl = static_cast<uint32_t>(capability::ioctl);
    constexpr uint32_t can_seek = static_cast<uint32_t>(capability::seekable);
}

// Protocol constants
constexpr size_t max_buffer_size = 1024 * 1024; // 1MB max transfer

// IOCTL result
struct ioctl_result {
    uint32_t result;              // ioctl return value
    std::vector<std::byte> data;  // response data
};

} // namespace protoflow::hw_protocol
