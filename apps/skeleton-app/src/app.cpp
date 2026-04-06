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
    
    // Initialize logging (AppBase framework service)
    setup_logging();
    log_info("Initializing " + config_.app_name);
    
    // Initialize message router
    router_ = std::make_unique<messaging::Router>();

    // --- RPC Transport (TCP to main app) ---
    // App creates transport, connects it, then AppBase handles logging and storage
    auto tcp_client = std::make_unique<transport::tcp::tcp_client>(
        config_.server_address, config_.server_port);
    
    tcp_client->connect();  // Attempt connection (logging handled in setup_rpc_transport)
    
    std::string server_desc = "RPC server at " + config_.server_address + ":" + 
                              std::to_string(config_.server_port);
    setup_rpc_transport(std::move(tcp_client), server_desc);

    // --- App Registration Client (Service) ---
    // This service handles connection lifecycle, handshake, registration, and heartbeats
    // AppBase automatically routes messages between this service and the RPC transport
    app_registration_client::Config reg_config;
    reg_config.app_name = config_.app_name;
    reg_config.version = 1;
    reg_config.endpoints = config_.endpoints;
    reg_config.server_address = config_.server_address;
    reg_config.server_port = config_.server_port;
    reg_config.heartbeat_interval = config_.heartbeat_interval;

    // App registration client is a service—just add to services_ and let AppBase manage it
    services_.push_back(std::make_unique<app_registration_client::AppRegistrationClient>(
        std::move(reg_config)));
    log_info("AppRegistrationClient configured\n");

    // --- Hardware Client (optional) ---
    if (config_.enable_hw_client && !config_.hardware_resources.empty()) {
        auto tcp = std::make_unique<transport::tcp::tcp_client>(
            config_.server_address, config_.server_port);
        
        auto connect_result = tcp->connect();
        if (connect_result) {
            hw_transport_ = std::move(tcp);
            hw_client_ = std::make_unique<hw::hw_client>(*hw_transport_);
            log_info("HardwareClient connected\n");
        } else {
            log_warn("HardwareClient connection failed, will retry later\n");
        }
    }

    log_info(config_.app_name + " initialized\n");
    return true;
}

rpc::app_registration App::registration() const {
    return rpc::app_registration{
        .name = config_.app_name,
        .endpoints = config_.endpoints,
        .description = config_.app_name + " v" + config_.version
    };
}

std::string App::render_fragment(std::string_view fragment_id) {
    using namespace html;

    if (fragment_id != "status") {
        return {};
    }

    // Find registration client in services
    auto* reg_client = static_cast<app_registration_client::AppRegistrationClient*>(nullptr);
    for (const auto& svc : services_) {
        if (auto* client = dynamic_cast<app_registration_client::AppRegistrationClient*>(svc.get())) {
            reg_client = client;
            break;
        }
    }

    bool registered = reg_client && reg_client->is_registered();
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

std::string App::get_state_json() const {
    // Find registration client in services
    auto* reg_client = static_cast<app_registration_client::AppRegistrationClient*>(nullptr);
    for (const auto& svc : services_) {
        if (auto* client = dynamic_cast<app_registration_client::AppRegistrationClient*>(svc.get())) {
            reg_client = client;
            break;
        }
    }

    bool registered = reg_client && reg_client->is_registered();
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
