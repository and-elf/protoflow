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
    
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> result;
        
        // Serialize level (1 byte)
        result.push_back(static_cast<uint8_t>(level));
        
        // Serialize timestamp (8 bytes - milliseconds since epoch)
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        for (int i = 0; i < 8; ++i) {
            result.push_back(static_cast<uint8_t>((ms >> (i * 8)) & 0xFF));
        }
        
        // Serialize text length (4 bytes)
        uint32_t len = static_cast<uint32_t>(text.size());
        for (int i = 0; i < 4; ++i) {
            result.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
        
        // Serialize text data
        result.insert(result.end(), text.begin(), text.end());
        
        return result;
    }
    
    static LogMessage deserialize(std::span<const std::byte> data) {
        if (data.size() < 13) { // Minimum: 1 + 8 + 4 + 0
            return {};
        }
        
        size_t offset = 0;
        
        // Deserialize level (1 byte)
        auto level = static_cast<LogLevel>(static_cast<uint8_t>(data[offset++]));
        
        // Deserialize timestamp (8 bytes)
        uint64_t ms = 0;
        for (int i = 0; i < 8; ++i) {
            ms |= static_cast<uint64_t>(static_cast<uint8_t>(data[offset++])) << (i * 8);
        }
        auto timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(ms));
        
        // Deserialize text length (4 bytes)
        uint32_t len = 0;
        for (int i = 0; i < 4; ++i) {
            len |= static_cast<uint32_t>(static_cast<uint8_t>(data[offset++])) << (i * 8);
        }
        
        // Deserialize text data
        if (offset + len > data.size()) {
            return {};
        }
        std::string text;
        text.reserve(len);
        for (uint32_t i = 0; i < len; ++i) {
            text.push_back(static_cast<char>(data[offset++]));
        }
        
        return LogMessage{level, std::move(text), timestamp};
    }
};

} // namespace protoflow::messaging
