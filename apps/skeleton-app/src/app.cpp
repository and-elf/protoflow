#include "skeleton_app/app.hpp"

#include <protoflow/html.hpp>
#include <protoflow/transport/tcp.hpp>
#include <iostream>
#include <cstring>

namespace protoflow::skeleton {

App::App(AppConfig config)
    : AppBase(AppBase::Config{.cycle_time = config.cycle_time}),
      config_(std::move(config)) {
}

App::~App() = default;

bool App::initialize() {
    std::cout << "Initializing " << config_.app_name << "...\n";

    // Initialize message router
    router_ = std::make_unique<messaging::Router>();

    // --- RPC Transport (TCP to main app) ---
    // Create and establish TCP connection to main app's RPC server
    auto tcp_client = std::make_unique<transport::tcp::tcp_client>(
        config_.server_address, config_.server_port);
    
    auto tcp_connect_result = tcp_client->connect();
    if (tcp_connect_result) {
        std::cout << "  - Connected to RPC server at " 
                  << config_.server_address << ":" << config_.server_port << "\n";
        rpc_transport_ = std::move(tcp_client);
        
        std::cout << "  - Sending app registration over RPC...\n";
        
        // Manually send app registration directly using proper protocol types
        // Format: [request:2][size:2][payload]
        // request = AppRegistrationRequests + 1 (register_app = 121)
        std::vector<std::byte> reg_payload;
        
        // Add app name (with length prefix)
        uint16_t name_len = static_cast<uint16_t>(config_.app_name.size());
        reg_payload.resize(reg_payload.size() + 2);
        std::memcpy(reg_payload.data() + reg_payload.size() - 2, &name_len, 2);
        reg_payload.insert(reg_payload.end(), 
                          reinterpret_cast<const std::byte*>(config_.app_name.data()),
                          reinterpret_cast<const std::byte*>(config_.app_name.data() + config_.app_name.size()));
        
        // Add endpoint count and endpoints
        uint32_t ep_count = static_cast<uint32_t>(config_.endpoints.size());
        reg_payload.resize(reg_payload.size() + 4);
        std::memcpy(reg_payload.data() + reg_payload.size() - 4, &ep_count, 4);
        
        for (const auto& ep : config_.endpoints) {
            uint16_t ep_len = static_cast<uint16_t>(ep.size());
            reg_payload.resize(reg_payload.size() + 2);
            std::memcpy(reg_payload.data() + reg_payload.size() - 2, &ep_len, 2);
            reg_payload.insert(reg_payload.end(),
                              reinterpret_cast<const std::byte*>(ep.data()),
                              reinterpret_cast<const std::byte*>(ep.data() + ep.size()));
        }
        
        // Create the RPC protocol message
        std::vector<std::byte> rpc_msg;
        rpc_msg.resize(4 + reg_payload.size());
        
        // Use proper protocol value: AppRegistrationRequests (120) + 1 for register_app = 121
        uint16_t cmd = 121;  // request::register_app
        uint16_t payload_size = static_cast<uint16_t>(reg_payload.size());
        std::memcpy(rpc_msg.data(), &cmd, 2);
        std::memcpy(rpc_msg.data() + 2, &payload_size, 2);
        if (!reg_payload.empty()) {
            std::memcpy(rpc_msg.data() + 4, reg_payload.data(), reg_payload.size());
        }
        
        bool sent = rpc_transport_->send(std::span<const std::byte>(rpc_msg));  
        std::cout << "  - Registration message " << (sent ? "sent" : "FAILED TO SEND") 
                  << " (cmd=121, size=" << payload_size << ")\n";
    } else {
        std::cerr << "  - Failed to connect to RPC server at " 
                  << config_.server_address << ":" << config_.server_port << "\n";
        rpc_transport_ = std::move(tcp_client);
    }
    
    // --- App Registration Client ---
    // This connects to the main app's RPC server, performs handshake,
    // registers the app, and maintains a heartbeat.
    app_registration_client::Config reg_config;
    reg_config.app_name = config_.app_name;
    reg_config.version = 1;
    reg_config.endpoints = config_.endpoints;
    reg_config.server_address = config_.server_address;
    reg_config.server_port = config_.server_port;
    reg_config.heartbeat_interval = config_.heartbeat_interval;

    registration_client_ = std::make_unique<app_registration_client::AppRegistrationClient>(
        std::move(reg_config));

    std::cout << "  - AppRegistrationClient (server=" 
              << config_.server_address << ":" << config_.server_port << ")\n";

    // --- Hardware Client (optional) ---
    if (config_.enable_hw_client && !config_.hardware_resources.empty()) {
        // Create a TCP transport to the main app for hardware arbitration.
        // In production, this could be a separate port or multiplexed over
        // the same RPC connection.
        auto tcp = std::make_unique<transport::tcp::tcp_client>(
            config_.server_address, config_.server_port);
        
        auto connect_result = tcp->connect();
        if (connect_result) {
            hw_transport_ = std::move(tcp);
            hw_client_ = std::make_unique<hw::hw_client>(*hw_transport_);
            std::cout << "  - HardwareClient (connected)\n";
        } else {
            std::cerr << "  - HardwareClient (connection failed, will retry later)\n";
        }
    }

    // Start services
    std::cout << "  - Starting services...\n";
    registration_client_->start();

    std::cout << config_.app_name << " initialized.\n";
    return true;
}

void App::cycle() {
    if (registration_client_) {
        registration_client_->poll();
    }

    // transport I/O for the registration client
    while (auto msg = registration_client_->pop_outbound()) {
        if (rpc_transport_ && rpc_transport_->is_connected()) {
            if (!rpc_transport_->send(std::span<const std::byte>(msg->data))) {
                rpc_transport_->close();
                break;
            }
        }
    }

    if (rpc_transport_ && rpc_transport_->is_connected()) {
        auto result = rpc_transport_->receive(8192);
        if (result.has_value() && !result->empty()) {
            messaging::MessageHeader header;
            header.type = messaging::MessageTypes::Payload;
            messaging::Message m{header, std::move(result.value())};
            registration_client_->on_message(std::move(m));
        }
    }

    AppBase::cycle();
}

// --- rpc_app interface ---

rpc::app_registration App::registration() const {
    return rpc::app_registration{
        .name = config_.app_name,
        .endpoints = config_.endpoints,
        .description = config_.app_name + " v" + config_.version
    };
}

std::string App::render_fragment(std::string_view fragment_id) {
    using namespace html;

    if (fragment_id == "status") {
        bool registered = registration_client_ && registration_client_->is_registered();
        bool hw_available = hw_client_ != nullptr;

        // Generic lambda lets us compose nodes whose badge types differ
        auto build_status = [&](auto reg_badge, auto hw_badge) {
            return to_html(
                div(attrs<class_<"skeleton-status">>{},
                    card(
                        h3(text(config_.app_name)),
                        data_row(text("Registered:"), std::move(reg_badge)),
                        data_row(text("Hardware:"),   std::move(hw_badge))
                    )
                )
            );
        };

        if (registered && hw_available)
            return build_status(badge_success(text("yes")),       badge_success(text("available")));
        if (registered)
            return build_status(badge_success(text("yes")),       badge_warning(text("n/a")));
        if (hw_available)
            return build_status(badge_error(text("no")),          badge_success(text("available")));
        return build_status(badge_error(text("no")),              badge_warning(text("n/a")));
    }
    return {};
}

std::string App::get_state_json() const {
    bool registered = registration_client_ && registration_client_->is_registered();
    bool hw_available = hw_client_ != nullptr;
    return "{\"app\":\"" + config_.app_name + "\","
           "\"version\":\"" + config_.version + "\","
           "\"registered\":" + (registered ? "true" : "false") + ","
           "\"hw_available\":" + (hw_available ? "true" : "false") + "}";
}

bool App::handle_custom_command(
    uint16_t command,
    [[maybe_unused]] std::span<const std::byte> payload,
    std::vector<std::byte>& response)
{
    // Example: handle a custom "ping" command (command code 1000)
    if (command == 1000) {
        std::string reply = "pong";
        response.resize(reply.size());
        std::memcpy(response.data(), reply.data(), reply.size());
        return true;
    }
    return false;
}

} // namespace protoflow::skeleton
