#include "app.hpp"
#include "services/app_registration_service.hpp"
#include "services/hardware_arbitration_service.hpp"
#include "services/http_service.hpp"
#include "services/http_listener_service.hpp"
#include "services/rpc_server_service.hpp"

#include <protoflow/config/hardware_config.hpp>
#include <protoflow/config/logging_config.hpp>
#include <iostream>

namespace protoflow::mainapp {

App::App(Config config)
    : AppBase(AppBase::Config{.cycle_time = std::chrono::milliseconds(10)}),
      config_(std::move(config)) {
}

bool App::initialize() {
    std::cout << "Initializing Protoflow Main Application Runtime...\n";

    // Initialize logging (AppBase handles service creation and management)
    std::cout << "  - Setting up logging...\n";
    auto log_config = config::load_logging_config(config_.log_config_path);
    if (!log_config) {
        std::cerr << "      Warning: Failed to load logging config from " 
                  << config_.log_config_path << "\n";
        std::cerr << "      Using default logging configuration\n";
    }
    
    auto logging_config = log_config.value_or(config::LoggingConfig{});
    auto logging_service = setup_logging();
    logging_service->set_console_output(logging_config.console_output);
    logging_service->set_min_level(logging_config.min_level);
    logging_service->set_max_stored_logs(logging_config.max_stored_logs);

    // Create services
    std::cout << "  - Creating services:\n";

    if (config_.enable_registration) {
        std::cout << "    * ApplicationRegistrationService\n";
        auto app_reg_service = std::make_unique<AppRegistrationService>();
        get_services().push_back(std::move(app_reg_service));
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
        get_services().push_back(std::move(hw_service));
    }

    if (config_.enable_http) {
        std::cout << "    * HTTPService (event-driven)\n";
        auto http_service = std::make_unique<HTTPService>();
        get_services().push_back(std::move(http_service));

        std::cout << "    * HttpListenerService (listening on "
                  << config_.listen_address << ":"
                  << config_.listen_port << ")\n";
        auto listener = std::make_unique<HttpListenerService>(
            config_.listen_address, config_.listen_port);
        get_services().push_back(std::move(listener));
    }

    // Initialize RPC server service if a transport is provided in config
    if (config_.rpc_server_transport) {
        auto rpc_server_service = std::make_unique<RpcServerService>(
            std::move(config_.rpc_server_transport),
            config_.rpc_server_config);
        std::cout << "    * RpcServerService (custom transport)\n";
        get_services().push_back(std::move(rpc_server_service));
    }

    std::cout << "Runtime initialized successfully.\n";
    return true;
}



} // namespace protoflow::mainapp
