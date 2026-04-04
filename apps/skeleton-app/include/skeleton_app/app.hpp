#pragma once

#include <protoflow/service/service.hpp>
#include <protoflow/messaging/message.hpp>
#include <protoflow/messaging/router.hpp>
#include <protoflow/rpc/rpc_app.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/runtime/app_base.hpp>
#include <protoflow/app_registration_client/app_registration_client.hpp>
#include <protoflow/app_registration_client/config.hpp>
#include <protoflow/hw/hw_client.hpp>
#include <protoflow/logging/log_message.hpp>

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <optional>

namespace protoflow::skeleton {

/// Configuration for the skeleton client application
struct AppConfig {
    /// Application name (used for registration with main app)
    std::string app_name = "skeleton-app";

    /// Application version string
    std::string version = "1.0.0";

    /// Server address (main app RPC)
    std::string server_address = "127.0.0.1";

    /// Server port (main app RPC)
    uint16_t server_port = 9123;

    /// HTTP endpoints this app provides
    std::vector<std::string> endpoints = {"/skeleton", "/skeleton/status"};

    /// Hardware resources to request (empty = no hardware needed)
    std::vector<std::string> hardware_resources;

    /// Heartbeat interval for registration keepalive
    std::chrono::seconds heartbeat_interval{30};

    /// Enable hardware client
    bool enable_hw_client = false;

    /// Main loop cycle time
    std::chrono::milliseconds cycle_time{10};
};

/// Skeleton client application
///
/// This is a template/skeleton for implementing protoflow client apps.
/// It demonstrates integration with:
///   - App registration (FSM-driven connection to main app)
///   - Hardware arbitration client (request/release device access)
///   - RPC client (custom command handling)
///   - Service/messaging framework
///   - Logging
///   - HTML fragment rendering
///
/// Copy this app as a starting point for new applications.
class App : public runtime::AppBase, public rpc::rpc_app {
public:
    explicit App(AppConfig config);
    ~App() override;

    // --- Lifecycle ---

    /// Initialize all services and internal state
    bool initialize() override;

    // --- rpc_app interface ---

    [[nodiscard]] rpc::app_registration registration() const override;
    [[nodiscard]] std::string render_fragment(std::string_view fragment_id) override;
    [[nodiscard]] std::string get_state_json() const override;
    [[nodiscard]] bool handle_custom_command(
        uint16_t command,
        std::span<const std::byte> payload,
        std::vector<std::byte>& response) override;

    // --- Accessors for testing ---

    /// Get app registration client (may be null if not yet initialized)
    [[nodiscard]] app_registration_client::AppRegistrationClient* 
    registration_client() const noexcept { return registration_client_.get(); }

    /// Get current configuration
    [[nodiscard]] const AppConfig& config() const noexcept { return config_; }

protected:
    /// Override cycle to add app-specific logic after base cycle
    void cycle() override;

    /// Override route_messages for app-specific message routing
    void route_messages() override;

private:
    AppConfig config_;

    // Services
    std::unique_ptr<hw::hw_client> hw_client_;
};

} // namespace protoflow::skeleton
