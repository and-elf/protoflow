#include <protoflow/rpc/rpc_server.hpp>
#include <cstring>
#include <algorithm>

namespace protoflow::rpc {

bool rpc_server_base::send_hello_ack(transport_interface& transport) {
    hello_ack msg{wire_version};
    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg),
                            hello_ack::wire_size};
    return send_with_header(transport, cmd::hello_ack, payload);
}

bool rpc_server_base::send_error(transport_interface& transport, uint32_t error_code,
                                 const std::string& message) {
    error_msg msg{};
    msg.error_code = error_code;
    
    size_t msg_len = std::min(message.size(), sizeof(msg.message) - 1);
    std::memcpy(msg.message, message.c_str(), msg_len);
    msg.message[msg_len] = '\0';
    
    auto payload = std::span{reinterpret_cast<const std::byte*>(&msg),
                            error_msg::wire_size};
    return send_with_header(transport, cmd::error, payload);
}

bool rpc_server_base::handle_hello(const rpc_header& hdr, std::span<const std::byte> payload,
                                   transport_interface& transport) {
    // Validate header version
    if (!hdr.version_compatible()) {
        transport.close();
        return false;
    }

    // Validate payload size
    if (hdr.payload_size != hello_msg::wire_size) {
        return false;
    }

    if (payload.size() != hello_msg::wire_size) {
        return false;
    }

    // Parse hello message
    hello_msg msg;
    std::memcpy(&msg, payload.data(), hello_msg::wire_size);

    // Check client version
    if (msg.version != wire_version) {
        transport.close();
        return false;
    }

    // Send acknowledgment
    return send_hello_ack(transport);
}

bool rpc_server_base::send_message(transport_interface& transport, cmd command,
                                   std::span<const std::byte> payload) {
    if (payload.size() > max_payload_size) {
        return false;
    }
    return send_with_header(transport, command, payload);
}

std::expected<rpc_header, std::string> rpc_server_base::receive_header(
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

std::expected<std::vector<std::byte>, std::string> rpc_server_base::receive_payload(
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

bool rpc_server_base::send_with_header(transport_interface& transport, cmd command,
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
