#pragma once

#include "protocol.hpp"
#include <span>
#include <vector>
#include <cstddef>
#include <cstring>
#include <expected>
#include <string>

namespace protoflow::rpc {

// Forward declarations for transport abstraction
class transport_interface {
public:
    virtual ~transport_interface() = default;
    
    virtual bool send(std::span<const std::byte> data) = 0;
    virtual std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) = 0;
    virtual void close() = 0;
    virtual bool is_connected() const = 0;
};

// Server-side RPC base class
class rpc_server_base {
public:
    static constexpr uint32_t max_payload_size = 10 * 1024 * 1024; // 10 MB

    // Handle hello handshake
    [[nodiscard]] bool handle_hello(const protocol::rpc_header& hdr,
                                     std::span<const std::byte> payload,
                                     transport_interface& transport) {
        if (!hdr.version_compatible()) {
            (void)send_error(transport, 1, "Protocol version mismatch");
            transport.close();
            return false;
        }

        if (payload.size() < protocol::hello_msg::wire_size) {
            (void)send_error(transport, 2, "Invalid hello message size");
            return false;
        }

        return send_hello_ack(transport);
    }

    // Send hello acknowledgment
    [[nodiscard]] bool send_hello_ack(transport_interface& transport) {
        protocol::hello_ack ack{protocol::wire_version};
        return send_message(transport, protocol::cmd::hello_ack, 
                          std::span{reinterpret_cast<const std::byte*>(&ack), 
                                   protocol::hello_ack::wire_size});
    }

    // Send error message
    [[nodiscard]] bool send_error(transport_interface& transport,
                                   uint32_t error_code,
                                   std::string_view message) {
        protocol::error_msg err{};
        err.error_code = error_code;
        
        size_t len = std::min(message.size(), sizeof(err.message) - 1);
        std::memcpy(err.message, message.data(), len);
        err.message[len] = '\0';

        return send_message(transport, protocol::cmd::error,
                          std::span{reinterpret_cast<const std::byte*>(&err),
                                   protocol::error_msg::wire_size});
    }

    // Generic message sender
    [[nodiscard]] bool send_message(transport_interface& transport,
                                     protocol::cmd command,
                                     std::span<const std::byte> payload) {
        if (payload.size() > max_payload_size) {
            return false;
        }

        auto header = protocol::make_header(command, static_cast<uint32_t>(payload.size()));
        
        // Send header
        std::vector<std::byte> buffer;
        buffer.reserve(protocol::rpc_header::wire_size + payload.size());
        
        auto* hdr_bytes = reinterpret_cast<const std::byte*>(&header);
        buffer.insert(buffer.end(), hdr_bytes, hdr_bytes + protocol::rpc_header::wire_size);
        buffer.insert(buffer.end(), payload.begin(), payload.end());

        return transport.send(buffer);
    }

    // Receive and parse header
    [[nodiscard]] std::expected<protocol::rpc_header, std::string>
    receive_header(transport_interface& transport) {
        auto result = transport.receive(protocol::rpc_header::wire_size);
        if (!result) {
            return std::unexpected(result.error());
        }

        if (result->size() != protocol::rpc_header::wire_size) {
            return std::unexpected("Incomplete header received");
        }

        protocol::rpc_header header;
        std::memcpy(&header, result->data(), protocol::rpc_header::wire_size);

        if (!header.is_valid()) {
            return std::unexpected("Invalid magic number");
        }

        if (header.payload_size > max_payload_size) {
            return std::unexpected("Payload too large");
        }

        return header;
    }

    // Receive payload
    [[nodiscard]] std::expected<std::vector<std::byte>, std::string>
    receive_payload(transport_interface& transport, size_t size) {
        if (size == 0) {
            return std::vector<std::byte>{};
        }

        if (size > max_payload_size) {
            return std::unexpected("Payload size exceeds maximum");
        }

        return transport.receive(size);
    }
};

// Client-side RPC base class
class rpc_client_base {
public:
    static constexpr uint32_t max_payload_size = 10 * 1024 * 1024; // 10 MB

    // Send hello handshake
    [[nodiscard]] bool send_hello(transport_interface& transport) {
        protocol::hello_msg msg{protocol::wire_version};
        return send_message(transport, protocol::cmd::hello,
                          std::span{reinterpret_cast<const std::byte*>(&msg),
                                   protocol::hello_msg::wire_size});
    }

    // Send heartbeat
    [[nodiscard]] bool send_heartbeat(transport_interface& transport, uint64_t timestamp) {
        protocol::heartbeat_msg msg{timestamp};
        return send_message(transport, protocol::cmd::heartbeat,
                          std::span{reinterpret_cast<const std::byte*>(&msg),
                                   protocol::heartbeat_msg::wire_size});
    }

    // Generic message sender
    [[nodiscard]] bool send_message(transport_interface& transport,
                                     protocol::cmd command,
                                     std::span<const std::byte> payload) {
        if (payload.size() > max_payload_size) {
            return false;
        }

        auto header = protocol::make_header(command, static_cast<uint32_t>(payload.size()));
        
        std::vector<std::byte> buffer;
        buffer.reserve(protocol::rpc_header::wire_size + payload.size());
        
        auto* hdr_bytes = reinterpret_cast<const std::byte*>(&header);
        buffer.insert(buffer.end(), hdr_bytes, hdr_bytes + protocol::rpc_header::wire_size);
        buffer.insert(buffer.end(), payload.begin(), payload.end());

        return transport.send(buffer);
    }

    // Receive and parse header
    [[nodiscard]] std::expected<protocol::rpc_header, std::string>
    receive_header(transport_interface& transport) {
        auto result = transport.receive(protocol::rpc_header::wire_size);
        if (!result) {
            return std::unexpected(result.error());
        }

        if (result->size() != protocol::rpc_header::wire_size) {
            return std::unexpected("Incomplete header received");
        }

        protocol::rpc_header header;
        std::memcpy(&header, result->data(), protocol::rpc_header::wire_size);

        if (!header.is_valid()) {
            return std::unexpected("Invalid magic number");
        }

        if (!header.version_compatible()) {
            return std::unexpected("Protocol version mismatch");
        }

        if (header.payload_size > max_payload_size) {
            return std::unexpected("Payload too large");
        }

        return header;
    }

    // Receive payload
    [[nodiscard]] std::expected<std::vector<std::byte>, std::string>
    receive_payload(transport_interface& transport, size_t size) {
        if (size == 0) {
            return std::vector<std::byte>{};
        }

        if (size > max_payload_size) {
            return std::unexpected("Payload size exceeds maximum");
        }

        return transport.receive(size);
    }
};

} // namespace protoflow::rpc
