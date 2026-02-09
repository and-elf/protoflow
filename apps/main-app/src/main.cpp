#include "runtime.hpp"
#include <iostream>
#include <cstring>
#include <getopt.h>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Protoflow Main Application - Hardware arbitration and app registration\n\n"
              << "Options:\n"
              << "  -h, --help                   Show this help message\n"
              << "  -v, --version                Show version information\n"
              << "  -a, --address ADDRESS        Listen address (default: 0.0.0.0)\n"
              << "  -p, --port PORT              Listen port (default: 8080)\n"
              << "  -c, --hw-config PATH         Hardware config file (default: /etc/protoflow/hardware.conf)\n"
              << "  -l, --log-config PATH        Logging config file (default: /etc/protoflow/logging.conf)\n"
              << "  --no-http                    Disable HTTP service\n"
              << "  --no-registration            Disable app registration service\n"
              << "  --no-hardware                Disable hardware arbitration service\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " -p 9000\n"
              << "  " << program_name << " --address 127.0.0.1 --port 8080\n"
              << "  " << program_name << " --hw-config /path/to/hardware.conf\n"
              << "\n";
}

void print_version() {
    std::cout << "Protoflow Main Application v1.0.0\n"
              << "Built with C++23\n"
              << "Copyright (c) 2026 Protoflow Project\n";
}

int main(int argc, char* argv[]) {
    protoflow::mainapp::Runtime::Config config;

    // Command line options
    static struct option long_options[] = {
        {"help",           no_argument,       nullptr, 'h'},
        {"version",        no_argument,       nullptr, 'v'},
        {"address",        required_argument, nullptr, 'a'},
        {"port",           required_argument, nullptr, 'p'},
        {"hw-config",      required_argument, nullptr, 'c'},
        {"log-config",     required_argument, nullptr, 'l'},
        {"no-http",        no_argument,       nullptr, 1},
        {"no-registration",no_argument,       nullptr, 2},
        {"no-hardware",    no_argument,       nullptr, 3},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hva:p:c:l:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            
            case 'v':
                print_version();
                return 0;
            
            case 'a':
                config.listen_address = optarg;
                break;
            
            case 'p':
                try {
                    config.listen_port = static_cast<uint16_t>(std::stoi(optarg));
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number: " << optarg << "\n";
                    return 1;
                }
                break;
            
            case 'c':
                config.hardware_config_path = optarg;
                break;
            
            case 'l':
                config.log_config_path = optarg;
                break;
            
            case 1:  // --no-http
                config.enable_http = false;
                break;
            
            case 2:  // --no-registration
                config.enable_registration = false;
                break;
            
            case 3:  // --no-hardware
                config.enable_hardware_arbitration = false;
                break;
            
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check for extra arguments
    if (optind < argc) {
        std::cerr << "Error: Unexpected argument: " << argv[optind] << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Print startup banner
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Protoflow Main Application                              ║\n";
    std::cout << "║  Hardware Arbitration & App Registration                 ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Listen Address:      " << config.listen_address << "\n";
    std::cout << "  Listen Port:         " << config.listen_port << "\n";
    std::cout << "  Hardware Config:     " << config.hardware_config_path << "\n";
    std::cout << "  Logging Config:      " << config.log_config_path << "\n";
    std::cout << "  HTTP Service:        " << (config.enable_http ? "enabled" : "disabled") << "\n";
    std::cout << "  Registration:        " << (config.enable_registration ? "enabled" : "disabled") << "\n";
    std::cout << "  Hardware Arbitration:" << (config.enable_hardware_arbitration ? "enabled" : "disabled") << "\n";
    std::cout << "\n";

    // Verify we're running as root (required for hardware access)
    if (geteuid() != 0) {
        std::cerr << "Warning: Not running as root. Hardware access may be limited.\n";
        std::cerr << "         For full functionality, run with sudo or as root.\n\n";
    }

    // Create and initialize runtime
    try {
        protoflow::mainapp::Runtime runtime(config);

        if (!runtime.initialize()) {
            std::cerr << "Error: Failed to initialize runtime\n";
            return 1;
        }

        // Run runtime (blocks until shutdown)
        runtime.run();

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
