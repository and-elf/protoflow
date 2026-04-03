
#include "app.hpp"
#include <iostream>
#include <CLI/CLI.hpp>
#include <protoflow/config_parser.hpp>
#include <protoflow/create_transport.hpp>

constexpr const char* VERSION_STRING = "Protoflow Main Application v1.0.0\nBuilt with C++23\nCopyright (c) 2026 Protoflow Project\n";



int main(int argc, char* argv[]) {
    protoflow::mainapp::App::Config config;

    CLI::App app{"Protoflow Main Application - Hardware arbitration and app registration"};

    std::string listen_address = "0.0.0.0";
    uint16_t listen_port = 8080;
    std::string hw_config = config.hardware_config_path;
    std::string log_config = config.log_config_path;
    
    std::string rpc_server_transport = "tcp";
    std::string rpc_server_address = "0.0.0.0";
    uint16_t rpc_tcp_port = 9123;

    bool show_version = false;
    app.add_flag("-v,--version", show_version, "Show version information");

    std::string main_config_path;
    app.add_option("--config", main_config_path, "Path to main configuration INI file");

    app.add_option("-a,--address", listen_address, "Listen address (default: 0.0.0.0)");
    app.add_option("-p,--port", listen_port, "Listen port (default: 8080)");
    app.add_option("-c,--hw-config", hw_config, "Hardware config file (default: /etc/protoflow/hardware.conf)");
    app.add_option("-l,--log-config", log_config, "Logging config file (default: /etc/protoflow/logging.conf)");
    bool disable_http = false;
    bool disable_registration = false;
    bool disable_hardware = false;
    app.add_flag("--no-http", disable_http, "Disable HTTP service");
    app.add_flag("--no-registration", disable_registration, "Disable app registration service");
    app.add_flag("--no-hardware", disable_hardware, "Disable hardware arbitration service");

    app.add_option("--rpc-server-transport", rpc_server_transport, "RPC server transport type (tcp, unix)");
    app.add_option("--rpc-server-address", rpc_server_address, "RPC server listen address (for TCP) or socket path (for Unix)");
    app.add_option("--rpc-server-port", rpc_tcp_port, "RPC server TCP port (default: 9123)");

    CLI11_PARSE(app, argc, argv);

    // If CLI provided a config path, parse and apply (CLI options override file values)
    if (!main_config_path.empty()) {
        auto file_config = protoflow::mainapp::get_config(main_config_path);
        // Use values from file as base
        config = std::move(file_config);
    }

    config.listen_address = listen_address;
    config.listen_port = listen_port;
    config.hardware_config_path = hw_config;
    config.log_config_path = log_config;
    config.enable_http = !disable_http;
    config.enable_registration = !disable_registration;
    config.enable_hardware_arbitration = !disable_hardware;


    if (show_version) {
        std::cout << VERSION_STRING;
        return 0;
    }

    // Leave RPC server transport to runtime to construct as needed
    // Apply CLI overrides for transport preferences
    config.rpc_server_transport = protoflow::mainapp::create_server_transport(
        rpc_server_transport,
        rpc_server_address,
        rpc_tcp_port
    );
    try {
        protoflow::mainapp::App runtime_app(std::move(config));

        if (!runtime_app.initialize()) {
            std::cerr << "Error: Failed to initialize app\n";
            return 1;
        }

        // Run app (blocks until shutdown)
        runtime_app.run();

        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Protoflow Main Application Shutdown Complete            ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: Unknown exception\n";
        return 1;
    }
}
