#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace protoflow::config {

/// Returns the ordered list of directories to search for configuration files
/// for a given application name. Directories are returned in priority order
/// (user-level first, system-level last):
///
///   Linux / macOS:
///     1. $XDG_CONFIG_HOME/protoflow/<app_name>/  (defaults to ~/.config/...)
///     2. /etc/protoflow/<app_name>/
///
///   Windows:
///     1. %APPDATA%/protoflow/<app_name>/
///
/// Only directories that actually exist on disk are returned.
[[nodiscard]] std::vector<std::filesystem::path>
config_directories(const std::string& app_name);

/// Like config_directories(), but returns ALL candidate paths regardless of
/// whether they exist. Useful for diagnostics or creating default directories.
[[nodiscard]] std::vector<std::filesystem::path>
config_directory_candidates(const std::string& app_name);

/// Search the standard config directories for a specific file.
/// Returns the first match, or std::nullopt if the file is not found.
///
/// Example:
///   auto path = find_config_file("my-app", "hardware.conf");
///   // might return ~/.config/protoflow/my-app/hardware.conf
[[nodiscard]] std::optional<std::filesystem::path>
find_config_file(const std::string& app_name, const std::string& filename);

/// Search the standard config directories for all instances of a file.
/// Returns every match in priority order (user-level first).
[[nodiscard]] std::vector<std::filesystem::path>
find_all_config_files(const std::string& app_name, const std::string& filename);

/// Returns the preferred user-level config directory for the given app,
/// creating it if it does not exist. Returns std::nullopt on failure.
///
///   Linux / macOS: $XDG_CONFIG_HOME/protoflow/<app_name>/
///   Windows:       %APPDATA%/protoflow/<app_name>/
[[nodiscard]] std::optional<std::filesystem::path>
user_config_directory(const std::string& app_name, bool create = false);

/// Returns the system-level config directory (not created automatically).
///   Linux / macOS: /etc/protoflow/<app_name>/
///   Windows:       (same as user_config_directory)
[[nodiscard]] std::filesystem::path
system_config_directory(const std::string& app_name);

} // namespace protoflow::config
