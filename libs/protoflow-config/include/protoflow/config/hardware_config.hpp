#pragma once

#include <protoflow/config/ini_parser.hpp>
#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <expected>

namespace protoflow::config {

/// Hardware resource configuration
struct HardwareResource {
    std::string device;      // e.g., /dev/ttyUSB0
    std::string mode;        // "exclusive" or "shared"
    int max_clients = 1;     // for shared resources
    std::chrono::seconds timeout{30};
    uint32_t capabilities = 0;  // Bitmask of capability flags
};

/// Hardware configuration map: resource name -> resource config
using HardwareConfig = std::unordered_map<std::string, HardwareResource>;

/// Load hardware configuration from INI-style file
/// Returns map of resource name -> resource config
[[nodiscard]] std::expected<HardwareConfig, IniError> 
load_hardware_config(const std::string& path);

/// Build hardware config from parsed INI document
[[nodiscard]] HardwareConfig
build_hardware_config(const IniDocument& ini);

/// Parse a single hardware resource from INI section
/// Used for testing or programmatic config building
[[nodiscard]] std::optional<HardwareResource>
parse_hardware_resource(const IniSection& properties);

} // namespace protoflow::config
