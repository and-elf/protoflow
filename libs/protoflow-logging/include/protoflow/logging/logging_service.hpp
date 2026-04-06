#pragma once

#include <protoflow/service/service.hpp>
#include <protoflow/logging/log_message.hpp>
#include <vector>
#include <functional>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <optional>
#include <string>
#include <ranges>

namespace protoflow::logging {

/// Message types for logging service
enum class LogMessageType : uint32_t {
    Log = 1,
    LogRequest = 2,
    LogResponse = 3
};

/// Message: Request to query/filter stored logs
struct LogRequest {
    messaging::ServiceId requester_id;                  // Service requesting the logs
    uint64_t request_id;                                // For matching responses
    std::optional<messaging::ServiceId> service_id;     // Filter by service
    std::optional<Level> min_level;                     // Minimum log level
    std::optional<std::chrono::system_clock::time_point> start_time;  // Time range start
    std::optional<std::chrono::system_clock::time_point> end_time;    // Time range end
    std::optional<std::string> text_filter;             // Text substring search
    size_t limit = 100;                                 // Maximum results
    size_t offset = 0;                                  // Pagination offset
    
    std::vector<uint8_t> serialize() const { return {}; }  // TODO: implement
    static LogRequest deserialize(std::span<const std::byte>) { return {}; }  // TODO
};

/// Message: Response containing filtered logs
struct LogResponse {
    messaging::ServiceId requester_id;                  // Original requester
    uint64_t request_id;                                // Matches the request
    std::vector<LogMessage> logs;                       // Filtered log messages
    size_t total_matches;                               // Total matching logs (before limit/offset)
    
    std::vector<uint8_t> serialize() const { return {}; }  // TODO: implement
    static LogResponse deserialize(std::span<const std::byte>) { return {}; }  // TODO
};

/// Internal structure to store logs with their source
struct StoredLog {
    LogMessage log;
    messaging::ServiceId source_id;
};

/// Service that collects and manages log messages from all services
/// Other services send LogMessage via the messaging system
/// LoggingService can filter, store, and forward logs to sinks
class LoggingService : public service::Service {
public:
    LoggingService() = default;
    
    /// Set minimum log level to process (default: Info)
    void set_min_level(Level min_level) {
        min_level_ = min_level;
    }
    
    /// Enable/disable console output (enabled by default)
    void set_console_output(bool enabled) {
        console_output_ = enabled;
    }
    
    /// Get stored log messages
    [[nodiscard]] const std::vector<StoredLog>& get_logs() const {
        return stored_logs_;
    }
    
    /// Clear stored log messages
    void clear_logs() {
        stored_logs_.clear();
    }
    
    /// Set maximum number of stored logs (0 = unlimited)
    void set_max_stored_logs(size_t max) {
        max_stored_logs_ = max;
    }

    /// Public logging methods for framework/app use
    void log_trace(const std::string& text) {
        process_log({Level::Trace, text}, service_id_);
    }

    void log_debug(const std::string& text) {
        process_log({Level::Debug, text}, service_id_);
    }

    void log_info(const std::string& text) {
        process_log({Level::Info, text}, service_id_);
    }

    void log_warn(const std::string& text) {
        process_log({Level::Warn, text}, service_id_);
    }

    void log_error(const std::string& text) {
        process_log({Level::Error, text}, service_id_);
    }

    void log_fatal(const std::string& text) {
        process_log({Level::Fatal, text}, service_id_);
    }

protected:
    void handle(service::Message&& msg) override {
        // Handle log messages
        if (msg.header.type == static_cast<uint32_t>(LogMessageType::Log)) {
            auto log_msg = LogMessage::deserialize(msg.bytes());
            process_log(log_msg, msg.header.source);
        }
        // Handle log query requests
        else if (msg.header.type == static_cast<uint32_t>(LogMessageType::LogRequest)) {
            auto request = LogRequest::deserialize(msg.bytes());
            handle_log_request(request);
        }
    }
    
    [[nodiscard]] std::vector<service::Message> generate_outbound() override {
        std::vector<service::Message> messages;
        
        // Send pending log responses
        for (auto& response : pending_responses_) {
            auto data = response.serialize();
            auto msg = messaging::MessageBuilder()
                .type(static_cast<uint32_t>(LogMessageType::LogResponse))
                .from(service_id_)
                .priority(messaging::Priority::Normal)
                .payload(std::as_bytes(std::span(data)))
                .build();
            messages.push_back(std::move(msg));
        }
        pending_responses_.clear();
        
        return messages;
    }

private:
    void handle_log_request(const LogRequest& request) {
        LogResponse response{
            .requester_id = request.requester_id,
            .request_id = request.request_id,
            .logs = {},
            .total_matches = 0
        };
        
        // Define individual filter predicates
        auto matches_service = [&](const auto& entry) {
            return !request.service_id || entry.source_id == *request.service_id;
        };
        
        auto matches_level = [&](const auto& entry) {
            return !request.min_level || entry.log.level >= *request.min_level;
        };
        
        auto matches_time_range = [&](const auto& entry) {
            if (request.start_time && entry.log.timestamp < *request.start_time) return false;
            if (request.end_time && entry.log.timestamp > *request.end_time) return false;
            return true;
        };
        
        auto matches_text = [&](const auto& entry) {
            return !request.text_filter || entry.log.text.find(*request.text_filter) != std::string::npos;
        };
        
        // Apply all filters using ranges
        auto filtered = stored_logs_ 
            | std::views::filter(matches_service)
            | std::views::filter(matches_level)
            | std::views::filter(matches_time_range)
            | std::views::filter(matches_text);
        
        // Count total matches
        response.total_matches = static_cast<size_t>(std::ranges::distance(filtered));
        
        // Apply offset and limit
        auto paginated = filtered 
            | std::views::drop(request.offset)
            | std::views::take(request.limit);
        
        // Extract LogMessages from StoredLog
        for (const auto& stored : paginated) {
            response.logs.push_back(stored.log);
        }
        
        pending_responses_.push_back(std::move(response));
    }
    
    void process_log(const LogMessage& log_msg, messaging::ServiceId source) {
        // Filter by minimum level
        if (log_msg.level < min_level_) {
            return;
        }
        
        // Store the log message with source
        if (max_stored_logs_ == 0 || stored_logs_.size() < max_stored_logs_) {
            stored_logs_.push_back({log_msg, source});
        } else if (max_stored_logs_ > 0) {
            // Rotate logs if at capacity
            stored_logs_.erase(stored_logs_.begin());
            stored_logs_.push_back({log_msg, source});
        }
        
        // Output to console if enabled
        if (console_output_) {
            print_log(log_msg, source);
        }
    }
    
    void print_log(const LogMessage& log_msg, messaging::ServiceId source) {
        // Format: [TIMESTAMP] [LEVEL] [SERVICE_ID] Message
        auto time_t = std::chrono::system_clock::to_time_t(log_msg.timestamp);
        std::tm tm_buf{};
        localtime_r(&time_t, &tm_buf);
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            log_msg.timestamp.time_since_epoch()) % 1000;
        
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ["
                  << std::setw(5) << to_string(log_msg.level)
                  << "] ["
                  << source
                  << "] "
                  << log_msg.text
                  << std::endl;
    }
    
    Level min_level_{Level::Info};
    bool console_output_{true};
    size_t max_stored_logs_{1000};
    std::vector<StoredLog> stored_logs_;
    std::vector<LogResponse> pending_responses_;
};

} // namespace protoflow::logging
