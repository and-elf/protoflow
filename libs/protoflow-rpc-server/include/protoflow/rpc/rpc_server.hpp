#pragma once

#include <protoflow/rpc/protocol.hpp>
#include <expected>
#include <vector>
#include <cstddef>
#include <string>

namespace protoflow::rpc {

/// Base class for RPC servers
class rpc_server_base {
public:
    static constexpr size_t max_payload_size = protoflow::rpc::max_payload_size;

    virtual ~rpc_server_base() = default;

    /// Send hello acknowledgment to client
    bool send_hello_ack(transport_interface& transport);

    /// Send error message to client
    bool send_error(transport_interface& transport, uint32_t error_code, 
                   const std::string& message);

    /// Handle hello message from client
    bool handle_hello(const rpc_header& hdr, std::span<const std::byte> payload,
                     transport_interface& transport);

    /// Send a generic message with command and payload
    bool send_message(transport_interface& transport, cmd command,
                     std::span<const std::byte> payload);

    /// Receive and validate RPC header
    std::expected<rpc_header, std::string> receive_header(transport_interface& transport);

    /// Receive payload of specified size
    std::expected<std::vector<std::byte>, std::string> receive_payload(
        transport_interface& transport, size_t size);

protected:
    /// Send raw data with header
    bool send_with_header(transport_interface& transport, cmd command,
                         std::span<const std::byte> payload);
};

} // namespace protoflow::rpc
