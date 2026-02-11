#include <protoflow/rpc_service/rpc_client_service.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/logging/macros.hpp>

namespace protoflow::rpc_service {

RpcClientService::RpcClientService(std::unique_ptr<protoflow::rpc::transport_interface> transport)
    : transport_(std::move(transport)), connected_(false), connection_id_(1) {}

RpcClientService::~RpcClientService() = default;

void RpcClientService::start() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service started");
}

void RpcClientService::stop() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service stopping - closing connection");
    if (transport_) {
        transport_->close();
    }
    transport_.reset();
    pending_sends_.clear();
    connected_ = false;
    outbound_.clear();
}

void RpcClientService::poll() {
    poll_connection();
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
    // This method is now a stub or can be removed. Use add_connection() instead.
    PROTOFLOW_LOG_WARN(*this, "handle_connect_request is deprecated. Use add_connection() to add new connections.");
}


void RpcClientService::handle_send_request(const RpcSendRequest& req) {
    if (!transport_ || !transport_->is_connected()) {
        PROTOFLOW_LOG_WARN(*this, "Send request for disconnected client");
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::SendFailed)
            .payload(RpcSendFailed{
                connection_id_,
                "Client not connected"
            }.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        return;
    }
    // Add to pending sends
    pending_sends_.push_back(req.data);
    // Try to send immediately
    try_send_pending();
}

void RpcClientService::handle_disconnect_request(const RpcDisconnectRequest& req) {
    PROTOFLOW_LOG_INFO(*this, "Disconnecting client");
    if (transport_) {
        transport_->close();
    }
    connected_ = false;
    // Send disconnected message
    auto msg = messaging::MessageBuilder{}
        .type(RpcMessageTypes::Disconnected)
        .payload(RpcDisconnected{
            connection_id_,
            "User requested disconnect"
        }.serialize())
        .build();
    outbound_.push_back(std::move(msg));
}

void RpcClientService::poll_connection() {
    if (!transport_ || !transport_->is_connected()) {
        return;
    }
    try_send_pending();
    try_receive();
}

void RpcClientService::try_send_pending() {
    while (!pending_sends_.empty() && transport_ && transport_->is_connected()) {
        auto& data = pending_sends_.front();
        bool sent = transport_->send(data);
        if (sent) {
            size_t bytes_sent = data.size();
            PROTOFLOW_LOG_DEBUG(*this, "Sent " << bytes_sent << " bytes to server");
            // Send confirmation
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::Sent)
                .payload(RpcSent{connection_id_, bytes_sent}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            // Remove from pending
            pending_sends_.erase(pending_sends_.begin());
        }
        else {
            PROTOFLOW_LOG_ERROR(*this, "Send failed to server");
            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::SendFailed)
                .payload(RpcSendFailed{connection_id_, "Send failed"}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            // Disconnect on error
            transport_->close();
            pending_sends_.clear();
            break;
        }
    }
}

void RpcClientService::try_receive() {
    if (!transport_ || !transport_->is_connected()) {
        return;
    }
    // Try to receive data (non-blocking)
    auto result = transport_->receive(8192);
    if (result.has_value()) {
        auto& data = result.value();
        if (!data.empty()) {
            PROTOFLOW_LOG_DEBUG(*this, "Received " << data.size() << " bytes from server");
            // Send received message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::Received)
                .payload(RpcReceived{connection_id_, std::move(data)}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
        }
    }
    else {
        // Error receiving - transport interface doesn't expose error codes
        // so we just handle the error generically
        PROTOFLOW_LOG_ERROR(*this, "Receive failed from server: " << result.error());
        // Send error message
        auto msg = messaging::MessageBuilder{}
            .type(RpcMessageTypes::Error)
            .payload(RpcError{connection_id_, result.error()}.serialize())
            .build();
        outbound_.push_back(std::move(msg));
        // Disconnect on error
        transport_->close();
    }
}

} // namespace protoflow::rpc_service
