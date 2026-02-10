#pragma once

#include "protocol.hpp"
#include <protoflow/rpc/rpc_base.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <expected>
#include <string>
#include <vector>
#include <span>
#include <chrono>
#include <cstring>

namespace protoflow::hw {

// Hardware resource handle wrapper with RAII semantics
class hw_handle {
public:
    explicit hw_handle(uint32_t id = 0) noexcept : id_(id) {}
    
    [[nodiscard]] constexpr uint32_t id() const noexcept { return id_; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return id_ != 0; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return is_valid(); }
    
private:
    uint32_t id_;
};

// Hardware access result
struct hw_access_result {
    hw_handle handle;
    std::chrono::milliseconds timeout;
    uint32_t capabilities;
};

// Hardware client errors
enum class hw_error {
    transport_error,
    protocol_error,
    resource_not_found,
    resource_busy,
    permission_denied,
    invalid_handle,
    timeout,
    io_error,
    invalid_operation,
    buffer_too_large,
    resource_released
};

[[nodiscard]] inline std::string to_string(hw_error err) {
    switch (err) {
        case hw_error::transport_error: return "Transport error";
        case hw_error::protocol_error: return "Protocol error";
        case hw_error::resource_not_found: return "Resource not found";
        case hw_error::resource_busy: return "Resource busy";
        case hw_error::permission_denied: return "Permission denied";
        case hw_error::invalid_handle: return "Invalid handle";
        case hw_error::timeout: return "Timeout";
        case hw_error::io_error: return "I/O error";
        case hw_error::invalid_operation: return "Invalid operation";
        case hw_error::buffer_too_large: return "Buffer too large";
        case hw_error::resource_released: return "Resource released";
    }
    return "Unknown error";
}

// Hardware arbitration client
class hw_client {
public:
    explicit hw_client(rpc::transport_interface& transport) 
        : transport_(transport) {}

    // Request hardware access
    [[nodiscard]] std::expected<hw_access_result, hw_error>
    request_access(std::string_view resource,
                   protocol::access_mode mode = protocol::access_mode::exclusive,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) {
        
        protocol::request_hw_access_msg msg{};
        msg.mode = static_cast<uint32_t>(mode);
        msg.timeout_ms = static_cast<uint32_t>(timeout.count());
        msg.flags = 0;
        
        // Copy resource path
        size_t len = std::min(resource.size(), sizeof(msg.resource) - 1);
        std::memcpy(msg.resource, resource.data(), len);
        msg.resource[len] = '\0';
        
        // Send request
        auto header = rpc::protocol::make_header(
            static_cast<rpc::protocol::cmd>(protocol::cmd::request_hw_access),
            protocol::request_hw_access_msg::wire_size
        );
        
        if (!send_with_header(header, std::span{
                reinterpret_cast<const std::byte*>(&msg),
                protocol::request_hw_access_msg::wire_size})) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Receive response
        auto resp_header = receive_header();
        if (!resp_header) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Check response type
        auto resp_cmd = static_cast<protocol::cmd>(resp_header->cmd);
        
        if (resp_cmd == protocol::cmd::hw_access_granted) {
            auto payload = receive_payload(resp_header->payload_size);
            if (!payload || payload->size() < protocol::hw_access_granted_msg::wire_size) {
                return std::unexpected(hw_error::protocol_error);
            }
            
            auto* grant = reinterpret_cast<const protocol::hw_access_granted_msg*>(payload->data());
            return hw_access_result{
                .handle = hw_handle{grant->handle},
                .timeout = std::chrono::milliseconds{grant->timeout_ms},
                .capabilities = grant->capabilities
            };
        } else if (resp_cmd == protocol::cmd::hw_access_denied) {
            auto payload = receive_payload(resp_header->payload_size);
            if (!payload || payload->size() < protocol::hw_access_denied_msg::wire_size) {
                return std::unexpected(hw_error::protocol_error);
            }
            
            auto* denied = reinterpret_cast<const protocol::hw_access_denied_msg*>(payload->data());
            return std::unexpected(map_error_code(denied->reason_code));
        } else {
            return std::unexpected(hw_error::protocol_error);
        }
    }

    // Release hardware access
    [[nodiscard]] std::expected<void, hw_error>
    release(hw_handle handle) {
        if (!handle.is_valid()) {
            return std::unexpected(hw_error::invalid_handle);
        }
        
        protocol::hw_release_msg msg{};
        msg.handle = handle.id();
        msg.flags = 0;
        
        auto header = rpc::make_header(
            static_cast<uint16_t>(protocol::cmd::hw_read),
            protocol::hw_release_msg::wire_size
        );
        
        if (!send_with_header(header, std::span{
                reinterpret_cast<const std::byte*>(&msg),
                protocol::hw_release_msg::wire_size})) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Wait for acknowledgment
        auto resp_header = receive_header();
        if (!resp_header) {
            return std::unexpected(hw_error::transport_error);
        }
        
        auto resp_cmd = static_cast<protocol::cmd>(resp_header->cmd);
        if (resp_cmd != protocol::cmd::hw_release_ack) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto payload = receive_payload(resp_header->payload_size);
        if (!payload || payload->size() < protocol::hw_release_ack_msg::wire_size) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto* ack = reinterpret_cast<const protocol::hw_release_ack_msg*>(payload->data());
        if (ack->status != 0) {
            return std::unexpected(hw_error::io_error);
        }
        
        return {};
    }

    // Write to hardware
    [[nodiscard]] std::expected<size_t, hw_error>
    write(hw_handle handle, std::span<const std::byte> data, uint32_t offset = 0) {
        if (!handle.is_valid()) {
            return std::unexpected(hw_error::invalid_handle);
        }
        
        if (data.size() > max_io_size) {
            return std::unexpected(hw_error::buffer_too_large);
        }
        
        protocol::hw_write_msg msg{};
        msg.handle = handle.id();
        msg.offset = offset;
        msg.length = static_cast<uint32_t>(data.size());
        msg.flags = 0;
        
        // Build payload: msg + data
        std::vector<std::byte> payload;
        payload.reserve(protocol::hw_write_msg::wire_size + data.size());
        
        auto* msg_bytes = reinterpret_cast<const std::byte*>(&msg);
        payload.insert(payload.end(), msg_bytes, msg_bytes + protocol::hw_write_msg::wire_size);
        payload.insert(payload.end(), data.begin(), data.end());
        
        auto header = rpc::protocol::make_header(
            static_cast<rpc::protocol::cmd>(protocol::cmd::hw_write),
            static_cast<uint32_t>(payload.size())
        );
        
        if (!send_with_header(header, payload)) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Wait for acknowledgment
        auto resp_header = receive_header();
        if (!resp_header) {
            return std::unexpected(hw_error::transport_error);
        }
        
        auto resp_cmd = static_cast<protocol::cmd>(resp_header->cmd);
        if (resp_cmd != protocol::cmd::hw_write_ack) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto resp_payload = receive_payload(resp_header->payload_size);
        if (!resp_payload || resp_payload->size() < protocol::hw_write_ack_msg::wire_size) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto* ack = reinterpret_cast<const protocol::hw_write_ack_msg*>(resp_payload->data());
        if (ack->status != 0) {
            return std::unexpected(hw_error::io_error);
        }
        
        return ack->bytes_written;
    }

    // Read from hardware
    [[nodiscard]] std::expected<std::vector<std::byte>, hw_error>
    read(hw_handle handle, size_t max_length, uint32_t offset = 0) {
        if (!handle.is_valid()) {
            return std::unexpected(hw_error::invalid_handle);
        }
        
        if (max_length > max_io_size) {
            return std::unexpected(hw_error::buffer_too_large);
        }
        
        protocol::hw_read_msg msg{};
        msg.handle = handle.id();
        msg.offset = offset;
        msg.length = static_cast<uint32_t>(max_length);
        msg.flags = 0;
        
        auto header = rpc::make_header(
            static_cast<uint16_t>(protocol::cmd::hw_read),
            protocol::hw_read_msg::wire_size
        );
        
        if (!send_with_header(header, std::span{
                reinterpret_cast<const std::byte*>(&msg),
                protocol::hw_read_msg::wire_size})) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Wait for response
        auto resp_header = receive_header();
        if (!resp_header) {
            return std::unexpected(hw_error::transport_error);
        }
        
        auto resp_cmd = static_cast<protocol::cmd>(resp_header->cmd);
        if (resp_cmd != protocol::cmd::hw_read_response) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto payload = receive_payload(resp_header->payload_size);
        if (!payload || payload->size() < protocol::hw_read_response_msg::wire_size) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto* resp = reinterpret_cast<const protocol::hw_read_response_msg*>(payload->data());
        if (resp->status != 0) {
            return std::unexpected(hw_error::io_error);
        }
        
        // Extract data
        size_t data_offset = protocol::hw_read_response_msg::wire_size;
        if (payload->size() < data_offset + resp->bytes_read) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        std::vector<std::byte> data(
            payload->begin() + static_cast<std::ptrdiff_t>(data_offset),
            payload->begin() + static_cast<std::ptrdiff_t>(data_offset + resp->bytes_read)
        );
        
        return data;
    }

    // Perform ioctl operation
    [[nodiscard]] std::expected<std::vector<std::byte>, hw_error>
    ioctl(hw_handle handle, uint32_t request, std::span<const std::byte> arg = {}) {
        if (!handle.is_valid()) {
            return std::unexpected(hw_error::invalid_handle);
        }
        
        if (arg.size() > max_io_size) {
            return std::unexpected(hw_error::buffer_too_large);
        }
        
        protocol::hw_ioctl_msg msg{};
        msg.handle = handle.id();
        msg.request = request;
        msg.arg_length = static_cast<uint32_t>(arg.size());
        msg.flags = 0;
        
        // Build payload: msg + arg
        std::vector<std::byte> payload;
        payload.reserve(protocol::hw_ioctl_msg::wire_size + arg.size());
        
        auto* msg_bytes = reinterpret_cast<const std::byte*>(&msg);
        payload.insert(payload.end(), msg_bytes, msg_bytes + protocol::hw_ioctl_msg::wire_size);
        payload.insert(payload.end(), arg.begin(), arg.end());
        
        auto header = rpc::make_header(
            static_cast<uint16_t>(protocol::cmd::hw_ioctl),
            static_cast<uint32_t>(payload.size())
        );
        
        if (!send_with_header(header, payload)) {
            return std::unexpected(hw_error::transport_error);
        }
        
        // Wait for response
        auto resp_header = receive_header();
        if (!resp_header) {
            return std::unexpected(hw_error::transport_error);
        }
        
        auto resp_cmd = static_cast<protocol::cmd>(resp_header->cmd);
        if (resp_cmd != protocol::cmd::hw_ioctl_response) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto resp_payload = receive_payload(resp_header->payload_size);
        if (!resp_payload || resp_payload->size() < protocol::hw_ioctl_response_msg::wire_size) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        auto* resp = reinterpret_cast<const protocol::hw_ioctl_response_msg*>(resp_payload->data());
        if (resp->status != 0) {
            return std::unexpected(hw_error::io_error);
        }
        
        // Extract response data
        size_t data_offset = protocol::hw_ioctl_response_msg::wire_size;
        if (resp_payload->size() < data_offset + resp->data_length) {
            return std::unexpected(hw_error::protocol_error);
        }
        
        std::vector<std::byte> data(
            resp_payload->begin() + static_cast<std::ptrdiff_t>(data_offset),
            resp_payload->begin() + static_cast<std::ptrdiff_t>(data_offset + resp->data_length)
        );
        
        return data;
    }

private:
    static constexpr size_t max_io_size = 1024 * 1024; // 1 MB
    
    rpc::transport_interface& transport_;
    
    [[nodiscard]] bool send_with_header(const rpc::protocol::rpc_header& header,
                                         std::span<const std::byte> payload) {
        std::vector<std::byte> buffer;
        buffer.reserve(rpc::protocol::rpc_header::wire_size + payload.size());
        
        auto* hdr_bytes = reinterpret_cast<const std::byte*>(&header);
        buffer.insert(buffer.end(), hdr_bytes, hdr_bytes + rpc::protocol::rpc_header::wire_size);
        buffer.insert(buffer.end(), payload.begin(), payload.end());
        
        return transport_.send(buffer);
    }
    
    [[nodiscard]] std::expected<rpc::protocol::rpc_header, std::string>
    receive_header() {
        auto result = transport_.receive(rpc::protocol::rpc_header::wire_size);
        if (!result) {
            return std::unexpected(result.error());
        }
        
        if (result->size() != rpc::protocol::rpc_header::wire_size) {
            return std::unexpected("Invalid header size");
        }
        
        rpc::protocol::rpc_header header;
        std::memcpy(&header, result->data(), rpc::protocol::rpc_header::wire_size);
        
        if (!header.is_valid()) {
            return std::unexpected("Invalid header magic");
        }
        
        return header;
    }
    
    [[nodiscard]] std::expected<std::vector<std::byte>, std::string>
    receive_payload(size_t size) {
        if (size == 0) {
            return std::vector<std::byte>{};
        }
        
        return transport_.receive(size);
    }
    
    [[nodiscard]] static hw_error map_error_code(uint32_t code) {
        switch (static_cast<protocol::error_code>(code)) {
            case protocol::error_code::resource_not_found:
                return hw_error::resource_not_found;
            case protocol::error_code::resource_busy:
                return hw_error::resource_busy;
            case protocol::error_code::permission_denied:
                return hw_error::permission_denied;
            case protocol::error_code::invalid_handle:
                return hw_error::invalid_handle;
            case protocol::error_code::timeout:
                return hw_error::timeout;
            case protocol::error_code::io_error:
                return hw_error::io_error;
            case protocol::error_code::invalid_operation:
                return hw_error::invalid_operation;
            case protocol::error_code::buffer_too_large:
                return hw_error::buffer_too_large;
            case protocol::error_code::resource_released:
                return hw_error::resource_released;
            default:
                return hw_error::protocol_error;
        }
    }
};

// RAII wrapper for automatic hardware resource release
class scoped_hw_access {
public:
    scoped_hw_access(hw_client& client, hw_handle handle)
        : client_(client), handle_(handle) {}
    
    ~scoped_hw_access() {
        if (handle_.is_valid()) {
            (void)client_.release(handle_);
        }
    }
    
    // No copy
    scoped_hw_access(const scoped_hw_access&) = delete;
    scoped_hw_access& operator=(const scoped_hw_access&) = delete;
    
    // Move allowed
    scoped_hw_access(scoped_hw_access&& other) noexcept
        : client_(other.client_), handle_(other.handle_) {
        other.handle_ = hw_handle{0};
    }
    
    scoped_hw_access& operator=(scoped_hw_access&& other) noexcept {
        if (this != &other) {
            if (handle_.is_valid()) {
                (void)client_.release(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = hw_handle{0};
        }
        return *this;
    }
    
    [[nodiscard]] hw_handle handle() const noexcept { return handle_; }
    [[nodiscard]] hw_handle* operator->() noexcept { return &handle_; }
    [[nodiscard]] const hw_handle* operator->() const noexcept { return &handle_; }

private:
    hw_client& client_;
    hw_handle handle_;
};

} // namespace protoflow::hw
