#include "runtime.hpp"
#include "services/app_registration_service.hpp"
#include "services/hardware_arbitration_service.hpp"
#include "services/http_service.hpp"
#include "services/rpc_server_service.hpp"

#include <protoflow/config/hardware_config.hpp>
#include <protoflow/config/logging_config.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>

namespace protoflow::mainapp {

namespace {
    std::atomic<Runtime*> g_runtime{nullptr};
    
    void signal_handler(int signal) {
        if (auto* runtime = g_runtime.load()) {
            std::cout << "\nReceived signal " << signal << ", shutting down...\n";
            runtime->shutdown();
        }
    }
}

Runtime::Runtime(Config config)
    : config_(std::move(config))
{
    g_runtime.store(this);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

Runtime::~Runtime() {
    if (running_.load()) {
        shutdown();
    }
    g_runtime.store(nullptr);
}

bool Runtime::initialize() {
    std::cout << "Initializing Protoflow Main Application Runtime...\n";

    // Initialize logging
    std::cout << "  - Setting up logging...\n";
    
    // Load logging configuration
    auto log_config = config::load_logging_config(config_.log_config_path);
    if (!log_config) {
        std::cerr << "      Warning: Failed to load logging config from " 
                  << config_.log_config_path << "\n";
        std::cerr << "      Using default logging configuration\n";
    }
    
    auto logging_config = log_config.value_or(config::LoggingConfig{});
    // Create logging service and transfer ownership to services_ vector
    auto logging_service = std::make_unique<logging::LoggingService>();
    logging_service->set_console_output(logging_config.console_output);
    logging_service->set_min_level(logging_config.min_level);
    logging_service->set_max_stored_logs(logging_config.max_stored_logs);
    logger_ = logging_service.get();
    services_.push_back(std::move(logging_service));

    // Initialize message router
    std::cout << "  - Initializing message router...\n";
    router_ = std::make_unique<messaging::Router>();

    // Create services
    std::cout << "  - Creating services:\n";

    if (config_.enable_registration) {
        std::cout << "    * ApplicationRegistrationService\n";
        auto app_reg_service = std::make_unique<AppRegistrationService>();
        services_.push_back(std::move(app_reg_service));
    }

    if (config_.enable_hardware_arbitration) {
        std::cout << "    * HardwareArbitrationService\n";
        
        // Load hardware configuration
        auto hw_config = config::load_hardware_config(config_.hardware_config_path);
        if (!hw_config) {
            std::cerr << "      Warning: Failed to load hardware config from " 
                      << config_.hardware_config_path << "\n";
            std::cerr << "      Starting with empty hardware configuration\n";
        }
        
        auto hw_service = std::make_unique<HardwareArbitrationService>(
            hw_config.value_or(config::HardwareConfig{})
        );
        services_.push_back(std::move(hw_service));
    }

    if (config_.enable_http) {
        std::cout << "    * HTTPService (listening on " 
                  << config_.listen_address << ":" 
                  << config_.listen_port << ")\n";
        auto http_service = std::make_unique<HTTPService>(
            config_.listen_address,
            config_.listen_port
        );
        services_.push_back(std::move(http_service));
    }

    // Initialize RPC server service if a transport is provided in config
    if (config_.rpc_server_transport) {
        auto rpc_server_service = std::make_unique<RpcServerService>(std::move(config_.rpc_server_transport));
        std::cout << "    * RpcServerService (custom transport)\n";
        services_.push_back(std::move(rpc_server_service));
    }
    // Start all services
    std::cout << "  - Starting services...\n";
    for (auto& service : services_) {
        service->start();
    }

    std::cout << "Runtime initialized successfully.\n";
    return true;
}

void Runtime::run() {
    if (services_.empty()) {
        std::cerr << "Error: No services configured. Runtime cannot start.\n";
        return;
    }

    running_.store(true);
    std::cout << "Runtime started. Processing messages...\n";

    using namespace std::chrono_literals;
    const auto cycle_time = 10ms; // 100Hz cycle rate

    while (running_.load()) {
        auto cycle_start = std::chrono::steady_clock::now();

        // Execute one scheduler cycle
        cycle();

        // Route messages between services
        route_messages();

        // Sleep to maintain cycle time
        auto elapsed = std::chrono::steady_clock::now() - cycle_start;
        if (elapsed < cycle_time) {
            std::this_thread::sleep_for(cycle_time - elapsed);
        }
    }

    std::cout << "Runtime stopped.\n";
}

void Runtime::shutdown() {
    std::cout << "Shutting down runtime...\n";
    running_.store(false);

    // Stop all services
    for (auto& service : services_) {
        service->stop();
    }

    std::cout << "All services stopped.\n";
}

void Runtime::cycle() {
    // One message per service per cycle (deterministic execution)
    for (auto& service : services_) {
        service->poll();
    }
}

void Runtime::route_messages() {
    // Collect outbound messages from all services and broadcast to other services
    for (auto& service_ptr : services_) {
        auto* service = service_ptr.get();

        // Pop outbound messages until none remain
        while (true) {
            auto opt = service->pop_outbound();
            if (!opt.has_value()) break;

            auto msg = std::move(*opt);

            // Deliver message to all other services (broadcast)
            for (auto& dest_ptr : services_) {
                if (dest_ptr.get() == service) continue;
                dest_ptr->on_message(std::move(msg));
            }
        }
    }
}

} // namespace protoflow::mainapp
