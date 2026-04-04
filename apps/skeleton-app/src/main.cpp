// Protoflow Skeleton Client Application
//
// This is a template/reference app showing how to build a protoflow client
// that registers with the main app, optionally requests hardware access,
// and renders HTML fragments.
//
// Usage:
//   protoflow-skeleton-app [options]
//
// Copy this app as a starting point for new applications.

#include "skeleton_app/app.hpp"
#include <CLI/CLI.hpp>
#include <iostream>

constexpr const char* VERSION_STRING =
    "Protoflow Skeleton Application v1.0.0\n"
    "Built with C++23\n";

int main(int argc, char* argv[]) {
    protoflow::skeleton::AppConfig config;

    CLI::App cli{"Protoflow Skeleton Application - template for client apps"};

    bool show_version = false;
    cli.add_flag("-v,--version", show_version, "Show version information");

    cli.add_option("-n,--name", config.app_name,
        "Application name (default: skeleton-app)");
    cli.add_option("-s,--server-address", config.server_address,
        "Main app RPC server address (default: 127.0.0.1)");
    cli.add_option("-p,--server-port", config.server_port,
        "Main app RPC server port (default: 9123)");
    cli.add_option("-e,--endpoints", config.endpoints,
        "HTTP endpoints to register (default: /skeleton, /skeleton/status)");
    cli.add_option("--hw-resources", config.hardware_resources,
        "Hardware resources to request access to");
    cli.add_flag("--enable-hw", config.enable_hw_client,
        "Enable hardware client");

    CLI11_PARSE(cli, argc, argv);

    if (show_version) {
        std::cout << VERSION_STRING;
        return 0;
    }

    try {
        protoflow::skeleton::App app(std::move(config));

        if (!app.initialize()) {
            std::cerr << "Error: Failed to initialize app\n";
            return 1;
        }

        app.run();

        std::cout << "\nSkeleton app shutdown complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
