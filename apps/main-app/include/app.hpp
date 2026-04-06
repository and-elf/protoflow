#pragma once

#include <protoflow/service.hpp>
#include <protoflow/messaging.hpp>
#include <protoflow/logging.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/runtime/app_base.hpp>

#include <memory>
#include <string>

namespace protoflow::mainapp {

/// Main application runtime
/// Manages services, routes messages, and schedules execution
class App : public runtime::AppBase {

public:
    struct Config {
        std::string listen_address = "0.0.0.0";
        uint16_t listen_port = 8080;
        std::string hardware_config_path = "/etc/protoflow/hardware.conf";
        std::string log_config_path = "/etc/protoflow/logging.conf";
        bool enable_http = true;
        bool enable_registration = true;
        bool enable_hardware_arbitration = true;

        // Optional: Pre-constructed RPC transport (server)
        std::unique_ptr<protoflow::rpc::transport_interface> rpc_server_transport;
    };

    explicit App(Config config);

    /// Initialize runtime and all services
    bool initialize() override;

protected:
    /// Execute one scheduler cycle
    void cycle() override;

private:
    Config config_;
    
    // Message router
    std::unique_ptr<messaging::Router> router_;
    
    // Logging (non-owning pointer, ownership held in services_)
    logging::LoggingService* logger_ = nullptr;
};

} // namespace protoflow::mainapp
