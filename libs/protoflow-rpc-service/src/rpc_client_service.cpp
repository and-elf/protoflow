#include <protoflow/rpc_service/rpc_client_service.hpp>
#include <protoflow/logging/macros.hpp>

namespace protoflow::rpc_service {

RpcClientService::RpcClientService(TransportFactory transport_factory)
    : transport_factory_(std::move(transport_factory))
{
}

RpcClientService::~RpcClientService() = default;

void RpcClientService::start() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service started");
}

void RpcClientService::stop() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service stopping - closing all connections");
    
    // Close all connections
    for (auto& [id, conn] : connections_) {
        if (conn->transport) {
            conn->transport->close();
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
        if ((!conn->transport || !conn->transport->is_connected()) && conn->pending_sends.empty()) {
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
    
    // Create and connect transport from factory
    conn->transport = transport_factory_(req);
    
    if (conn->transport && conn->transport->is_connected()) {
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
        PROTOFLOW_LOG_ERROR(*this, "Connection failed to " << req.host << ":" << req.port);
        
        // Send failure message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::ConnectionFailed)
            .payload(RpcConnectionFailed{
                req.connection_id, 
                "Failed to establish connection"
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
    
    if (!conn->transport || !conn->transport->is_connected()) {
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
    
    if (conn->transport) {
        conn->transport->close();
    }
    
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
    if (!conn.transport || !conn.transport->is_connected()) {
        return;
    }
    
    // Try to send pending data
    try_send_pending(conn);
    
    // Try to receive data
    try_receive(conn);
}

void RpcClientService::try_send_pending(Connection& conn) {
    while (!conn.pending_sends.empty() && conn.transport && conn.transport->is_connected()) {
        auto& data = conn.pending_sends.front();
        
        bool sent = conn.transport->send(data);
        
        if (sent) {
            size_t bytes_sent = data.size();
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
            PROTOFLOW_LOG_ERROR(*this, "Send failed on connection " << conn.id);
            
            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::SendFailed)
                .payload(RpcSendFailed{conn.id, "Send failed"}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            
            // Disconnect on error
            conn.transport->close();
            
            // Clear pending sends
            conn.pending_sends.clear();
            break;
        }
    }
}

void RpcClientService::try_receive(Connection& conn) {
    if (!conn.transport || !conn.transport->is_connected()) {
        return;
    }
    
    // Try to receive data (non-blocking)
    auto result = conn.transport->receive(8192);
    
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
        // Error receiving - transport interface doesn't expose error codes
        // so we just handle the error generically
        PROTOFLOW_LOG_ERROR(*this, "Receive failed on connection " << conn.id 
                           << ": " << result.error());
        
        // Send error message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::Error)
            .payload(RpcError{conn.id, result.error()}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        
        // Disconnect on error
        conn.transport->close();
    }
}

} // namespace protoflow::rpc_service
