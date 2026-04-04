#include "skeleton_app/app.hpp"

#include <protoflow/html.hpp>
#include <protoflow/transport/tcp.hpp>
#include <iostream>
#include <thread>
#include <csignal>

namespace protoflow::skeleton {

namespace {
    std::atomic<App*> g_app{nullptr};

    [[maybe_unused]] void signal_handler(int signal) {
        if (auto* app = g_app.load(); app != nullptr) {
            std::cout << "\nReceived signal " << signal << ", shutting down...\n";
            app->shutdown();
        }
    }
} // namespace

App::App(AppConfig config)
    : config_(std::move(config))
{
    g_app.store(this);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

App::~App() {
    if (running_.load()) {
        shutdown();
    }
    g_app.store(nullptr);
}

bool App::initialize() {
    std::cout << "Initializing " << config_.app_name << "...\n";

    // Initialize message router
    router_ = std::make_unique<messaging::Router>();

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

void App::run() {
    running_.store(true);
    std::cout << config_.app_name << " running.\n";

    while (running_.load()) {
        auto cycle_start = std::chrono::steady_clock::now();

        cycle();

        auto elapsed = std::chrono::steady_clock::now() - cycle_start;
        if (elapsed < config_.cycle_time) {
            std::this_thread::sleep_for(config_.cycle_time - elapsed);
        }
    }

    std::cout << config_.app_name << " stopped.\n";
}

void App::shutdown() {
    std::cout << "Shutting down " << config_.app_name << "...\n";
    running_.store(false);

    // Release any held hardware resources
    hw_client_.reset();
    hw_transport_.reset();

    // Stop services
    registration_client_->stop();

    std::cout << "All services stopped.\n";
}

void App::cycle() {
    // Poll registration client (handles connection, heartbeats, etc.)
    registration_client_->poll();

    // Poll other services
    for (auto& svc : services_) {
        svc->poll();
    }

    route_messages();
}

void App::route_messages() {
    // Collect outbound from registration client
    while (auto msg = registration_client_->pop_outbound()) {
        // Route to interested services
        for (auto& svc : services_) {
            auto types = svc->get_message_types();
            if (types.empty()) continue;
            for (auto t : types) {
                if (t == msg->type()) {
                    svc->on_message(messaging::Message(*msg));
                    break;
                }
            }
        }
    }

    // Collect outbound from other services
    for (auto& svc : services_) {
        while (auto msg = svc->pop_outbound()) {
            // Could route back to registration client or other services
        }
    }
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
