#include <protoflow/rpc_service/messages.hpp>
#include <cstring>

namespace protoflow::rpc_service {

namespace {
    // Helper to serialize string
    void serialize_string(std::vector<std::byte>& out, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        auto len_bytes = reinterpret_cast<const std::byte*>(&len);
        out.insert(out.end(), len_bytes, len_bytes + sizeof(len));
        auto str_bytes = reinterpret_cast<const std::byte*>(str.data());
        out.insert(out.end(), str_bytes, str_bytes + str.size());
    }
    
    // Helper to deserialize string
    std::string deserialize_string(std::span<const std::byte>& data) {
        if (data.size() < sizeof(uint32_t)) return "";
        uint32_t len;
        std::memcpy(&len, data.data(), sizeof(len));
        data = data.subspan(sizeof(len));
        
        if (data.size() < len) return "";
        std::string result(reinterpret_cast<const char*>(data.data()), len);
        data = data.subspan(len);
        return result;
    }
}

// RpcConnectRequest
std::vector<std::byte> RpcConnectRequest::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    
    serialize_string(result, host);
    
    auto port_bytes = reinterpret_cast<const std::byte*>(&port);
    result.insert(result.end(), port_bytes, port_bytes + sizeof(port));
    
    auto ct_bytes = reinterpret_cast<const std::byte*>(&connect_timeout_ms);
    result.insert(result.end(), ct_bytes, ct_bytes + sizeof(connect_timeout_ms));
    
    auto rt_bytes = reinterpret_cast<const std::byte*>(&read_timeout_ms);
    result.insert(result.end(), rt_bytes, rt_bytes + sizeof(read_timeout_ms));
    
    auto wt_bytes = reinterpret_cast<const std::byte*>(&write_timeout_ms);
    result.insert(result.end(), wt_bytes, wt_bytes + sizeof(write_timeout_ms));
    
    return result;
}

RpcConnectRequest RpcConnectRequest::deserialize(std::span<const std::byte> data) {
    RpcConnectRequest req;
    if (data.size() < sizeof(ConnectionId)) return req;
    
    std::memcpy(&req.connection_id, data.data(), sizeof(req.connection_id));
    data = data.subspan(sizeof(req.connection_id));
    
    req.host = deserialize_string(data);
    
    if (data.size() < sizeof(uint16_t)) return req;
    std::memcpy(&req.port, data.data(), sizeof(req.port));
    data = data.subspan(sizeof(req.port));
    
    if (data.size() < sizeof(uint32_t)) return req;
    std::memcpy(&req.connect_timeout_ms, data.data(), sizeof(req.connect_timeout_ms));
    data = data.subspan(sizeof(req.connect_timeout_ms));
    
    if (data.size() < sizeof(uint32_t)) return req;
    std::memcpy(&req.read_timeout_ms, data.data(), sizeof(req.read_timeout_ms));
    data = data.subspan(sizeof(req.read_timeout_ms));
    
    if (data.size() < sizeof(uint32_t)) return req;
    std::memcpy(&req.write_timeout_ms, data.data(), sizeof(req.write_timeout_ms));
    
    return req;
}

// RpcConnected
std::vector<std::byte> RpcConnected::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    return result;
}

RpcConnected RpcConnected::deserialize(std::span<const std::byte> data) {
    RpcConnected msg;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
    }
    return msg;
}

// RpcConnectionFailed
std::vector<std::byte> RpcConnectionFailed::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    serialize_string(result, error);
    return result;
}

RpcConnectionFailed RpcConnectionFailed::deserialize(std::span<const std::byte> data) {
    RpcConnectionFailed msg;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
        data = data.subspan(sizeof(msg.connection_id));
        msg.error = deserialize_string(data);
    }
    return msg;
}

// RpcSendRequest
std::vector<std::byte> RpcSendRequest::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    
    uint32_t len = static_cast<uint32_t>(data.size());
    auto len_bytes = reinterpret_cast<const std::byte*>(&len);
    result.insert(result.end(), len_bytes, len_bytes + sizeof(len));
    result.insert(result.end(), data.begin(), data.end());
    
    return result;
}

RpcSendRequest RpcSendRequest::deserialize(std::span<const std::byte> data) {
    RpcSendRequest req;
    if (data.size() < sizeof(ConnectionId)) return req;
    
    std::memcpy(&req.connection_id, data.data(), sizeof(req.connection_id));
    data = data.subspan(sizeof(req.connection_id));
    
    if (data.size() < sizeof(uint32_t)) return req;
    uint32_t len;
    std::memcpy(&len, data.data(), sizeof(len));
    data = data.subspan(sizeof(len));
    
    if (data.size() >= len) {
        req.data.assign(data.begin(), data.begin() + len);
    }
    return req;
}

// RpcSent
std::vector<std::byte> RpcSent::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    
    auto size_bytes = reinterpret_cast<const std::byte*>(&bytes_sent);
    result.insert(result.end(), size_bytes, size_bytes + sizeof(bytes_sent));
    
    return result;
}

RpcSent RpcSent::deserialize(std::span<const std::byte> data) {
    RpcSent msg;
    if (data.size() >= sizeof(ConnectionId) + sizeof(size_t)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
        data = data.subspan(sizeof(msg.connection_id));
        std::memcpy(&msg.bytes_sent, data.data(), sizeof(msg.bytes_sent));
    }
    return msg;
}

// RpcSendFailed
std::vector<std::byte> RpcSendFailed::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    serialize_string(result, error);
    return result;
}

RpcSendFailed RpcSendFailed::deserialize(std::span<const std::byte> data) {
    RpcSendFailed msg;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
        data = data.subspan(sizeof(msg.connection_id));
        msg.error = deserialize_string(data);
    }
    return msg;
}

// RpcReceived
std::vector<std::byte> RpcReceived::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    
    uint32_t len = static_cast<uint32_t>(data.size());
    auto len_bytes = reinterpret_cast<const std::byte*>(&len);
    result.insert(result.end(), len_bytes, len_bytes + sizeof(len));
    result.insert(result.end(), data.begin(), data.end());
    
    return result;
}

RpcReceived RpcReceived::deserialize(std::span<const std::byte> data) {
    RpcReceived msg;
    if (data.size() < sizeof(ConnectionId)) return msg;
    
    std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
    data = data.subspan(sizeof(msg.connection_id));
    
    if (data.size() < sizeof(uint32_t)) return msg;
    uint32_t len;
    std::memcpy(&len, data.data(), sizeof(len));
    data = data.subspan(sizeof(len));
    
    if (data.size() >= len) {
        msg.data.assign(data.begin(), data.begin() + len);
    }
    return msg;
}

// RpcDisconnectRequest
std::vector<std::byte> RpcDisconnectRequest::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    return result;
}

RpcDisconnectRequest RpcDisconnectRequest::deserialize(std::span<const std::byte> data) {
    RpcDisconnectRequest req;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&req.connection_id, data.data(), sizeof(req.connection_id));
    }
    return req;
}

// RpcDisconnected
std::vector<std::byte> RpcDisconnected::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    serialize_string(result, reason);
    return result;
}

RpcDisconnected RpcDisconnected::deserialize(std::span<const std::byte> data) {
    RpcDisconnected msg;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
        data = data.subspan(sizeof(msg.connection_id));
        msg.reason = deserialize_string(data);
    }
    return msg;
}

// RpcError
std::vector<std::byte> RpcError::serialize() const {
    std::vector<std::byte> result;
    auto id_bytes = reinterpret_cast<const std::byte*>(&connection_id);
    result.insert(result.end(), id_bytes, id_bytes + sizeof(connection_id));
    serialize_string(result, error);
    return result;
}

RpcError RpcError::deserialize(std::span<const std::byte> data) {
    RpcError msg;
    if (data.size() >= sizeof(ConnectionId)) {
        std::memcpy(&msg.connection_id, data.data(), sizeof(msg.connection_id));
        data = data.subspan(sizeof(msg.connection_id));
        msg.error = deserialize_string(data);
    }
    return msg;
}

} // namespace protoflow::rpc_service
