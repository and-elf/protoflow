#pragma once

#include <protoflow/service/service.hpp>
#include <protoflow/logging/log_message.hpp>
#include <vector>
#include <functional>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace protoflow::logging {

/// Callback type for log message handling
using LogCallback = std::function<void(const LogMessage&, messaging::ServiceId source)>;

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
    
    /// Add a callback to be invoked for each log message
    void add_callback(LogCallback callback) {
        callbacks_.push_back(std::move(callback));
    }
    
    /// Enable/disable console output (enabled by default)
    void set_console_output(bool enabled) {
        console_output_ = enabled;
    }
    
    /// Get stored log messages
    [[nodiscard]] const std::vector<LogMessage>& get_logs() const {
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

protected:
    void handle(service::Message&& msg) override {
        // Check if this is a log message
        if (auto* log_msg = std::get_if<LogMessage>(&msg.payload)) {
            process_log(*log_msg, msg.header.source);
        }
    }

private:
    void process_log(const LogMessage& log_msg, messaging::ServiceId source) {
        // Filter by minimum level
        if (log_msg.level < min_level_) {
            return;
        }
        
        // Store the log message
        if (max_stored_logs_ == 0 || stored_logs_.size() < max_stored_logs_) {
            stored_logs_.push_back(log_msg);
        } else if (max_stored_logs_ > 0) {
            // Rotate logs if at capacity
            stored_logs_.erase(stored_logs_.begin());
            stored_logs_.push_back(log_msg);
        }
        
        // Output to console if enabled
        if (console_output_) {
            print_log(log_msg, source);
        }
        
        // Invoke callbacks
        for (const auto& callback : callbacks_) {
            callback(log_msg, source);
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
    std::vector<LogMessage> stored_logs_;
    std::vector<LogCallback> callbacks_;
};

} // namespace protoflow::logging
