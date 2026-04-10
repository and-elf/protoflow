#include <protoflow/app_registration_client/app_registration_client.hpp>

#ifdef PROTOFLOW_TEST_MODE
#include "test_logging_stub.hpp"
#else
#include <protoflow/logging/macros.hpp>
#endif

#include <protoflow/rpc/protocol.hpp>
#include <protoflow/fsm/fsm.hpp>
#include <protoflow/app_registration_protocol/messages.hpp>
#include <memory>
#include <protoflow/fsm/sinks.hpp>
#include <cstring>
#include <functional>
#include <queue>
#include <thread>
#include <sstream>

namespace protoflow::app_registration_client {

namespace {
    using namespace std::chrono_literals;
    using namespace app_registration_protocol;
    
    // Convert config to string for logging
    std::string config_to_string(const Config& cfg) {
        std::ostringstream oss;
        oss << "{ app: " << cfg.app_name 
            << ", version: " << cfg.version
            << ", endpoints: [";
        for (size_t i = 0; i < cfg.endpoints.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << cfg.endpoints[i];
        }
        oss << "] }";
        return oss.str();
    }
    
    // Helper to create protocol message
    service::Message make_protocol_message(request cmd, std::span<const std::byte> payload) {
        // Protocol format: [request:2][payload_size:2][payload]
        std::vector<std::byte> data;
        data.resize(4 + payload.size());
        
        auto cmd_val = static_cast<uint16_t>(cmd);
        auto size_val = static_cast<uint16_t>(payload.size());
        
        std::memcpy(data.data(), &cmd_val, 2);
        std::memcpy(data.data() + 2, &size_val, 2);
        if (!payload.empty()) {
            std::memcpy(data.data() + 4, payload.data(), payload.size());
        }
        
        std::cout << "[DEBUG] make_protocol_message: cmd=" << static_cast<int>(cmd) 
                  << ", payload_size=" << payload.size() 
                  << ", total_data_size=" << data.size() << "\n";
        
        messaging::MessageHeader header;
        header.type = messaging::MessageTypes::Payload;
        header.priority = messaging::Priority::Normal;
        
        return service::Message{header, std::move(data)};
    }
}

// Production sink that uses protoflow logging
struct AppRegistrationSink {
    AppRegistrationClient* client;
    
    void emit(const fsm::FsmEvent& e) const;
};

// FSM implementation using when-then-to-otherwise API
template<typename Sink>
struct FsmImplT {
    using State = app_registration_protocol::State;
    using Event = app_registration_protocol::Event;
    
    AppRegistrationClient* client;
    decltype(fsm::make_fsm(
        std::string{},
        fsm::when<State::Disconnected, Event::Connect>()
            .then(std::function<void()>{})
            .to<State::Connecting>()
        | fsm::when<State::Disconnected, Event::Shutdown>()
            .stay()
        | fsm::when<State::Connecting, Event::Connected>()
            .then(std::function<void()>{})
            .to<State::Registering>()
        | fsm::when<State::Connecting, Event::Disconnected>()
            .to<State::Disconnected>()
        | fsm::when<State::Connecting, Event::FatalError>()
            .to<State::Failed>()
        | fsm::when<State::Connecting, Event::Shutdown>()
            .then(std::function<void()>{})
            .to<State::Disconnected>()
        | fsm::when<State::Registering, Event::HandshakeComplete>()
            .stay()
        | fsm::when<State::Registering, Event::RegistrationAck>()
            .then(std::function<void()>{})
            .to<State::Registered>()
        | fsm::when<State::Registering, Event::Disconnected>()
            .to<State::Reconnecting>()
        | fsm::when<State::Registering, Event::FatalError>()
            .to<State::Failed>()
        | fsm::when<State::Registering, Event::Shutdown>()
            .then(std::function<void()>{})
            .to<State::Disconnected>()
        | fsm::when<State::Registered, Event::HeartbeatTick>()
            .then(std::function<void()>{})
            .stay()
        | fsm::when<State::Registered, Event::HeartbeatAck>()
            .stay()
        | fsm::when<State::Registered, Event::Disconnected>()
            .to<State::Reconnecting>()
        | fsm::when<State::Registered, Event::Shutdown>()
            .then(std::function<void()>{})
            .to<State::Disconnected>()
        | fsm::when<State::Reconnecting, Event::Reconnect>()
            .then(std::function<void()>{})
            .to<State::Connecting>()
        | fsm::when<State::Reconnecting, Event::FatalError>()
            .to<State::Failed>()
        | fsm::when<State::Reconnecting, Event::Shutdown>()
            .then(std::function<void()>{})
            .to<State::Disconnected>()
        | fsm::when<State::Failed, Event::Shutdown>()
            .then(std::function<void()>{})
            .to<State::Disconnected>()
        | fsm::otherwise()
            .then(std::function<void()>{})
            .to<State::Failed>(),
        Sink{},
        State::Disconnected
    )) machine;
    
    explicit FsmImplT(AppRegistrationClient* c, Sink sink)
        : client(c)
        , machine(fsm::make_fsm(
            c->name(),
            // Disconnected state transitions
            fsm::when<State::Disconnected, Event::Connect>()
                .then(std::function<void()>([c](){ c->on_connect(); }))
                .to<State::Connecting>()
            | fsm::when<State::Disconnected, Event::Shutdown>()
                .stay()
            
            // Connecting state transitions
            | fsm::when<State::Connecting, Event::Connected>()
                .then(std::function<void()>([c](){ c->on_connected(); }))
                .to<State::Registering>()
            | fsm::when<State::Connecting, Event::Disconnected>()
                .to<State::Disconnected>()
            | fsm::when<State::Connecting, Event::FatalError>()
                .to<State::Failed>()
            | fsm::when<State::Connecting, Event::Shutdown>()
                .then(std::function<void()>([c](){ c->on_shutdown(); }))
                .to<State::Disconnected>()
            
            // Registering state transitions
            | fsm::when<State::Registering, Event::HandshakeComplete>()
                .stay()
            | fsm::when<State::Registering, Event::RegistrationAck>()
                .then(std::function<void()>([c](){ c->on_registration_ack(); }))
                .to<State::Registered>()
            | fsm::when<State::Registering, Event::Disconnected>()
                .to<State::Reconnecting>()
            | fsm::when<State::Registering, Event::FatalError>()
                .to<State::Failed>()
            | fsm::when<State::Registering, Event::Shutdown>()
                .then(std::function<void()>([c](){ c->on_shutdown(); }))
                .to<State::Disconnected>()
            
            // Registered state transitions
            | fsm::when<State::Registered, Event::HeartbeatTick>()
                .then(std::function<void()>([c](){ c->on_heartbeat_tick(); }))
                .stay()
            | fsm::when<State::Registered, Event::HeartbeatAck>()
                .stay()
            | fsm::when<State::Registered, Event::Disconnected>()
                .to<State::Reconnecting>()
            | fsm::when<State::Registered, Event::Shutdown>()
                .then(std::function<void()>([c](){ c->on_shutdown(); }))
                .to<State::Disconnected>()
            
            // Reconnecting state transitions
            | fsm::when<State::Reconnecting, Event::Reconnect>()
                .then(std::function<void()>([c](){ c->on_reconnect(); }))
                .to<State::Connecting>()
            | fsm::when<State::Reconnecting, Event::FatalError>()
                .to<State::Failed>()
            | fsm::when<State::Reconnecting, Event::Shutdown>()
                .then(std::function<void()>([c](){ c->on_shutdown(); }))
                .to<State::Disconnected>()
            
            // Failed state transitions
            | fsm::when<State::Failed, Event::Shutdown>()
                .then(std::function<void()>([c](){ c->on_shutdown(); }))
                .to<State::Disconnected>()
            
            // Otherwise clause - log invalid transitions
            | fsm::otherwise()
                .then(std::function<void()>([](){
                    // Sink handles logging
                }))
                .to<State::Failed>(),
            
            std::move(sink),
            State::Disconnected
        ))
    {
        client->state_ = State::Disconnected;
    }
    
    void process(Event event) {
        machine.process(event);
        client->state_ = machine.state();
    }
    
    State current() const {
        return machine.state();
    }
};

// Concrete type for production
struct AppRegistrationClient::FsmImpl : FsmImplT<AppRegistrationSink> {
    using FsmImplT<AppRegistrationSink>::FsmImplT;
};

// AppRegistrationSink implementation
void AppRegistrationSink::emit(const fsm::FsmEvent& e) const {
    if (e.kind == fsm::FsmEvent::Kind::Otherwise) {
        PROTOFLOW_LOG_ERROR(*client, "[" << e.fsm_name << "] Invalid transition: " 
            << e.from_state << " --[" << e.event << "]--> " << e.to_state);
    } else {
        PROTOFLOW_LOG_DEBUG(*client, "[" << e.fsm_name << "] " 
            << e.from_state << " --[" << e.event << "]--> " << e.to_state);
    }
}

// Constructor
AppRegistrationClient::AppRegistrationClient(Config config)
    : Service()
    , config_(std::move(config))
    , state_(State::Disconnected)
    , reconnect_count_(0)
    , current_backoff_delay_(config_.reconnect_delay)
    , reconnect_timer_initialized_(false)
{
    PROTOFLOW_LOG_INFO(*this, "Creating AppRegistrationClient: " << config_to_string(config_));
    create_fsm();
}

// Destructor
AppRegistrationClient::~AppRegistrationClient() {
    // TODO: Cleanup
}

// Outbound message queue
static thread_local std::queue<service::Message> outbound_queue_;

// Create FSM
void AppRegistrationClient::create_fsm() {
    fsm_ = std::make_unique<FsmImpl>(this, AppRegistrationSink{this});
}

// Create FSM with custom sink (for testing)
template<typename Sink>
void AppRegistrationClient::create_fsm_with_sink(Sink sink) {
    fsm_ = std::make_unique<FsmImplT<Sink>>(this, std::move(sink));
}

// Process FSM event
void AppRegistrationClient::process_event(Event event) {
    fsm_->process(event);
}

// Service lifecycle
void AppRegistrationClient::start() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Starting");
    process_event(Event::Connect);
}

void AppRegistrationClient::stop() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Stopping");
    process_event(Event::Shutdown);
}

void AppRegistrationClient::poll() {
    // Check timeouts and state transitions
    if (state_ == State::Registered) {
        check_heartbeat_timer();
    } else if (state_ == State::Connecting) {
        check_connection_timeout();
    } else if (state_ == State::Reconnecting) {
        check_reconnect_timer();
    }
}

// Handle incoming messages - dispatch to FSM events
void AppRegistrationClient::handle(service::Message&& msg) {
    if (msg.data.size() < 4) {
        PROTOFLOW_LOG_ERROR(*this, "Received malformed message (too small)");
        return;
    }
    
    // Parse protocol header: [response:2][size:2][payload]
    uint16_t cmd_val;
    uint16_t size_val;
    std::memcpy(&cmd_val, msg.data.data(), 2);
    std::memcpy(&size_val, msg.data.data() + 2, 2);
    
    auto cmd = static_cast<response>(cmd_val);
    std::span<const std::byte> payload(msg.data.data() + 4, size_val);
    
    PROTOFLOW_LOG_DEBUG(*this, "Received " << to_string(cmd) << " (" << size_val << " bytes)");
    
    // Dispatch based on response
    switch (cmd) {
        case response::hello_ack:
            handle_hello(payload);
            break;
        case response::register_ack:
            handle_register_ack(payload);
            break;
        case response::heartbeat_ack:
            handle_heartbeat_ack(payload);
            break;
        case response::error:
            PROTOFLOW_LOG_ERROR(*this, "Received error from server");
            process_event(Event::FatalError);
            break;
        default:
            PROTOFLOW_LOG_WARN(*this, "Unexpected request: " << to_string(cmd));
            break;
    }
}

// Generate outbound messages to runtime
std::vector<service::Message> AppRegistrationClient::generate_outbound() {
    std::vector<service::Message> messages;
    int count = 0;
    while (!outbound_queue_.empty()) {
        messages.push_back(std::move(outbound_queue_.front()));
        outbound_queue_.pop();
        count++;
    }
    if (count > 0) {
        std::cerr << "[CLIENT_DEBUG] Generating " << count << " outbound messages\n";
    }
    return messages;
}

// ============================================================================
// FSM Action Handlers
// ============================================================================

void AppRegistrationClient::on_connect() {
    PROTOFLOW_LOG_INFO(*this, "Initiating connection to registration server");
    connection_start_ = std::chrono::steady_clock::now();
    
    // In a real implementation, this would establish a TCP connection
    // For now, we're in Connecting state and waiting for a server response
}

void AppRegistrationClient::on_connected() {
    PROTOFLOW_LOG_INFO(*this, "Connection established, sending HELLO");
    
    // Send HELLO message
    hello_msg msg{};
    msg.version = config_.version;
    
    auto payload = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(&msg), 
        sizeof(msg)
    );
    
    outbound_queue_.push(make_protocol_message(request::hello, payload));
}

void AppRegistrationClient::on_handshake_complete() {
    PROTOFLOW_LOG_INFO(*this, "Handshake complete, registering app: " << config_.app_name);
    std::cerr << "[CLIENT] on_handshake_complete() - queuing REGISTER_APP\n";
    
    // Build REGISTER_APP message with endpoints
    std::vector<std::byte> payload;
    
    // App name (with length prefix)
    uint16_t name_len = static_cast<uint16_t>(config_.app_name.size());
    payload.resize(payload.size() + 2);
    std::memcpy(payload.data() + payload.size() - 2, &name_len, 2);
    payload.insert(payload.end(), 
                  reinterpret_cast<const std::byte*>(config_.app_name.data()),
                  reinterpret_cast<const std::byte*>(config_.app_name.data() + config_.app_name.size()));
    
    // Endpoint count and endpoints
    uint32_t ep_count = static_cast<uint32_t>(config_.endpoints.size());
    payload.resize(payload.size() + 4);
    std::memcpy(payload.data() + payload.size() - 4, &ep_count, 4);
    
    for (const auto& ep : config_.endpoints) {
        uint16_t ep_len = static_cast<uint16_t>(ep.size());
        payload.resize(payload.size() + 2);
        std::memcpy(payload.data() + payload.size() - 2, &ep_len, 2);
        payload.insert(payload.end(),
                      reinterpret_cast<const std::byte*>(ep.data()),
                      reinterpret_cast<const std::byte*>(ep.data() + ep.size()));
    }
    
    outbound_queue_.push(make_protocol_message(request::register_app, std::span<const std::byte>(payload)));
}

void AppRegistrationClient::on_registration_ack() {
    last_heartbeat_ = std::chrono::steady_clock::now();
    reset_backoff();
}

void AppRegistrationClient::on_heartbeat_tick() {
    PROTOFLOW_LOG_DEBUG(*this, "Sending heartbeat");
    
    heartbeat_msg msg{};
    msg.timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count());
    
    auto payload = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(&msg),
        sizeof(msg)
    );
    
    outbound_queue_.push(make_protocol_message(request::heartbeat, payload));
    last_heartbeat_ = std::chrono::steady_clock::now();
}

void AppRegistrationClient::on_heartbeat_ack() {
    last_heartbeat_ = std::chrono::steady_clock::now();
}

void AppRegistrationClient::on_disconnected() {
    PROTOFLOW_LOG_WARN(*this, "Connection lost");
    
    // Set up reconnection timer when entering Reconnecting state
    // (This will be called as a transition handler when we go to Reconnecting)
}

void AppRegistrationClient::on_reconnect() {
    ++reconnect_count_;
    PROTOFLOW_LOG_INFO(*this, "Reconnecting (attempt #" << reconnect_count_ 
                              << " with " << current_backoff_delay_.count() << "ms delay)");
    
    // Check if we've exceeded max attempts (0 = infinite)
    if (config_.max_reconnect_attempts > 0 && reconnect_count_ >= config_.max_reconnect_attempts) {
        PROTOFLOW_LOG_ERROR(*this, "Max reconnect attempts (" << config_.max_reconnect_attempts 
                                   << ") reached");
        process_event(Event::FatalError);
    }
}

void AppRegistrationClient::on_fatal_error() {
    PROTOFLOW_LOG_ERROR(*this, "Fatal error occurred, entering failed state");
    
    // Clear any pending messages
    while (!outbound_queue_.empty()) {
        outbound_queue_.pop();
    }
}

void AppRegistrationClient::on_shutdown() {
    PROTOFLOW_LOG_INFO(*this, "Shutting down gracefully");
    
    // Clear any pending messages
    while (!outbound_queue_.empty()) {
        outbound_queue_.pop();
    }
}

// ============================================================================
// App Registration Protocol Handlers
// ============================================================================

void AppRegistrationClient::handle_hello(std::span<const std::byte> payload) {
    if (payload.size() < sizeof(hello_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "Invalid HELLO_ACK size");
        process_event(Event::FatalError);
        return;
    }
    
    hello_ack ack;
    std::memcpy(&ack, payload.data(), sizeof(ack));
    
    PROTOFLOW_LOG_INFO(*this, "Received HELLO_ACK (server version: " << ack.version << ")");
    
    // Check version compatibility
    if (ack.version != config_.version) {
        PROTOFLOW_LOG_WARN(*this, "Version mismatch: client=" << config_.version 
                                   << " server=" << ack.version);
    }
    
    process_event(Event::HandshakeComplete);
}

void AppRegistrationClient::handle_register_ack(std::span<const std::byte> payload) {
    if (payload.size() < sizeof(register_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "Invalid REGISTER_ACK size");
        process_event(Event::FatalError);
        return;
    }
    
    register_ack ack;
    std::memcpy(&ack, payload.data(), sizeof(ack));
    
    if (ack.status == 0) {
        PROTOFLOW_LOG_INFO(*this, "Registration successful (app_id: " << ack.app_id << ")");
        process_event(Event::RegistrationAck);
    } else {
        PROTOFLOW_LOG_ERROR(*this, "Registration failed with status: " << ack.status);
        process_event(Event::FatalError);
    }
}

void AppRegistrationClient::handle_heartbeat_ack(std::span<const std::byte> payload) {
    if (payload.size() < sizeof(heartbeat_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "Invalid HEARTBEAT_ACK size");
        return;
    }
    
    heartbeat_ack ack;
    std::memcpy(&ack, payload.data(), sizeof(ack));
    
    PROTOFLOW_LOG_DEBUG(*this, "Received HEARTBEAT_ACK (timestamp: " << ack.timestamp << ")");
    process_event(Event::HeartbeatAck);
}

// ============================================================================
// Connection Management
// ============================================================================

void AppRegistrationClient::process_incoming_data(std::span<const std::byte> data) {
    // This method is for processing raw TCP data before it's framed into messages
    // In the current implementation, messages are already framed by the transport layer
    // This would be used if we needed to handle partial messages or buffering
    PROTOFLOW_LOG_DEBUG(*this, "Received raw data: " << data.size() << " bytes");
    
    // For now, wrap it in a Message and handle
    messaging::MessageHeader header;
    header.type = messaging::MessageTypes::Payload;
    handle(service::Message{header, data});
}

// ============================================================================
// Wait for Registration
// ============================================================================

bool AppRegistrationClient::wait_for_registered(uint32_t timeout_ms) noexcept {
    auto start = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    
    while (std::chrono::steady_clock::now() - start < timeout_duration) {
        // Poll to drive the FSM
        poll();
        
        if (state_ == State::Registered) {
            PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Successfully registered");
            return true;
        }
        
        if (state_ == State::Failed) {
            PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Registration failed");
            registration_error_ = "FSM reached Failed state";
            return false;
        }
        
        // Yield to other threads/tasks
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Registration timeout after " << timeout_ms << "ms (state=" 
                         << static_cast<int>(state_) << ")");
    registration_error_ = "Timeout waiting for registration";
    return false;
}

// ============================================================================
// Timing Checks
// ============================================================================

void AppRegistrationClient::check_heartbeat_timer() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat_);
    
    if (elapsed >= config_.heartbeat_interval) {
        process_event(Event::HeartbeatTick);
    }
}

void AppRegistrationClient::check_connection_timeout() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection_start_);
    
    if (elapsed >= config_.connection_timeout) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Connection timeout - no response received");
        // Don't change state without a response - transition back to Disconnected for retry
        process_event(Event::Disconnected);
    }
}

void AppRegistrationClient::check_reconnect_timer() {
    // Initialize timer on first check in Reconnecting state
    if (!reconnect_timer_initialized_) {
        reconnect_timer_ = std::chrono::steady_clock::now();
        reconnect_timer_initialized_ = true;
        PROTOFLOW_LOG_WARN(*this, "Waiting " << current_backoff_delay_.count() 
                                  << "ms before reconnection attempt");
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - reconnect_timer_);
    
    if (elapsed >= current_backoff_delay_) {
        PROTOFLOW_LOG_DEBUG(*this, "Reconnect timer elapsed, triggering reconnection");
        reconnect_timer_initialized_ = false;  // Reset for next Reconnecting state
        process_event(Event::Reconnect);
        
        // Reset timer for next backoff attempt
        reconnect_timer_ = std::chrono::steady_clock::now();
        
        // Increase backoff for next attempt (capped at 5 minutes)
        auto next_delay = current_backoff_delay_;
        next_delay = std::chrono::milliseconds(next_delay.count() * 2);
        const auto max_delay = std::chrono::minutes(5);
        if (next_delay > max_delay) {
            next_delay = max_delay;
        }
        current_backoff_delay_ = next_delay;
    }
}

void AppRegistrationClient::reset_backoff() {
    current_backoff_delay_ = config_.reconnect_delay;
    reconnect_count_ = 0;
    PROTOFLOW_LOG_DEBUG(*this, "Backoff reset");
}

std::chrono::milliseconds AppRegistrationClient::calculate_backoff_delay() {
    // Exponential backoff: delay * 2^(attempt - 1), up to 5 minutes
    auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(config_.reconnect_delay);
    for (uint32_t i = 1; i < reconnect_count_; ++i) {
        delay = std::chrono::milliseconds(delay.count() * 2);
        if (delay > std::chrono::minutes(5)) {
            delay = std::chrono::minutes(5);
            break;
        }
    }
    return delay;
}

} // namespace protoflow::app_registration_client
