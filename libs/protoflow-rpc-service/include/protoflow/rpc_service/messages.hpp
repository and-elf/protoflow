#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <protoflow/messaging/message.hpp>

namespace protoflow::rpc_service {

/// Unique identifier for RPC connections
using ConnectionId = uint64_t;

/// RPC message types
namespace RpcMessageTypes {
    constexpr protoflow::messaging::MessageType ConnectRequest = protoflow::messaging::MessageTypes::RpcBase + 1;
    constexpr protoflow::messaging::MessageType Connected = protoflow::messaging::MessageTypes::RpcBase + 2;
    constexpr protoflow::messaging::MessageType ConnectionFailed = protoflow::messaging::MessageTypes::RpcBase + 3;
    constexpr protoflow::messaging::MessageType SendRequest = protoflow::messaging::MessageTypes::RpcBase + 4;
    constexpr protoflow::messaging::MessageType Sent = protoflow::messaging::MessageTypes::RpcBase + 5;
    constexpr protoflow::messaging::MessageType SendFailed = protoflow::messaging::MessageTypes::RpcBase + 6;
    constexpr protoflow::messaging::MessageType Received = protoflow::messaging::MessageTypes::RpcBase + 7;
    constexpr protoflow::messaging::MessageType DisconnectRequest = protoflow::messaging::MessageTypes::RpcBase + 8;
    constexpr protoflow::messaging::MessageType Disconnected = protoflow::messaging::MessageTypes::RpcBase + 9;
    constexpr protoflow::messaging::MessageType Error = protoflow::messaging::MessageTypes::RpcBase + 10;
}

/// Request to establish a new RPC connection
struct RpcConnectRequest {
    ConnectionId connection_id;
    std::string host;
    uint16_t port;
    uint32_t connect_timeout_ms = 5000;
    uint32_t read_timeout_ms = 1000;
    uint32_t write_timeout_ms = 1000;
    
    std::vector<std::byte> serialize() const;
    static RpcConnectRequest deserialize(std::span<const std::byte> data);
};

/// Connection successfully established
struct RpcConnected {
    ConnectionId connection_id;
    
    std::vector<std::byte> serialize() const;
    static RpcConnected deserialize(std::span<const std::byte> data);
};

/// Connection failed
struct RpcConnectionFailed {
    ConnectionId connection_id;
    std::string error;
    
    std::vector<std::byte> serialize() const;
    static RpcConnectionFailed deserialize(std::span<const std::byte> data);
};

/// Request to send data over a connection
struct RpcSendRequest {
    ConnectionId connection_id;
    std::vector<std::byte> data;
    
    std::vector<std::byte> serialize() const;
    static RpcSendRequest deserialize(std::span<const std::byte> data);
};

/// Data successfully sent
struct RpcSent {
    ConnectionId connection_id;
    size_t bytes_sent;
    
    std::vector<std::byte> serialize() const;
    static RpcSent deserialize(std::span<const std::byte> data);
};

/// Send operation failed
struct RpcSendFailed {
    ConnectionId connection_id;
    std::string error;
    
    std::vector<std::byte> serialize() const;
    static RpcSendFailed deserialize(std::span<const std::byte> data);
};

/// Data received from connection
struct RpcReceived {
    ConnectionId connection_id;
    std::vector<std::byte> data;
    
    std::vector<std::byte> serialize() const;
    static RpcReceived deserialize(std::span<const std::byte> data);
};

/// Request to close a connection
struct RpcDisconnectRequest {
    ConnectionId connection_id;
    
    std::vector<std::byte> serialize() const;
    static RpcDisconnectRequest deserialize(std::span<const std::byte> data);
};

/// Connection closed
struct RpcDisconnected {
    ConnectionId connection_id;
    std::string reason;
    
    std::vector<std::byte> serialize() const;
    static RpcDisconnected deserialize(std::span<const std::byte> data);
};

/// Error on connection
struct RpcError {
    ConnectionId connection_id;
    std::string error;
    
    std::vector<std::byte> serialize() const;
    static RpcError deserialize(std::span<const std::byte> data);
};

} // namespace protoflow::rpc_service
