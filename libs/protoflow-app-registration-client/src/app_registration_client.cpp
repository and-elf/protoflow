#include <protoflow/app_registration_client/app_registration_client.hpp>

#ifdef PROTOFLOW_TEST_MODE
#include "test_logging_stub.hpp"
#else
#include <protoflow/logging/macros.hpp>
#endif

#include <protoflow/rpc/protocol.hpp>
#include <protoflow/fsm/fsm.hpp>
#include <protoflow/fsm/sinks.hpp>
#include <cstring>
#include <functional>

namespace protoflow::app_registration_client {

namespace {
    using namespace std::chrono_literals;
    
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
}

// FSM implementation using when-then-to-otherwise API
struct AppRegistrationClient::FsmImpl {
    using State = app_registration_protocol::State;
    using Event = app_registration_protocol::Event;
    
    // Use decltype to deduce the FSM type
    using FsmType = decltype(
        fsm::make_fsm(
            std::declval<std::string>(),
            std::declval<decltype(
                fsm::when<State::Disconnected, Event::Connect>()
                    .then(std::declval<std::function<void()>>())
                    .to<State::Connecting>()
                | fsm::otherwise()
                    .then(std::declval<std::function<void()>>())
                    .to<State::Disconnected>()
            )>(),
            std::declval<fsm::sinks::LoggingSink>(),
            std::declval<State>()
        )
    );
    
    FsmType machine;
    AppRegistrationClient* client;
    
    explicit FsmImpl(AppRegistrationClient* c)
        : machine(fsm::make_fsm(
            c->name(),
            // Disconnected state transitions
            fsm::when<State::Disconnected, Event::Connect>()
                .then([c](){ c->on_connect(); })
                .to<State::Connecting>()
            
            // Connecting state transitions
            | fsm::when<State::Connecting, Event::Connected>()
                .then([c](){ c->on_connected(); })
                .to<State::Registering>()
            | fsm::when<State::Connecting, Event::Disconnected>()
                .then([c](){ c->on_disconnected(); })
                .to<State::Disconnected>()
            | fsm::when<State::Connecting, Event::FatalError>()
                .then([c](){ c->on_fatal_error(); })
                .to<State::Failed>()
            
            // Registering state transitions
            | fsm::when<State::Registering, Event::HandshakeComplete>()
                .then([c](){ c->on_handshake_complete(); })
                .stay()
            | fsm::when<State::Registering, Event::RegistrationAck>()
                .then([c](){ c->on_registration_ack(); })
                .to<State::Registered>()
            | fsm::when<State::Registering, Event::Disconnected>()
                .then([c](){ c->on_disconnected(); })
                .to<State::Reconnecting>()
            | fsm::when<State::Registering, Event::FatalError>()
                .then([c](){ c->on_fatal_error(); })
                .to<State::Failed>()
            
            // Registered state transitions
            | fsm::when<State::Registered, Event::HeartbeatTick>()
                .then([c](){ c->on_heartbeat_tick(); })
                .stay()
            | fsm::when<State::Registered, Event::HeartbeatAck>()
                .then([c](){ c->on_heartbeat_ack(); })
                .stay()
            | fsm::when<State::Registered, Event::Disconnected>()
                .then([c](){ c->on_disconnected(); })
                .to<State::Reconnecting>()
            | fsm::when<State::Registered, Event::Shutdown>()
                .then([c](){ c->on_shutdown(); })
                .to<State::Disconnected>()
            
            // Reconnecting state transitions
            | fsm::when<State::Reconnecting, Event::Reconnect>()
                .then([c](){ c->on_reconnect(); })
                .to<State::Connecting>()
            | fsm::when<State::Reconnecting, Event::FatalError>()
                .then([c](){ c->on_fatal_error(); })
                .to<State::Failed>()
            | fsm::when<State::Reconnecting, Event::Shutdown>()
                .then([c](){ c->on_shutdown(); })
                .to<State::Disconnected>()
            
            // Failed state transitions
            | fsm::when<State::Failed, Event::Shutdown>()
                .then([c](){ c->on_shutdown(); })
                .to<State::Disconnected>()
            
            // Otherwise clause (required for compile-time validation)
            | fsm::otherwise()
                .then([c](){
                    PROTOFLOW_LOG_WARN(*c, "[" << c->name() << "] Invalid state transition");
                })
                .to<State::Failed>(),
            
            fsm::sinks::LoggingSink{c->name()},
            State::Disconnected
        ))
        , client(c)
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

// Constructor
AppRegistrationClient::AppRegistrationClient(Config config)
    : Service()
    , config_(std::move(config))
    , state_(State::Disconnected)
    , connection_id_(0)
    , connection_requested_(false)
    , reconnect_count_(0)
{
    PROTOFLOW_LOG_INFO(*this, "Creating AppRegistrationClient: " << config_to_string(config_));
    create_fsm();
}

// Destructor
AppRegistrationClient::~AppRegistrationClient() {
    if (is_connected() && connection_requested_) {
        disconnect_connection();
    }
}

// Create FSM
void AppRegistrationClient::create_fsm() {
    fsm_ = std::make_unique<FsmImpl>(this);
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

// Handle incoming messages from runtime
void AppRegistrationClient::handle(service::Message&& msg) {
    using namespace rpc_service;
    
    switch (msg.type()) {
        case RpcMessageTypes::Connected:
            handle_rpc_connected(RpcConnected::deserialize(msg.bytes()));
            break;
        case RpcMessageTypes::ConnectionFailed:
            handle_rpc_connection_failed(RpcConnectionFailed::deserialize(msg.bytes()));
            break;
        case RpcMessageTypes::Received:
            handle_rpc_received(RpcReceived::deserialize(msg.bytes()));
            break;
        case RpcMessageTypes::Disconnected:
            handle_rpc_disconnected(RpcDisconnected::deserialize(msg.bytes()));
            break;
        default:
            PROTOFLOW_LOG_WARN(*this, "[" << name() << "] Unexpected message type: " << msg.type());
            break;
    }
}

// Generate outbound messages to runtime
std::vector<service::Message> AppRegistrationClient::generate_outbound() {
    return {};
}

// ============================================================================
// FSM Action Handlers
// ============================================================================

void AppRegistrationClient::on_connect() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Initiating connection to " 
        << config_.server_address << ":" << config_.server_port);
    request_connection();
}

void AppRegistrationClient::on_connected() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] TCP connection established");
    send_hello();
}

void AppRegistrationClient::on_handshake_complete() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Handshake complete, sending registration");
    send_registration();
}

void AppRegistrationClient::on_registration_ack() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Registration acknowledged, entering active state");
    last_heartbeat_ = std::chrono::steady_clock::now();
    reconnect_count_ = 0;
}

void AppRegistrationClient::on_heartbeat_tick() {
    send_heartbeat();
}

void AppRegistrationClient::on_heartbeat_ack() {
    last_heartbeat_ = std::chrono::steady_clock::now();
}

void AppRegistrationClient::on_disconnected() {
    PROTOFLOW_LOG_WARN(*this, "[" << name() << "] Disconnected from server");
    disconnect_connection();
}

void AppRegistrationClient::on_reconnect() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Attempting reconnection (attempt " 
        << ++reconnect_count_ << ")");
    request_connection();
}

void AppRegistrationClient::on_fatal_error() {
    PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Fatal error occurred");
    disconnect_connection();
}

void AppRegistrationClient::on_shutdown() {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Shutting down");
    disconnect_connection();
}

// ============================================================================
// RPC Protocol Handlers
// ============================================================================

void AppRegistrationClient::send_hello() {
    using namespace app_registration_protocol;
    
    hello_msg msg{.version = config_.version};
    
    auto header = rpc::make_header(
        static_cast<uint16_t>(command::hello),
        sizeof(msg)
    );
    
    std::vector<std::byte> data;
    data.resize(sizeof(header) + sizeof(msg));
    std::memcpy(data.data(), &header, sizeof(header));
    std::memcpy(data.data() + sizeof(header), &msg, sizeof(msg));
    
    PROTOFLOW_LOG_DEBUG(*this, "[" << name() << "] Sending hello (version=" << config_.version << ")");
    send_rpc_data(std::move(data));
}

void AppRegistrationClient::send_registration() {
    using namespace app_registration_protocol;
    
    register_app_msg msg{};
    std::strncpy(msg.name, config_.app_name.c_str(), sizeof(msg.name) - 1);
    msg.endpoint_count = static_cast<uint32_t>(config_.endpoints.size());
    
    std::vector<std::byte> payload;
    payload.resize(sizeof(msg));
    std::memcpy(payload.data(), &msg, sizeof(msg));
    
    for (const auto& endpoint : config_.endpoints) {
        payload.insert(payload.end(), 
                      reinterpret_cast<const std::byte*>(endpoint.data()),
                      reinterpret_cast<const std::byte*>(endpoint.data() + endpoint.size() + 1));
    }
    
    auto header = rpc::make_header(
        static_cast<uint16_t>(command::register_app),
        static_cast<uint32_t>(payload.size())
    );
    
    std::vector<std::byte> data;
    data.resize(sizeof(header) + payload.size());
    std::memcpy(data.data(), &header, sizeof(header));
    std::memcpy(data.data() + sizeof(header), payload.data(), payload.size());
    
    PROTOFLOW_LOG_DEBUG(*this, "[" << name() << "] Sending registration (app=" 
        << config_.app_name << ", endpoints=" << config_.endpoints.size() << ")");
    send_rpc_data(std::move(data));
}

void AppRegistrationClient::send_heartbeat() {
    using namespace app_registration_protocol;
    
    heartbeat_msg msg{
        .timestamp = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count()
        )
    };
    
    auto header = rpc::make_header(
        static_cast<uint16_t>(command::heartbeat),
        sizeof(msg)
    );
    
    std::vector<std::byte> data;
    data.resize(sizeof(header) + sizeof(msg));
    std::memcpy(data.data(), &header, sizeof(header));
    std::memcpy(data.data() + sizeof(header), &msg, sizeof(msg));
    
    PROTOFLOW_LOG_DEBUG(*this, "[" << name() << "] Sending heartbeat");
    send_rpc_data(std::move(data));
}

void AppRegistrationClient::handle_hello_ack(std::span<const std::byte> payload) {
    using namespace app_registration_protocol;
    
    if (payload.size() < sizeof(hello_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Invalid hello_ack size: " << payload.size());
        process_event(Event::FatalError);
        return;
    }
    
    hello_ack msg;
    std::memcpy(&msg, payload.data(), sizeof(msg));
    
    PROTOFLOW_LOG_DEBUG(*this, "[" << name() << "] Received hello_ack (version=" << msg.version << ")");
    
    if (msg.version != config_.version) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Version mismatch: expected " 
            << config_.version << ", got " << msg.version);
        process_event(Event::FatalError);
        return;
    }
    
    process_event(Event::HandshakeComplete);
}

void AppRegistrationClient::handle_register_ack(std::span<const std::byte> payload) {
    using namespace app_registration_protocol;
    
    if (payload.size() < sizeof(register_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Invalid register_ack size: " << payload.size());
        process_event(Event::FatalError);
        return;
    }
    
    register_ack msg;
    std::memcpy(&msg, payload.data(), sizeof(msg));
    
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] Received register_ack (app_id=" 
        << msg.app_id << ", status=" << msg.status << ")");
    
    if (msg.status != 0) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Registration failed with status " << msg.status);
        process_event(Event::FatalError);
        return;
    }
    
    process_event(Event::RegistrationAck);
}

void AppRegistrationClient::handle_heartbeat_ack(std::span<const std::byte> payload) {
    using namespace app_registration_protocol;
    
    if (payload.size() < sizeof(heartbeat_ack)) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Invalid heartbeat_ack size: " << payload.size());
        return;
    }
    
    heartbeat_ack msg;
    std::memcpy(&msg, payload.data(), sizeof(msg));
    
    PROTOFLOW_LOG_DEBUG(*this, "[" << name() << "] Received heartbeat_ack (timestamp=" << msg.timestamp << ")");
    process_event(Event::HeartbeatAck);
}

// ============================================================================
// RPC Service Message Handlers
// ============================================================================

void AppRegistrationClient::handle_rpc_connected(const rpc_service::RpcConnected& msg) {
    PROTOFLOW_LOG_INFO(*this, "[" << name() << "] RPC connection established (id=" << msg.connection_id << ")");
    connection_id_ = msg.connection_id;
    connection_start_ = std::chrono::steady_clock::now();
    process_event(Event::Connected);
}

void AppRegistrationClient::handle_rpc_connection_failed(const rpc_service::RpcConnectionFailed& msg) {
    PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] RPC connection failed: " << msg.error);
    connection_requested_ = false;
    
    if (state_ == State::Connecting) {
        process_event(Event::Disconnected);
    } else if (state_ == State::Reconnecting) {
        process_event(Event::FatalError);
    }
}

void AppRegistrationClient::handle_rpc_received(const rpc_service::RpcReceived& msg) {
    if (msg.connection_id != connection_id_) {
        PROTOFLOW_LOG_WARN(*this, "[" << name() << "] Received data for unknown connection " << msg.connection_id);
        return;
    }
    
    process_incoming_data(msg.data);
}

void AppRegistrationClient::handle_rpc_disconnected(const rpc_service::RpcDisconnected& msg) {
    if (msg.connection_id != connection_id_) {
        return;
    }
    
    PROTOFLOW_LOG_WARN(*this, "[" << name() << "] RPC connection " << msg.connection_id << " disconnected");
    connection_requested_ = false;
    
    if (state_ == State::Registered || state_ == State::Registering) {
        process_event(Event::Disconnected);
    }
}

// ============================================================================
// Connection Management
// ============================================================================

void AppRegistrationClient::request_connection() {
    if (connection_requested_) {
        return;
    }
    
    rpc_service::RpcConnectRequest request;
    request.connection_id = connection_id_;
    request.host = config_.server_address;
    request.port = config_.server_port;
    request.connect_timeout_ms = static_cast<uint32_t>(config_.connection_timeout.count());
    request.read_timeout_ms = static_cast<uint32_t>(config_.read_timeout.count());
    request.write_timeout_ms = static_cast<uint32_t>(config_.write_timeout.count());
    
    messaging::MessageHeader hdr;
    hdr.type = rpc_service::RpcMessageTypes::ConnectRequest;
    write(service::Message{hdr, request.serialize()});
    connection_requested_ = true;
}

void AppRegistrationClient::send_rpc_data(std::vector<std::byte> data) {
    rpc_service::RpcSendRequest request;
    request.connection_id = connection_id_;
    request.data = std::move(data);
    
    messaging::MessageHeader hdr;
    hdr.type = rpc_service::RpcMessageTypes::SendRequest;
    write(service::Message{hdr, request.serialize()});
}

void AppRegistrationClient::disconnect_connection() {
    if (!connection_requested_) {
        return;
    }
    
    rpc_service::RpcDisconnectRequest request;
    request.connection_id = connection_id_;
    
    messaging::MessageHeader hdr;
    hdr.type = rpc_service::RpcMessageTypes::DisconnectRequest;
    write(service::Message{hdr, request.serialize()});
    connection_requested_ = false;
    connection_id_ = 0;
}

void AppRegistrationClient::process_incoming_data(std::span<const std::byte> data) {
    using namespace app_registration_protocol;
    
    if (data.size() < sizeof(rpc::rpc_header)) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Received data too small for RPC header");
        return;
    }
    
    rpc::rpc_header header;
    std::memcpy(&header, data.data(), sizeof(header));
    
    if (!header.is_valid()) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Invalid RPC header magic");
        return;
    }
    
    if (!header.version_compatible()) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Incompatible RPC version");
        process_event(Event::FatalError);
        return;
    }
    
    auto payload = data.subspan(sizeof(header));
    if (payload.size() < header.payload_size) {
        PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Incomplete payload");
        return;
    }
    
    auto cmd = static_cast<command>(header.cmd);
    
    switch (cmd) {
        case command::hello_ack:
            handle_hello_ack(payload);
            break;
            
        case command::register_ack:
            handle_register_ack(payload);
            break;
            
        case command::heartbeat_ack:
            handle_heartbeat_ack(payload);
            break;
            
        case command::error: {
            error_msg err;
            if (payload.size() >= sizeof(err)) {
                std::memcpy(&err, payload.data(), sizeof(err));
                PROTOFLOW_LOG_ERROR(*this, "[" << name() << "] Server error " 
                    << err.error_code << ": " << err.message);
            }
            process_event(Event::FatalError);
            break;
        }
            
        default:
            PROTOFLOW_LOG_WARN(*this, "[" << name() << "] Unknown command: " << header.cmd);
            break;
    }
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
