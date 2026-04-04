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

namespace protoflow::app_registration_client {

namespace {
    using namespace std::chrono_literals;
    using namespace app_registration_protocol;
    
    // Convert config to string for logging
    std::string config_to_string(const Config& cfg) {
        std::ostringstream oss;
        oss << "{ app: " << cfg.app_name 
            << ", version: " << cfg.version
            << ", server: " << cfg.server_address << ":" << cfg.server_port
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
    // Check timeouts
    if (state_ == State::Registered) {
        check_heartbeat_timer();
    } else if (state_ == State::Connecting) {
        check_connection_timeout();
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
    while (!outbound_queue_.empty()) {
        messages.push_back(std::move(outbound_queue_.front()));
        outbound_queue_.pop();
    }
    return messages;
}

// ============================================================================
// FSM Action Handlers
// ============================================================================

void AppRegistrationClient::on_connect() {
    PROTOFLOW_LOG_INFO(*this, "Initiating connection to " << config_.server_address << ":" << config_.server_port);
    connection_start_ = std::chrono::steady_clock::now();
    
    // In a real implementation, this would initiate TCP connection
    // For now, immediately transition to Connected (will be replaced with actual TCP transport)
    process_event(Event::Connected);
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
    
    // Send REGISTER_APP message
    register_app_msg msg{};
    std::strncpy(msg.name, config_.app_name.c_str(), sizeof(msg.name) - 1);
    msg.endpoint_count = static_cast<uint32_t>(config_.endpoints.size());
    
    // For now, just send the base message (endpoints would be appended in full impl)
    auto payload = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(&msg),
        sizeof(msg)
    );
    
    outbound_queue_.push(make_protocol_message(request::register_app, payload));
}

void AppRegistrationClient::on_registration_ack() {
    last_heartbeat_ = std::chrono::steady_clock::now();
    reconnect_count_ = 0;
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
    
    // Will transition to Reconnecting state via FSM
}

void AppRegistrationClient::on_reconnect() {
    ++reconnect_count_;
    PROTOFLOW_LOG_INFO(*this, "Reconnecting (attempt #" << reconnect_count_ << ")");
    
    if (reconnect_count_ >= config_.max_reconnect_attempts) {
        PROTOFLOW_LOG_ERROR(*this, "Max reconnect attempts reached");
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
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Connection timeout");
        process_event(Event::FatalError);
    }
}

} // namespace protoflow::app_registration_client
