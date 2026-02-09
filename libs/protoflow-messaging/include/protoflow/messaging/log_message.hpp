#pragma once

#include <chrono>
#include <string>
#include <cstdint>

namespace protoflow::messaging {

/// Log message severity levels
enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

/// Convert log level to string
inline const char* to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

/// Log message containing level, text, and timestamp
/// Part of messaging to avoid circular dependencies
struct LogMessage {
    LogLevel level;
    std::string text;
    std::chrono::system_clock::time_point timestamp;
    
    LogMessage() = default;
    
    LogMessage(LogLevel lvl, std::string txt)
        : level(lvl)
        , text(std::move(txt))
        , timestamp(std::chrono::system_clock::now()) {}
    
    LogMessage(LogLevel lvl, std::string txt, std::chrono::system_clock::time_point ts)
        : level(lvl)
        , text(std::move(txt))
        , timestamp(ts) {}
};

} // namespace protoflow::messaging
