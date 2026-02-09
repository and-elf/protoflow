#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <expected>

namespace protoflow::config {

/// INI section: key-value pairs
using IniSection = std::unordered_map<std::string, std::string>;

/// INI document: section name -> properties
using IniDocument = std::unordered_map<std::string, IniSection>;

/// Error codes for INI parsing
enum class IniError {
    file_not_found,
    parse_error
};

/// Parse an INI file
/// Returns map of section name -> key-value pairs
[[nodiscard]] std::expected<IniDocument, IniError>
parse_ini_file(const std::string& path);

/// Parse INI content from string
[[nodiscard]] std::expected<IniDocument, IniError>
parse_ini_string(const std::string& content);

} // namespace protoflow::config