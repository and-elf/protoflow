#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <algorithm>
#include <protoflow/config/ini_parser.hpp>
#include <protoflow/create_transport.hpp>
// Do not construct RPC transport here; runtime will create one if needed.
namespace protoflow::mainapp {

    App::Config get_config(const std::string& path) {
        App::Config config;

        auto res = protoflow::config::parse_ini_file(path);
        std::string rpc_server_address;
        uint16_t rpc_tcp_port = 9123;
        if (res) {
            const auto& doc = *res;
            // Look for [main] or [runtime] section
            const protoflow::config::IniSection* sec = nullptr;
            if (auto it = doc.find("main"); it != doc.end()) sec = &it->second;
            else if (auto it2 = doc.find("runtime"); it2 != doc.end()) sec = &it2->second;

            if (sec) {
                auto get = [&](const std::string& key) -> std::optional<std::string> {
                    if (auto it = sec->find(key); it != sec->end()) return it->second;
                    return std::nullopt;
                };

                if (auto v = get("listen_address")) config.listen_address = *v;
                if (auto v = get("listen_port")) {
                    try { config.listen_port = static_cast<uint16_t>(std::stoi(*v)); } catch(...) {}
                }
                if (auto v = get("hardware_config_path")) config.hardware_config_path = *v;
                if (auto v = get("log_config_path")) config.log_config_path = *v;
                if (auto v = get("enable_http")) {
                    std::string s = *v;
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    config.enable_http = (s == "1" || s == "true" || s == "yes");
                }
                if (auto v = get("enable_registration")) {
                    std::string s = *v;
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    config.enable_registration = (s == "1" || s == "true" || s == "yes");
                }
                if (auto v = get("enable_hardware_arbitration")) {
                    std::string s = *v;
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    config.enable_hardware_arbitration = (s == "1" || s == "true" || s == "yes");
                }
                if (auto v = get("rpc_server_transport")) {
                    std::string s = *v;
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    if (auto address = get("rpc_server_address")) {
                        rpc_server_address = *address;
                    }
                    if (auto port_str = get("rpc_server_port")) {
                        try { rpc_tcp_port = static_cast<uint16_t>(std::stoi(*port_str)); } catch(...) {}
                    }

                    // Record transport preference in config so Runtime can create it.
                    if (s == "tcp" || s == "unix") {
                        config.rpc_server_transport = protoflow::mainapp::create_server_transport(s, rpc_server_address, rpc_tcp_port);
                    }
                    else {
                        std::cerr << "Warning: Unsupported RPC server transport type '" << s << "' in config file\n";
                    }
                }
            }
        } else {
            std::cerr << "Warning: Failed to parse config file '" << path << "'\n";
        }
        return config;
    }
};