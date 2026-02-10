#pragma once

#include "config.hpp"
#include <protoflow/app_registration_protocol.hpp>
#include <protoflow/service/service.hpp>
#include <protoflow/fsm.hpp>
#include <protoflow/rpc_service/messages.hpp>
#include <memory>
#include <chrono>
#include <optional>

namespace protoflow::app_registration_client {

// Import protocol types
using State = app_registration_protocol::State;
using Event = app_registration_protocol::Event;

/// App registration client service
/// Connects to main app server and maintains registration via RPC protocol
/// Handles connection lifecycle, handshake, registration, and heartbeats
class AppRegistrationClient : public service::Service {
public:
    /// Construct with configuration
    explicit AppRegistrationClient(Config config);
    
    /// Destructor
    ~AppRegistrationClient() override;
    
    // Service lifecycle
    void start() override;
    void stop() override;
    void poll() override;
    
    /// Get current FSM state
    [[nodiscard]] State current_state() const noexcept { return state_; }
    
    /// Check if registered with server
    [[nodiscard]] bool is_registered() const noexcept { 
        return state_ == State::Registered; 
    }
    
    /// Check if connected to server
    [[nodiscard]] bool is_connected() const noexcept {
        return state_ == State::Registered || 
               state_ == State::Registering ||
               state_ == State::Connecting;
    }
    
    /// Get configuration
    [[nodiscard]] const Config& config() const noexcept { return config_; }
    
    /// Logging support
    std::string name() const { return "AppRegistrationClient[" + config_.app_name + "]"; }

protected:
    /// Handle incoming messages (from runtime)
    void handle(service::Message&& msg) override;
    
    /// Generate outbound messages (to runtime)
    std::vector<service::Message> generate_outbound() override;

private:
    // Forward declaration for FSM implementation
    struct FsmImpl;
    
    // Configuration
    Config config_;
    
    // Current state
    State state_;
    
    // FSM for state management (PIMPL)
    std::unique_ptr<FsmImpl> fsm_;
    
    // RPC connection ID (managed by RpcService)
    rpc_service::ConnectionId connection_id_;
    bool connection_requested_ = false;
    
    // Timing
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::chrono::steady_clock::time_point connection_start_;
    uint32_t reconnect_count_;
    
    // FSM action handlers
    void on_connect();
    void on_connected();
    void on_handshake_complete();
    void on_registration_ack();
    void on_heartbeat_tick();
    void on_heartbeat_ack();
    void on_disconnected();
    void on_reconnect();
    void on_fatal_error();
    void on_shutdown();
    
    // RPC protocol handlers
    void send_hello();
    void send_registration();
    void send_heartbeat();
    void handle_hello_ack(std::span<const std::byte> payload);
    void handle_register_ack(std::span<const std::byte> payload);
    void handle_heartbeat_ack(std::span<const std::byte> payload);
    
    // RPC service message handlers
    void handle_rpc_connected(const rpc_service::RpcConnected& msg);
    void handle_rpc_connection_failed(const rpc_service::RpcConnectionFailed& msg);
    void handle_rpc_received(const rpc_service::RpcReceived& msg);
    void handle_rpc_disconnected(const rpc_service::RpcDisconnected& msg);
    
    // Connection management
    void request_connection();
    void send_rpc_data(std::vector<std::byte> data);
    void disconnect_connection();
    void process_incoming_data(std::span<const std::byte> data);
    
    // FSM
    void create_fsm();
    void process_event(Event event);
    
    // Timing checks
    void check_heartbeat_timer();
    void check_connection_timeout();
};

} // namespace protoflow::app_registration_client
