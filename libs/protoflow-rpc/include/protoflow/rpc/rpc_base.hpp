#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <string>
#include <expected>

namespace protoflow::rpc {

/// Generic transport interface for RPC communication
/// Abstracts away the underlying transport mechanism (TCP, Unix sockets, etc.)
class transport_interface {
public:
    virtual ~transport_interface() = default;
    
    /// Send data over the transport
    /// @param data Data to send
    /// @return true on success, false on failure
    virtual bool send(std::span<const std::byte> data) = 0;
    
    /// Receive data from the transport
    /// @param max_size Maximum number of bytes to receive
    /// @return Received data or error message
    virtual std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) = 0;
    
    /// Close the transport connection
    virtual void close() = 0;
    
    /// Check if transport is connected
    /// @return true if connected, false otherwise
    virtual bool is_connected() const = 0;
};

} // namespace protoflow::rpc
