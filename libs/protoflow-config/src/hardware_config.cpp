#include <protoflow/config/hardware_config.hpp>

namespace protoflow::config {

std::expected<HardwareConfig, IniError> 
load_hardware_config(const std::string& path) {
    auto ini_result = parse_ini_file(path);
    if (!ini_result) {
        return std::unexpected(ini_result.error());
    }

    return build_hardware_config(*ini_result);
}

HardwareConfig
build_hardware_config(const IniDocument& ini) {
    HardwareConfig config;
    
    for (const auto& [section_name, properties] : ini) {
        // Skip empty section names (global properties)
        if (section_name.empty()) continue;
        
        if (auto resource = parse_hardware_resource(properties)) {
            config[section_name] = *resource;
        }
    }
    
    return config;
}

std::optional<HardwareResource>
parse_hardware_resource(const IniSection& properties) {
    HardwareResource resource;
    
    // Device is required
    auto device_it = properties.find("device");
    if (device_it == properties.end()) {
        return std::nullopt;
    }
    resource.device = device_it->second;

    // Mode (default: exclusive)
    if (auto it = properties.find("mode"); it != properties.end()) {
        resource.mode = it->second;
    } else {
        resource.mode = "exclusive";
    }

    // Max clients (default: 1)
    if (auto it = properties.find("max_clients"); it != properties.end()) {
        try {
            resource.max_clients = std::stoi(it->second);
        } catch (...) {
            resource.max_clients = 1;
        }
    }

    // Timeout (default: 30s)
    if (auto it = properties.find("timeout"); it != properties.end()) {
        const auto& value = it->second;
        if (!value.empty() && value.back() == 's') {
            try {
                int seconds = std::stoi(value.substr(0, value.size() - 1));
                resource.timeout = std::chrono::seconds(seconds);
            } catch (...) {
                resource.timeout = std::chrono::seconds(30);
            }
        }
    }

    // Capabilities (default: read + write)
    // This would be extended based on the actual hw::capability_flags
    resource.capabilities = 0x03; // can_read | can_write

    return resource;
}

} // namespace protoflow::config
