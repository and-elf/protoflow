#include "services/rpc_server_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <protoflow/messages.hpp>
#include <protoflow/app_registration_protocol/messages.hpp>
#include <cstring>
#include <iostream>
#include <chrono>

namespace protoflow::mainapp {

RpcServerService::RpcServerService(
    std::unique_ptr<rpc::transport_interface> server_transport,
    const RpcServerConfig& config
)
    : Service({protoflow::app_registration_protocol::response::hello_ack,
               protoflow::app_registration_protocol::response::register_ack,
               protoflow::app_registration_protocol::response::heartbeat_ack,
               protoflow::app_registration_protocol::response::unregister_app_ack,
               protoflow::app_registration_protocol::response::error})
    , client_timeout_seconds_(config.client_timeout_seconds)
    , server_transport_(std::move(server_transport))
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
}

void RpcServerService::poll() {
    if (listening_) {
        // Accept new connections
        accept_new_connections();

        // Check for client timeouts
        check_client_timeouts();

        // Poll all clients
        for (auto it = clients_.begin(); it != clients_.end();) {
            auto& client = it->second;
            poll_client(client.get());

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
                    write(std::move(msg));

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
    
    // Check if this is an app registration protocol response (types 130-134)
    if (msg.header.type >= messaging::MessageTypes::AppRegistrationResponses &&
        msg.header.type <= messaging::MessageTypes::AppRegistrationResponses + 4) {
        // This is a response from AppRegistrationService
        std::cerr << "[RPC_HANDLE] Received response type " << msg.header.type 
                  << " for client " << last_request_client_id_ << "\n";
        
        if (last_request_client_id_ != UINT32_MAX && clients_.find(last_request_client_id_) != clients_.end()) {
            // Wrap response back into wire protocol format
            auto client = clients_[last_request_client_id_].get();
            
            // Build wire message: [cmd (2 bytes)][size (2 bytes)][payload]
            std::vector<std::byte> wire_msg;
            
            // Command (response type)
            uint16_t cmd_val = static_cast<uint16_t>(msg.header.type);
            wire_msg.push_back(std::byte(cmd_val & 0xFF));
            wire_msg.push_back(std::byte((cmd_val >> 8) & 0xFF));
            
            // Size of payload
            uint16_t size_val = static_cast<uint16_t>(msg.data.size());
            wire_msg.push_back(std::byte(size_val & 0xFF));
            wire_msg.push_back(std::byte((size_val >> 8) & 0xFF));
            
            // Append payload
            for (auto byte : msg.data) {
                wire_msg.push_back(byte);
            }
            
            // Queue to client's pending sends
            client->pending_sends.push_back(std::move(wire_msg));
            std::cerr << "[RPC_HANDLE] Queued response of " << msg.data.size() 
                      << " bytes to client " << last_request_client_id_ << "\n";
            
            // Try to send immediately
            try_send_pending(client);
        }
        return;
    }
    
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

void RpcServerService::accept_new_connections() {
    // Accept new client connections from the server transport
    if (!server_transport_) return;

    // Accept returns a new client transport if a connection is available
    // TODO: You must implement accept() in your server transport class to return std::unique_ptr<rpc::transport_interface>
    auto client_transport = server_transport_->accept();
    if (client_transport && client_transport->is_connected()) {
        auto client = std::make_unique<ClientConnection>();
        client->id = next_connection_id_++;
        client->transport = std::move(client_transport);
        client->last_activity_time = std::chrono::steady_clock::now();

        PROTOFLOW_LOG_INFO(*this, "Accepted new RPC client connection " << client->id);

        // Send connected message
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::Connected)
            .payload(rpc_service::RpcConnected{client->id}.serialize())
            .build();
        write(std::move(msg));

        clients_[client->id] = std::move(client);
    }
    // If no connection available, accept() returns nullptr or unconnected transport
}

void RpcServerService::poll_client(ClientConnection* client) {
    if (!client || !client->transport || !client->transport->is_connected()) {
        return;
    }

    // Try to send pending data
    try_send_pending(client);

    // Try to receive data
    try_receive(client);
}

void RpcServerService::try_send_pending(ClientConnection* client) {
    if (!client) return;
    while (!client->pending_sends.empty() && client->transport && client->transport->is_connected()) {
        auto& data = client->pending_sends.front();

        bool sent = client->transport->send(data);

        if (sent) {
            size_t bytes_sent = data.size();
            PROTOFLOW_LOG_DEBUG(*this, "Sent " << bytes_sent << " bytes to client " << client->id);

            // Send confirmation
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::Sent)
                .payload(rpc_service::RpcSent{client->id, bytes_sent}.serialize())
                .build();
            write(std::move(msg));

            // Remove from pending
            client->pending_sends.erase(client->pending_sends.begin());
        }
        else {
            PROTOFLOW_LOG_ERROR(*this, "Send failed to client " << client->id);

            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::SendFailed)
                .payload(rpc_service::RpcSendFailed{client->id, "Send failed"}.serialize())
                .build();
            write(std::move(msg));

            // Disconnect on error
            client->transport->close();
            client->pending_sends.clear();
            break;
        }
    }
}

void RpcServerService::try_receive(ClientConnection* client) {
    if (!client || !client->transport || !client->transport->is_connected()) {
        return;
    }

    // Try to receive data (non-blocking)
    auto result = client->transport->receive(8192);
    
    if (result.has_value()) {
        auto& data = result.value();

        if (!data.empty()) {
            // Update last activity time on successful receive
            client->last_activity_time = std::chrono::steady_clock::now();
            
            // Try to parse as app registration protocol message
            if (data.size() >= 4) {
                // Format: [request:2][size:2][payload]
                uint16_t cmd_val;
                uint16_t size_val;
                std::memcpy(&cmd_val, data.data(), 2);
                std::memcpy(&size_val, data.data() + 2, 2);
                
                if (static_cast<size_t>(size_val) + 4 <= data.size()) {
                    // Handle register_app (cmd=121) - parse RPC payload and create AppRegistrationEvent
                    if (cmd_val == 121) {  // 121 = AppRegistrationRequests + register_app
                        // Parse RPC payload: [name_len:2][name][endpoint_count:4][endpoints...]
                        const std::byte* rpc_payload = data.data() + 4;
                        size_t offset = 0;
                        
                        // Read app name
                        if (offset + 2 <= size_val) {
                            uint16_t name_len;
                            std::memcpy(&name_len, rpc_payload + offset, 2);
                            offset += 2;
                            
                            if (offset + name_len <= size_val) {
                                std::string app_name(reinterpret_cast<const char*>(rpc_payload + offset), name_len);
                                offset += name_len;
                                
                                // Read endpoint count
                                if (offset + 4 <= size_val) {
                                    uint32_t ep_count;
                                    std::memcpy(&ep_count, rpc_payload + offset, 4);
                                    offset += 4;
                                    
                                    // Read endpoints
                                    std::vector<std::string> endpoints;
                                    for (uint32_t i = 0; i < ep_count && offset + 2 <= size_val; ++i) {
                                        uint16_t ep_len;
                                        std::memcpy(&ep_len, rpc_payload + offset, 2);
                                        offset += 2;
                                        
                                        if (offset + ep_len <= size_val) {
                                            std::string endpoint(reinterpret_cast<const char*>(rpc_payload + offset), ep_len);
                                            endpoints.push_back(endpoint);
                                            offset += ep_len;
                                        }
                                    }
                                    
                                    // Create AppRegistrationEvent and serialize it
                                    using namespace protoflow::app_registration_protocol;
                                    AppRegistrationEvent event;
                                    event.app_name = app_name;
                                    event.endpoints = endpoints;
                                    
                                    auto event_payload = event.serialize();
                                    auto msg = messaging::MessageBuilder{}
                                        .type(static_cast<uint16_t>(request::register_app))
                                        .payload(std::move(event_payload))
                                        .build();

                                    write(std::move(msg));
                                    return;
                                }
                            }
                        }
                    }
                    
                    // For other message types, queue the raw payload
                    std::vector<std::byte> payload(data.begin() + 4, data.begin() + 4 + size_val);
                    
                    auto msg = messaging::MessageBuilder{}
                        .type(cmd_val)  // Use the app protocol request type directly
                        .payload(std::move(payload))
                        .build();
                    
                    // Track which client sent this request
                    last_request_client_id_ = client->id;
                    std::cerr << "[RPC] Storing client " << client->id << " for pending request\n";
                    
                    write(std::move(msg));
                    return;
                }
            }
            
            // Not recognized as app protocol, send as generic RPC message
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::Received)
                .payload(rpc_service::RpcReceived{client->id, std::move(data)}.serialize())
                .build();
            write(std::move(msg));
        }
    }
    else {
        // Error during receive
        PROTOFLOW_LOG_ERROR(*this, "Receive failed from client " << client->id
                           << ": " << result.error());

        // Send error message
        auto msg = messaging::MessageBuilder{}
            .type(rpc_service::RpcMessageTypes::Error)
            .payload(rpc_service::RpcError{client->id, result.error()}.serialize())
            .build();
        write(std::move(msg));

        // Disconnect on error
        client->transport->close();
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
        write(std::move(msg));
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
        write(std::move(msg));
        return;
    }

    // Add to pending sends
    client->pending_sends.push_back(req.data);

    // Try to send immediately
    try_send_pending(client.get());
}

void RpcServerService::handle_server_disconnect_request(const rpc_service::RpcDisconnectRequest& req) {
    auto it = clients_.find(req.connection_id);
    if (it == clients_.end()) {
        PROTOFLOW_LOG_WARN(*this, "Disconnect request for unknown client: " << req.connection_id);
        return;
    }

    auto* client = it->second.get();

    PROTOFLOW_LOG_INFO(*this, "Disconnecting client " << req.connection_id);

    if (client && client->transport) {
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
    write(std::move(msg));
}

void RpcServerService::check_client_timeouts() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = clients_.begin(); it != clients_.end();) {
        auto& client = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - client->last_activity_time).count();
        
        if (elapsed > static_cast<int64_t>(client_timeout_seconds_)) {
            PROTOFLOW_LOG_WARN(*this, "Client " << client->id << " timeout after " 
                               << elapsed << " seconds of inactivity");
            
            // Send timeout notification
            auto msg = messaging::MessageBuilder{}
                .type(rpc_service::RpcMessageTypes::Disconnected)
                .payload(rpc_service::RpcDisconnected{
                    client->id,
                    "Client timeout due to inactivity"
                }.serialize())
                .build();
            write(std::move(msg));
            
            // Close client connection
            if (client->transport) {
                client->transport->close();
            }
            client->pending_sends.clear();
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace protoflow::mainapp
