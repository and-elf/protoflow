#pragma once

#include <protoflow/config/ini_parser.hpp>
#include <protoflow/logging/log_message.hpp>
#include <string>
#include <vector>
#include <expected>

namespace protoflow::config {

/// Logging output sink configuration
struct LogSink {
    std::string type;           // "console", "file", "syslog"
    std::string path;           // For file sinks
    messaging::LogLevel min_level = messaging::LogLevel::Info;
};

/// Logging configuration
struct LoggingConfig {
    messaging::LogLevel min_level = messaging::LogLevel::Info;
    size_t max_stored_logs = 1000;  // 0 = unlimited
    bool console_output = true;
    std::vector<LogSink> sinks;
};

/// Load logging configuration from INI-style file
[[nodiscard]] std::expected<LoggingConfig, IniError>
load_logging_config(const std::string& path);

/// Build logging config from parsed INI document
[[nodiscard]] LoggingConfig
build_logging_config(const IniDocument& ini);

/// Parse log level from string
[[nodiscard]] messaging::LogLevel
parse_log_level(const std::string& level_str);

} // namespace protoflow::config