#include <protoflow/config/logging_config.hpp>
#include <algorithm>

namespace protoflow::config {

std::expected<LoggingConfig, IniError>
load_logging_config(const std::string& path) {
    auto ini_result = parse_ini_file(path);
    if (!ini_result) {
        return std::unexpected(ini_result.error());
    }

    return build_logging_config(*ini_result);
}

messaging::LogLevel
parse_log_level(const std::string& level_str) {
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "trace") return messaging::LogLevel::Trace;
    if (lower == "debug") return messaging::LogLevel::Debug;
    if (lower == "info") return messaging::LogLevel::Info;
    if (lower == "warn" || lower == "warning") return messaging::LogLevel::Warn;
    if (lower == "error") return messaging::LogLevel::Error;
    if (lower == "fatal") return messaging::LogLevel::Fatal;
    
    return messaging::LogLevel::Info; // Default
}

LoggingConfig
build_logging_config(const IniDocument& ini) {
    LoggingConfig config;
    
    // Parse [logging] section
    if (auto it = ini.find("logging"); it != ini.end()) {
        const auto& props = it->second;
        
        if (auto level_it = props.find("min_level"); level_it != props.end()) {
            config.min_level = parse_log_level(level_it->second);
        }
        
        if (auto max_it = props.find("max_stored_logs"); max_it != props.end()) {
            try {
                config.max_stored_logs = std::stoul(max_it->second);
            } catch (...) {}
        }
        
        if (auto console_it = props.find("console_output"); console_it != props.end()) {
            std::string val = console_it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            config.console_output = (val == "true" || val == "1" || val == "yes");
        }
    }
    
    // Parse sink sections [sink.*]
    for (const auto& [section_name, props] : ini) {
        if (section_name.starts_with("sink.")) {
            LogSink sink;
            
            if (auto type_it = props.find("type"); type_it != props.end()) {
                sink.type = type_it->second;
            } else {
                continue; // Type is required
            }
            
            if (auto path_it = props.find("path"); path_it != props.end()) {
                sink.path = path_it->second;
            }
            
            if (auto level_it = props.find("min_level"); level_it != props.end()) {
                sink.min_level = parse_log_level(level_it->second);
            } else {
                sink.min_level = config.min_level; // Inherit from global
            }
            
            config.sinks.push_back(std::move(sink));
        }
    }
    
    return config;
}

} // namespace protoflow::config