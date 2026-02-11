#include "services/rpc_server_service.hpp"
#include <protoflow/logging/macros.hpp>

namespace protoflow::mainapp {

RpcServerService::RpcServerService(
    ServerTransportFactory server_transport,
    ClientTransportFactory client_transport_factory
)
    : server_transport_(server_transport())
    , client_transport_factory_(std::move(client_transport_factory))
{
}

RpcServerService::~RpcServerService() = default;

void RpcServerService::start() {
    PROTOFLOW_LOG_INFO(*this, "RPC Server Service starting");
    
    if (server_transport_ && server_transport_->is_connected()) {
        listening_ = true;
        PROTOFLOW_LOG_INFO(*this, "RPC Server listening");
    } else {
        PROTOFLOW_LOG_ERROR(*this, "Failed to start RPC server: invalid transport");
    }
}

void RpcServerService::stop() {
    PROTOFLOW_LOG_INFO(*this, "RPC Server Service stopping");
    
    // Close all client connections
    for (auto& [id, client] : clients_) {
        if (client->transport) {
            client->transport->close();
        }
    }
    clients_.clear();
    
    // Stop server
    if (server_transport_) {
        server_transport_->close();
    }
    listening_ = false;
    outbound_.clear();
}

void RpcServerService::poll() {
    if (listening_) {
        // Accept new connections
        accept_new_connections();
        
        // Poll all clients
        for (auto it = clients_.begin(); it != clients_.end();) {
            auto& client = it->second;
            poll_client(*client);
            
            // Remove disconnected clients
            if (!client->transport || !client->transport->is_connected()) {
                if (client->pending_sends.empty()) {
                    PROTOFLOW_LOG_DEBUG(*this, "Removing disconnected client " << client->id);
                    
                    // Notify about disconnection
                    auto msg = messaging::MessageBuilder{}
                        .type(rpc_service::RpcMessageTypes::Disconnected)
                        .payload(rpc_service::RpcDisconnected{
                            client->id,
                            "Client disconnected"
                        }.serialize())
                        .build();
                    outbound_.push_back(std::move(msg));
                    
                    it = clients_.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
    
    // Handle incoming messages from message bus
    service::Service::poll();
}

void RpcServerService::handle(service::Message&& msg) {
    using namespace rpc_service;
    
    // Handle send requests to connected clients
    if (msg.header.type == RpcMessageTypes::SendRequest) {
        auto req = RpcSendRequest::deserialize(msg.data);
        handle_server_send_request(req);
    }
    else if (msg.header.type == RpcMessageTypes::DisconnectRequest) {
        auto req = RpcDisconnectRequest::deserialize(msg.data);
        handle_server_disconnect_request(req);
    }
    else {
        PROTOFLOW_LOG_WARN(*this, "Unknown message type: " << msg.header.type);
    }
}

std::vector<service::Message> RpcServerService::generate_outbound() {
    std::vector<service::Message> result;
    result.swap(outbound_);
    return result;
}

void RpcServerService::accept_new_connections() {
    // Create a new client transport from factory
    auto client_transport = client_transport_factory_();
    
    if (client_transport && client_transport->is_connected()) {
        auto client = std::make_unique<ClientConnection>();
        client->id = next_connection_id_++;
        client->transport = std::move(client_transport);
        
        PROTOFLOW_LOG_INFO(*this, "Accepted new RPC client connection " << client->id);
        
        // Send connected message
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::Connected)
            .payload(rpc_service::RpcConnected{client->id}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        
        clients_[client->id] = std::move(client);
    }
    // If no connection available, factory returns nullptr or unconnected transport
}

void RpcServerService::poll_client(ClientConnection& client) {
    if (!client.transport || !client.transport->is_connected()) {
        return;
    }
    
    // Try to send pending data
    try_send_pending(client);
    
    // Try to receive data
    try_receive(client);
}

void RpcServerService::try_send_pending(ClientConnection& client) {
    while (!client.pending_sends.empty() && client.transport && client.transport->is_connected()) {
        auto& data = client.pending_sends.front();
        
        bool sent = client.transport->send(data);
        
        if (sent) {
            size_t bytes_sent = data.size();
            PROTOFLOW_LOG_DEBUG(*this, "Sent " << bytes_sent << " bytes to client " << client.id);
            
            // Send confirmation
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::Sent)
                .payload(rpc_service::RpcSent{client.id, bytes_sent}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            
            // Remove from pending
            client.pending_sends.erase(client.pending_sends.begin());
        }
        else {
            PROTOFLOW_LOG_ERROR(*this, "Send failed to client " << client.id);
            
            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::SendFailed)
                .payload(rpc_service::RpcSendFailed{client.id, "Send failed"}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            
            // Disconnect on error
            client.transport->close();
            client.pending_sends.clear();
            break;
        }
    }
}

void RpcServerService::try_receive(ClientConnection& client) {
    if (!client.transport || !client.transport->is_connected()) {
        return;
    }
    
    // Try to receive data (non-blocking)
    auto result = client.transport->receive(8192);
    
    if (result.has_value()) {
        auto& data = result.value();
        
        if (!data.empty()) {
            PROTOFLOW_LOG_DEBUG(*this, "Received " << data.size() 
                               << " bytes from client " << client.id);
            
            // Send received message
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::Received)
                .payload(rpc_service::RpcReceived{client.id, std::move(data)}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
        }
    }
    else {
        // Error during receive
        PROTOFLOW_LOG_ERROR(*this, "Receive failed from client " << client.id 
                           << ": " << result.error());
        
        // Send error message
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::Error)
            .payload(rpc_service::RpcError{client.id, result.error()}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        
        // Disconnect on error
        client.transport->close();
    }
}

void RpcServerService::handle_server_send_request(const rpc_service::RpcSendRequest& req) {
    auto it = clients_.find(req.connection_id);
    if (it == clients_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Send request for unknown client: " << req.connection_id);
        
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::SendFailed)
            .payload(rpc_service::RpcSendFailed{
                req.connection_id,
                "Client not found"
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        return;
    }
    
    auto& client = it->second;
    
    if (!client->transport || !client->transport->is_connected()) {
        PROTOFLOW_LOG_WARN(*this, "Send request for disconnected client: " << req.connection_id);
        
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::SendFailed)
            .payload(rpc_service::RpcSendFailed{
                req.connection_id,
                "Client not connected"
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        return;
    }
    
    // Add to pending sends
    client->pending_sends.push_back(req.data);
    
    // Try to send immediately
    try_send_pending(*client);
}

void RpcServerService::handle_server_disconnect_request(const rpc_service::RpcDisconnectRequest& req) {
    auto it = clients_.find(req.connection_id);
    if (it == clients_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Disconnect request for unknown client: " << req.connection_id);
        return;
    }
    
    auto& client = it->second;
    
    PROTOFLOW_LOG_INFO(*this, "Disconnecting client " << req.connection_id);
    
    if (client->transport) {
        client->transport->close();
    }
    
    // Send disconnected message
    auto msg = messaging::MessageBuilder{}
        .type(rpc_service::RpcMessageTypes::Disconnected)
        .payload(rpc_service::RpcDisconnected{
            req.connection_id,
            "Server requested disconnect"
        }.serialize())
        .build();
    outbound_.push_back(std::move(msg));
}

} // namespace protoflow::mainapp
