#include <protoflow/rpc/protocol.hpp>
#pragma once

#include <protoflow/rpc/protocol.hpp>
#include <expected>
#include <vector>
#include <cstddef>
#include <string>

namespace protoflow::rpc {

/// Base class for RPC clients
class rpc_client_base {
public:
    static constexpr size_t max_payload_size = protoflow::rpc::max_payload_size;

    virtual ~rpc_client_base() = default;

    /// Send hello message to server
    bool send_hello(transport_interface& transport);

    /// Send heartbeat message
    bool send_heartbeat(transport_interface& transport, uint64_t timestamp);

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
