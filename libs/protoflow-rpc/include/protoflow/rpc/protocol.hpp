#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>

namespace protoflow::rpc {

/// Generic RPC wire protocol framing
/// Protocol-agnostic - just handles message framing with command ID and payload

// Protocol magic number for RPC framing
inline constexpr uint32_t rpc_magic = 0x30435052; // 'RPC0' in little-endian

// RPC framing version
inline constexpr uint32_t rpc_framing_version = 1;

/// RPC wire header - protocol-agnostic framing
/// Each protocol defines its own command IDs and message structures
struct rpc_header {
    uint32_t magic;         // Protocol identifier ('RPC0')
    uint32_t version;       // Framing version
    uint16_t cmd;           // Command type (protocol-specific)
    uint16_t reserved;      // Reserved for future use
    uint32_t payload_size;  // Size of payload following header

    static constexpr size_t wire_size = 16;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return magic == rpc_magic;
    }

    [[nodiscard]] constexpr bool version_compatible() const noexcept {
        return version == rpc_framing_version;
    }
} __attribute__((packed));

static_assert(sizeof(rpc_header) == rpc_header::wire_size, 
              "rpc_header must be 16 bytes");

/// Helper to create a header for any protocol
[[nodiscard]] constexpr rpc_header make_header(uint16_t command, uint32_t payload_size) noexcept {
    return rpc_header{
        .magic = rpc_magic,
        .version = rpc_framing_version,
        .cmd = command,
        .reserved = 0,
        .payload_size = payload_size
    };
}

} // namespace protoflow::rpc
