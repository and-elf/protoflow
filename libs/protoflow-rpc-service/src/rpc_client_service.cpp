#include <protoflow/rpc_service/rpc_client_service.hpp>
#include <protoflow/logging/macros.hpp>

namespace protoflow::rpc_service {

RpcClientService::RpcClientService() = default;

RpcClientService::~RpcClientService() = default;

void RpcClientService::start() {
    PROTOFLOW_LOG_INFO(*this, "RPC Service started");
}

void RpcClientService::stop() {
    PROTOFLOW_LOG_INFO(*this, "RPC Service stopping - closing all connections");
    
    // Close all connections
    for (auto& [id, conn] : connections_) {
        if (conn->connected) {
            conn->client.disconnect();
        }
    }
    connections_.clear();
    outbound_.clear();
}

void RpcClientService::poll() {
    // Poll all active connections
    for (auto it = connections_.begin(); it != connections_.end();) {
        auto& conn = it->second;
        poll_connection(*conn);
        
        // Remove disconnected connections
        if (!conn->connected && conn->pending_sends.empty()) {
            PROTOFLOW_LOG_DEBUG(*this, "Removing disconnected connection " << conn->id);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Handle incoming messages from message bus
    service::Service::poll();
}

void RpcClientService::handle(service::Message&& msg) {
    using namespace messaging;
    
    if (msg.header.type == RpcMessageTypes::ConnectRequest) {
        auto req = RpcConnectRequest::deserialize(msg.data);
        handle_connect_request(req);
    }
    else if (msg.header.type == RpcMessageTypes::SendRequest) {
        auto req = RpcSendRequest::deserialize(msg.data);
        handle_send_request(req);
    }
    else if (msg.header.type == RpcMessageTypes::DisconnectRequest) {
        auto req = RpcDisconnectRequest::deserialize(msg.data);
        handle_disconnect_request(req);
    }
    else {
        PROTOFLOW_LOG_WARN(*this, "Unknown message type: " << msg.header.type);
    }
}

std::vector<service::Message> RpcClientService::generate_outbound() {
    std::vector<service::Message> result;
    result.swap(outbound_);
    return result;
}

void RpcClientService::handle_connect_request(const RpcConnectRequest& req) {
    PROTOFLOW_LOG_INFO(*this, "Connecting to " << req.host << ":" << req.port 
                       << " (conn_id=" << req.connection_id << ")");
    
    // Create new connection
    auto conn = std::make_unique<Connection>();
    conn->id = req.connection_id;
    
    // Configure TCP client
    transport::tcp::tcp_config config;
    config.host = req.host;
    config.port = req.port;
    config.connect_timeout_ms = req.connect_timeout_ms;
    config.read_timeout_ms = req.read_timeout_ms;
    config.write_timeout_ms = req.write_timeout_ms;
    
    conn->client = transport::tcp::tcp_client(config);
    
    // Attempt connection
    auto result = conn->client.connect();
    
    if (result.has_value()) {
        conn->connected = true;
        PROTOFLOW_LOG_INFO(*this, "Connected to " << req.host << ":" << req.port);
        
        // Send success message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::Connected)
            .payload(RpcConnected{req.connection_id}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        
        connections_[req.connection_id] = std::move(conn);
    }
    else {
        PROTOFLOW_LOG_ERROR(*this, "Connection failed: " << result.error().to_string());
        
        // Send failure message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::ConnectionFailed)
            .payload(RpcConnectionFailed{
                req.connection_id, 
                result.error().to_string()
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
    }
}

void RpcClientService::handle_send_request(const RpcSendRequest& req) {
    auto it = connections_.find(req.connection_id);
    if (it == connections_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Send request for unknown connection: " << req.connection_id);
        
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::SendFailed)
            .payload(RpcSendFailed{
                req.connection_id,
                "Connection not found"
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        return;
    }
    
    auto& conn = it->second;
    
    if (!conn->connected) {
        PROTOFLOW_LOG_WARN(*this, "Send request for disconnected connection: " << req.connection_id);
        
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::SendFailed)
            .payload(RpcSendFailed{
                req.connection_id,
                "Connection not connected"
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        return;
    }
    
    // Add to pending sends
    conn->pending_sends.push_back(req.data);
    
    // Try to send immediately
    try_send_pending(*conn);
}

void RpcClientService::handle_disconnect_request(const RpcDisconnectRequest& req) {
    auto it = connections_.find(req.connection_id);
    if (it == connections_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Disconnect request for unknown connection: " << req.connection_id);
        return;
    }
    
    auto& conn = it->second;
    
    PROTOFLOW_LOG_INFO(*this, "Disconnecting connection " << req.connection_id);
    
    conn->client.disconnect();
    conn->connected = false;
    
    // Send disconnected message
    auto msg = messaging::MessageBuilder{}
        .type(RpcMessageTypes::Disconnected)
        .payload(RpcDisconnected{
            req.connection_id,
            "User requested disconnect"
        }.serialize())
        .build();
    outbound_.push_back(std::move(msg));
}

void RpcClientService::poll_connection(Connection& conn) {
    if (!conn.connected) {
        return;
    }
    
    // Try to send pending data
    try_send_pending(conn);
    
    // Try to receive data
    try_receive(conn);
}

void RpcClientService::try_send_pending(Connection& conn) {
    while (!conn.pending_sends.empty() && conn.connected) {
        auto& data = conn.pending_sends.front();
        
        auto result = conn.client.send(data);
        
        if (result.has_value()) {
            size_t bytes_sent = result.value();
            PROTOFLOW_LOG_DEBUG(*this, "Sent " << bytes_sent << " bytes on connection " << conn.id);
            
            // Send confirmation
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::Sent)
                .payload(RpcSent{conn.id, bytes_sent}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            
            // Remove from pending
            conn.pending_sends.erase(conn.pending_sends.begin());
        }
        else {
            auto& error = result.error();
            PROTOFLOW_LOG_ERROR(*this, "Send failed on connection " << conn.id 
                               << ": " << error.to_string());
            
            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::SendFailed)
                .payload(RpcSendFailed{conn.id, error.to_string()}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            
            // Disconnect on error
            conn.client.disconnect();
            conn.connected = false;
            
            // Clear pending sends
            conn.pending_sends.clear();
            break;
        }
    }
}

void RpcClientService::try_receive(Connection& conn) {
    if (!conn.connected) {
        return;
    }
    
    // Try to receive data (non-blocking)
    auto result = conn.client.receive(8192);
    
    if (result.has_value()) {
        auto& data = result.value();
        
        if (!data.empty()) {
            PROTOFLOW_LOG_DEBUG(*this, "Received " << data.size() 
                               << " bytes on connection " << conn.id);
            
            // Send received message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::Received)
                .payload(RpcReceived{conn.id, std::move(data)}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
        }
    }
    else {
        auto& error = result.error();
        
        // Non-blocking receive may return EAGAIN/EWOULDBLOCK - this is not an error
#if EAGAIN == EWOULDBLOCK
        if (error.error_code == EAGAIN) {
            return;
        }
#else
        if (error.error_code == EAGAIN || error.error_code == EWOULDBLOCK) {
            return;
        }
#endif
        
        PROTOFLOW_LOG_ERROR(*this, "Receive failed on connection " << conn.id 
                           << ": " << error.to_string());
        
        // Send error message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::Error)
            .payload(RpcError{conn.id, error.to_string()}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        
        // Disconnect on error
        conn.client.disconnect();
        conn.connected = false;
    }
}

} // namespace protoflow::rpc_service
