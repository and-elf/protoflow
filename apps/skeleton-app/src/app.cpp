#include "skeleton_app/app.hpp"

#include <protoflow/html.hpp>
#include <protoflow/transport/tcp.hpp>
#include <protoflow/config/logging_config.hpp>
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
    setup_logging(config::LoggingConfig{});
    log_info("Initializing " + config_.app_name);
    
    // Initialize message router
    router_ = std::make_unique<messaging::Router>();

    // --- RPC Connection Setup via Factory (clean API) ---
    // Create transport and attempt connection
    auto tcp_transport = std::make_unique<transport::tcp::tcp_client>(
        config_.server_address, config_.server_port);
    
    if (tcp_transport->connect()) {
        // Connection successful - configure the built-in app registration client
        // (transport management is separate from app registration protocol)
        configure_app_registration(config_.app_name,
                                  config_.endpoints);
        log_info("AppRegistrationClient configured\n");
    } else {
        log_warn("Failed to connect to RPC server at " + config_.server_address + ":" + 
                 std::to_string(config_.server_port) + ". Will retry in main loop.");
    }

    // --- Hardware Client (optional) ---
    if (config_.enable_hw_client && !config_.hardware_resources.empty()) {
        auto tcp = std::make_unique<transport::tcp::tcp_client>(
            config_.server_address, config_.server_port);
        
        auto hw_connect_result = tcp->connect();
        if (hw_connect_result) {
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

    // Get the built-in app registration client
    auto registered = is_registered();
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
    // Get the built-in app registration client
    auto registered = is_registered();
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
