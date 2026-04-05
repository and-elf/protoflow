#include <protoflow/rpc_service/rpc_client_service.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/logging/macros.hpp>
#include <random>

namespace protoflow::rpc_service {

RpcClientService::RpcClientService(
    std::unique_ptr<protoflow::rpc::transport_interface> transport,
    const RpcClientConfig& config)
    : transport_(std::move(transport)), connected_(false), connection_id_(1),
      state_(ConnectionState::DISCONNECTED), retry_config_(config.retry_config),
      retry_count_(0) {
    last_activity_time_ = std::chrono::steady_clock::now();
    last_heartbeat_sent_ = std::chrono::steady_clock::now();
    last_heartbeat_ack_ = std::chrono::steady_clock::now();
    heartbeat_send_interval_ms_ = config.heartbeat_config.send_interval_ms;
    heartbeat_timeout_ms_ = config.heartbeat_config.timeout_ms;
}

RpcClientService::~RpcClientService() = default;

void RpcClientService::start() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service started");
}

void RpcClientService::stop() {
    PROTOFLOW_LOG_INFO(*this, "RPC Client Service stopping - closing connection");
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = ConnectionState::DISCONNECTED;
    }
    if (transport_) {
        transport_->close();
    }
    transport_.reset();
    pending_sends_.clear();
    connected_ = false;
    outbound_.clear();
}

void RpcClientService::update_last_activity() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_activity_time_ = std::chrono::steady_clock::now();
}

void RpcClientService::check_and_send_heartbeat() {
    auto now = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        // If heartbeat is pending and we've exceeded timeout, handle it
        if (heartbeat_pending_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_heartbeat_sent_).count();
            if (elapsed > static_cast<int64_t>(heartbeat_timeout_ms_)) {
                PROTOFLOW_LOG_WARN(*this, "Heartbeat timeout - no ACK received after " 
                                   << elapsed << "ms");
                handle_heartbeat_timeout();
                return;
            }
        }
        
        // If not pending, check if it's time to send a new heartbeat
        if (!heartbeat_pending_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_heartbeat_sent_).count();
            if (elapsed >= static_cast<int64_t>(heartbeat_send_interval_ms_)) {
                last_heartbeat_sent_ = now;
                heartbeat_pending_ = true;
                PROTOFLOW_LOG_DEBUG(*this, "Sending heartbeat");
            }
        }
    }
    
    // Send heartbeat if pending and transport is connected
    if (heartbeat_pending_ && transport_ && transport_->is_connected()) {
        // Send heartbeat message
        std::vector<std::byte> heartbeat_data;
        bool sent = transport_->send(heartbeat_data);  // Empty payload for heartbeat
        if (sent) {
            PROTOFLOW_LOG_DEBUG(*this, "Heartbeat sent");
            // ACK will be checked on next receive
        } else {
            PROTOFLOW_LOG_WARN(*this, "Failed to send heartbeat");
        }
    }
}

void RpcClientService::handle_heartbeat_timeout() {
    PROTOFLOW_LOG_WARN(*this, "Heartbeat timeout reached - reconnecting");
    heartbeat_pending_ = false;
    if (transport_) {
        transport_->close();
    }
    connected_ = false;
    handle_connection_error("Heartbeat timeout");
}

void RpcClientService::transition_to_backoff() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_ = ConnectionState::BACKOFF;
    
    // Calculate backoff delay with exponential backoff and jitter
    uint32_t delay_ms = retry_config_.initial_delay_ms;
    for (uint32_t i = 1; i < retry_count_; ++i) {
        delay_ms = static_cast<uint32_t>(delay_ms * retry_config_.backoff_multiplier);
        if (delay_ms > retry_config_.max_delay_ms) {
            delay_ms = retry_config_.max_delay_ms;
            break;
        }
    }
    
    // Add jitter if enabled (±10%)
    if (retry_config_.enable_jitter) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(-10, 10);
        int jitter_offset = dis(gen);
        double jitter_factor = (100.0 + static_cast<double>(jitter_offset)) / 100.0;
        delay_ms = static_cast<uint32_t>(static_cast<double>(delay_ms) * jitter_factor);
    }
    
    PROTOFLOW_LOG_INFO(*this, "Transitioning to BACKOFF state for " << delay_ms << "ms (retry " << retry_count_ << ")");
    backoff_expiry_time_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
}

void RpcClientService::connect_with_retry() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Check if we're in backoff and backoff has expired
    if (state_ == ConnectionState::BACKOFF) {
        if (std::chrono::steady_clock::now() < backoff_expiry_time_) {
            return;  // Still in backoff period
        }
        // Backoff expired, attempt reconnection
        retry_count_++;
        if (retry_count_ > retry_config_.max_retries) {
            PROTOFLOW_LOG_ERROR(*this, "Max retries exceeded (" << retry_count_ << ")");
            state_ = ConnectionState::ERROR;
            return;
        }
        PROTOFLOW_LOG_INFO(*this, "Backoff expired, attempting reconnection (attempt " << retry_count_ << ")");
    }
    
    // If already connecting or connected, do nothing
    if (state_ == ConnectionState::CONNECTING || state_ == ConnectionState::CONNECTED) {
        return;
    }
    
    if (state_ == ConnectionState::DISCONNECTED || state_ == ConnectionState::ERROR) {
        retry_count_ = 1;
        state_ = ConnectionState::CONNECTING;
        PROTOFLOW_LOG_INFO(*this, "Starting connection attempt");
    }
}

void RpcClientService::handle_connection_error(const std::string& error_msg) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        PROTOFLOW_LOG_WARN(*this, "Connection error: " << error_msg << " (attempt " << retry_count_ << ")");
        
        if (retry_count_ < retry_config_.max_retries) {
            state_ = ConnectionState::ERROR;
        } else {
            state_ = ConnectionState::ERROR;
            PROTOFLOW_LOG_ERROR(*this, "Connection failed after " << retry_count_ << " attempts");
        }
    }
    
    if (transport_) {
        transport_->close();
    }
    connected_ = false;
    pending_sends_.clear();
    
    // Transition to backoff for retry
    transition_to_backoff();
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
    (void)req;
    // This method is now a stub or can be removed. Use add_connection() instead.
    PROTOFLOW_LOG_WARN(*this, "handle_connect_request is deprecated. Use add_connection() to add new connections.");
}


void RpcClientService::handle_send_request(const RpcSendRequest& req) {
    auto current_state = get_connection_state();
    
    if (current_state != ConnectionState::CONNECTED || !transport_ || !transport_->is_connected()) {
        PROTOFLOW_LOG_WARN(*this, "Send request in state " << static_cast<int>(current_state) 
                           << " (not CONNECTED)");
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
    (void)req;
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
    auto current_state = get_connection_state();
    
    // Handle state transitions
    switch (current_state) {
        case ConnectionState::DISCONNECTED:
        case ConnectionState::ERROR:
            // Begin connection attempt
            connect_with_retry();
            break;
            
        case ConnectionState::CONNECTING:
            // Wait for transport to connect (transport manages actual connection)
            // For now, assume if transport exists and is connected, we're good
            if (transport_ && transport_->is_connected()) {
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_ = ConnectionState::CONNECTED;
                    retry_count_ = 0;  // Reset retry count on successful connection
                }
                PROTOFLOW_LOG_INFO(*this, "Connection established successfully");
                update_last_activity();
            }
            break;
            
        case ConnectionState::CONNECTED:
            // Normal operation
            if (transport_ && transport_->is_connected()) {
                check_and_send_heartbeat();
                try_send_pending();
                try_receive();
            } else {
                // Connection dropped unexpectedly
                handle_connection_error("Transport closed unexpectedly");
            }
            break;
            
        case ConnectionState::BACKOFF:
            // Waiting for backoff to expire, checked in connect_with_retry()
            connect_with_retry();
            break;
    }
}

void RpcClientService::try_send_pending() {
    if (!transport_ || !transport_->is_connected()) {
        return;
    }
    
    while (!pending_sends_.empty()) {
        auto& data = pending_sends_.front();
        bool sent = transport_->send(data);
        if (sent) {
            size_t bytes_sent = data.size();
            PROTOFLOW_LOG_DEBUG(*this, "Sent " << bytes_sent << " bytes to server");
            update_last_activity();
            
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
            // Send failed, but don't immediately disconnect
            // Log as warning and stay connected to retry
            PROTOFLOW_LOG_WARN(*this, "Send failed to server, will retry on next poll");
            // Send error message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::SendFailed)
                .payload(RpcSendFailed{connection_id_, "Send failed"}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
            break;  // Stop trying to avoid busy loop, will retry next poll
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
            update_last_activity();
            
            // Send received message
            auto msg = messaging::MessageBuilder{}
                .type(RpcMessageTypes::Received)
                .payload(RpcReceived{connection_id_, std::move(data)}.serialize())
                .build();
            outbound_.push_back(std::move(msg));
        }
    }
    else {
        // Error receiving
        std::string error = result.error();
        PROTOFLOW_LOG_WARN(*this, "Receive error: " << error);
        // Don't immediately disconnect on receive errors that might be transient
        // Only handle critical errors
        if (error.find("Connection reset") != std::string::npos ||
            error.find("Connection refused") != std::string::npos ||
            error.find("Broken pipe") != std::string::npos) {
            handle_connection_error(error);
        }
    }
}

} // namespace protoflow::rpc_service
