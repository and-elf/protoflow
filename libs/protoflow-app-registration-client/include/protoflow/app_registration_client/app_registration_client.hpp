#pragma once

#include "config.hpp"
#include <protoflow/app_registration_protocol.hpp>
#include <protoflow/service/service.hpp>
#include <protoflow/fsm.hpp>
#include <memory>
#include <chrono>
#include <optional>

namespace protoflow::app_registration_client {

// Import protocol types
using State = app_registration_protocol::State;
using Event = app_registration_protocol::Event;

// Forward declarations for FSM
template<typename Sink> struct FsmImplT;
struct AppRegistrationSink;

/// App registration client service
/// Connects to main app server and maintains registration via RPC protocol
/// Handles connection lifecycle, handshake, registration, and heartbeats
class AppRegistrationClient : public service::Service {
public:
    /// Construct with configuration
    explicit AppRegistrationClient(Config config);
    
    /// For testing: factory to create client with custom FSM sink
    template<typename Sink>
    static std::unique_ptr<AppRegistrationClient> create_with_sink(Config config, Sink sink);
    
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
    
    /// Wait for registration to complete with timeout
    /// @param timeout_ms Timeout in milliseconds
    /// @return true if successfully registered, false on timeout or error
    bool wait_for_registered(uint32_t timeout_ms = 5000) noexcept;
    
    /// Get last registration error message
    [[nodiscard]] std::string get_registration_error() const noexcept { return registration_error_; }

protected:
    /// Handle incoming messages (from runtime)
    void handle(service::Message&& msg) override;
    
    /// Generate outbound messages (to runtime)
    std::vector<service::Message> generate_outbound() override;

private:
    // Forward declaration for FSM implementation
    struct FsmImpl;
    template<typename Sink> friend struct FsmImplT;
    friend struct AppRegistrationSink;
    
    // Internal constructor for testing with custom sink
    template<typename Sink>
    AppRegistrationClient(Config config, Sink sink, int /*tag*/);
    
    // Configuration
    Config config_;
    
    // Current state
    State state_;
    
    // FSM for state management (PIMPL)
    std::unique_ptr<FsmImpl> fsm_;
    
    // Timing
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::chrono::steady_clock::time_point connection_start_;
    uint32_t reconnect_count_;
    
    // Error tracking
    std::string registration_error_;
    
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
    void handle_hello(std::span<const std::byte> payload);
    void handle_register_ack(std::span<const std::byte> payload);
    void handle_heartbeat_ack(std::span<const std::byte> payload);
    
    // Connection management
    void process_incoming_data(std::span<const std::byte> data);
    
    // FSM
    void create_fsm();
    template<typename Sink>
    void create_fsm_with_sink(Sink sink);
    void process_event(Event event);
    
    // Timing checks
    void check_heartbeat_timer();
    void check_connection_timeout();
};

// Template implementation for testing
template<typename Sink>
std::unique_ptr<AppRegistrationClient> AppRegistrationClient::create_with_sink(Config config, Sink sink) {
    auto client = std::unique_ptr<AppRegistrationClient>(new AppRegistrationClient(std::move(config), std::move(sink), 0));
    return client;
}

template<typename Sink>
AppRegistrationClient::AppRegistrationClient(Config config, Sink sink, int /*tag*/)
    : Service()
    , config_(std::move(config))
    , state_(State::Disconnected)
    , reconnect_count_(0)
{
    create_fsm_with_sink(std::move(sink));
}

} // namespace protoflow::app_registration_client
