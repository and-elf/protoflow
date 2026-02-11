#include <protoflow/rpc/rpc_client.hpp>
#include <cstring>
#include <algorithm>

namespace protoflow::rpc {

bool rpc_client_base::send_hello(transport_interface& transport) {
    hello_msg msg{wire_version};
    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg), 
                            hello_msg::wire_size};
    return send_with_header(transport, cmd::hello, payload);
}

bool rpc_client_base::send_heartbeat(transport_interface& transport, uint64_t timestamp) {
    heartbeat_msg msg{timestamp};
    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg),
                            heartbeat_msg::wire_size};
    return send_with_header(transport, cmd::heartbeat, payload);
}

bool rpc_client_base::send_message(transport_interface& transport, cmd command,
                                   std::span<const std::byte> payload) {
    if (payload.size() > max_payload_size) {
        return false;
    }
    return send_with_header(transport, command, payload);
}

std::expected<rpc_header, std::string> rpc_client_base::receive_header(
    transport_interface& transport) {
    
    auto result = transport.receive(rpc_header::wire_size);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }

    if (result->size() != rpc_header::wire_size) {
        return std::unexpected("Incomplete header received");
    }

    rpc_header hdr;
    std::memcpy(&hdr, result->data(), rpc_header::wire_size);

    if (!hdr.is_valid()) {
        return std::unexpected("Invalid magic number in header");
    }

    if (!hdr.version_compatible()) {
        return std::unexpected("Incompatible protocol version");
    }

    if (hdr.payload_size > max_payload_size) {
        return std::unexpected("Payload size too large");
    }

    return hdr;
}

std::expected<std::vector<std::byte>, std::string> rpc_client_base::receive_payload(
    transport_interface& transport, size_t size) {
    
    if (size == 0) {
        return std::vector<std::byte>{};
    }

    if (size > max_payload_size) {
        return std::unexpected("Payload size exceeds maximum");
    }

    auto result = transport.receive(size);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }

    if (result->size() != size) {
        return std::unexpected("Incomplete payload received");
    }

    return result;
}

bool rpc_client_base::send_with_header(transport_interface& transport, cmd command,
                                       std::span<const std::byte> payload) {
    if (payload.size() > max_payload_size) {
        return false;
    }

    auto hdr = make_header(command, static_cast<uint32_t>(payload.size()));
    
    // Send header
    auto hdr_bytes = std::span{reinterpret_cast<const std::byte*>(&hdr), 
                              rpc_header::wire_size};
    if (!transport.send(hdr_bytes)) {
        return false;
    }

    // Send payload if present
    if (!payload.empty()) {
        if (!transport.send(payload)) {
            return false;
        }
    }

    return true;
}

} // namespace protoflow::rpc
