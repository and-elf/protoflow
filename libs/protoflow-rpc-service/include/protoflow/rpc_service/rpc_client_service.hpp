#pragma once

#include "messages.hpp"
#include <protoflow/service/service.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace protoflow::rpc_service {

/// Connection state machine
enum class ConnectionState {
    DISCONNECTED,    ///< Not connected
    CONNECTING,      ///< Attempting to connect
    CONNECTED,       ///< Connected and ready
    ERROR,           ///< Connection error occurred
    BACKOFF          ///< In exponential backoff before retry
};

/// Retry configuration for connection failures
struct RetryConfig {
    uint32_t max_retries = 5;
    double backoff_multiplier = 2.0;      // Exponential backoff multiplier
    uint32_t initial_delay_ms = 100;       // Initial delay in milliseconds
    uint32_t max_delay_ms = 30000;         // Max delay (30 seconds)
    bool enable_jitter = true;             // Add random jitter to prevent thundering herd
};

/// Heartbeat configuration for connection health monitoring
struct HeartbeatConfig {
    uint32_t send_interval_ms = 5000;      // Send heartbeat every 5 seconds
    uint32_t timeout_ms = 30000;           // Heartbeat timeout after 30 seconds
};

/// RPC Client Service configuration
struct RpcClientConfig {
    RetryConfig retry_config;
    HeartbeatConfig heartbeat_config;
};

/// RPC Client Service - manages multiple outbound RPC connections
/// Handles all network I/O via message passing
class RpcClientService : public service::Service {
public:
    /// Constructor with optional config
    explicit RpcClientService(
        std::unique_ptr<protoflow::rpc::transport_interface> transport,
        const RpcClientConfig& config = RpcClientConfig{});
    ~RpcClientService() override;

    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;

    // State accessors
    [[nodiscard]] ConnectionState get_connection_state() const noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point get_last_activity_time() const noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return last_activity_time_;
    }

    // Configure retry behavior
    void set_retry_config(const RetryConfig& config) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        retry_config_ = config;
    }

    // Configure heartbeat intervals (in milliseconds)
    void set_heartbeat_interval(uint32_t send_interval_ms, uint32_t timeout_ms) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        heartbeat_send_interval_ms_ = send_interval_ms;
        heartbeat_timeout_ms_ = timeout_ms;
    }

protected:
    void handle(service::Message&& msg) override;
    std::vector<service::Message> generate_outbound() override;

    // Only one connection/transport
    std::unique_ptr<protoflow::rpc::transport_interface> transport_;
    std::vector<std::vector<std::byte>> pending_sends_;
    bool connected_ = false;
    ConnectionId connection_id_ = 1;
    std::vector<service::Message> outbound_;

    // Connection state management
    mutable std::mutex state_mutex_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    RetryConfig retry_config_;
    uint32_t retry_count_ = 0;
    std::chrono::steady_clock::time_point last_activity_time_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point backoff_expiry_time_;
    
    // Heartbeat management
    uint32_t heartbeat_send_interval_ms_ = 5000;      // Send heartbeat every 5 seconds
    uint32_t heartbeat_timeout_ms_ = 30000;           // Heartbeat timeout after 30 seconds
    std::chrono::steady_clock::time_point last_heartbeat_sent_;
    std::chrono::steady_clock::time_point last_heartbeat_ack_;
    bool heartbeat_pending_ = false;

    void handle_send_request(const RpcSendRequest& req);
    void handle_connect_request(const RpcConnectRequest& req);
    void handle_disconnect_request(const RpcDisconnectRequest& req);
    void poll_connection();
    void try_send_pending();
    void try_receive();
    
    // New methods for fault tolerance
    void connect_with_retry();
    void handle_connection_error(const std::string& error_msg);
    void transition_to_backoff();
    void update_last_activity();
    void check_and_send_heartbeat();
    void handle_heartbeat_timeout();
};

} // namespace protoflow::rpc_service
