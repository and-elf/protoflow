#pragma once

#include <protoflow/service.hpp>
#include <protoflow/messaging.hpp>
#include <protoflow/logging.hpp>
#include <protoflow/rpc/protocol.hpp>

#include <memory>
#include <vector>
#include <string>
#include <atomic>

namespace protoflow::mainapp {

/// Main application runtime
/// Manages services, routes messages, and schedules execution
class App {

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
    ~App();

    /// Initialize runtime and all services
    bool initialize();

    /// Start runtime and begin message processing
    void run();

    /// Request shutdown
    void shutdown();

    /// Check if runtime is running
    [[nodiscard]] bool is_running() const { return running_.load(); }

private:
    /// Execute one scheduler cycle
    void cycle();

    void route_messages();

    Config config_;
    std::atomic<bool> running_{false};
    
    // Services
    std::vector<std::unique_ptr<service::Service>> services_;
    
    // Message router
    std::unique_ptr<messaging::Router> router_;
    
    // Logging (non-owning pointer, ownership held in services_)
    logging::LoggingService* logger_ = nullptr;
};

} // namespace protoflow::mainapp
