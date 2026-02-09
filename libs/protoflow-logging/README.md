# Protoflow Logging Library

The protoflow-logging library provides integrated logging support for protoflow services through the messaging system.

## Overview

Logging in protoflow is message-based. Services can emit log messages just like any other message, and a `LoggingService` can collect, filter, and process these logs.

## Features

- **Message-based logging**: Log messages are sent through the standard messaging infrastructure
- **Multiple severity levels**: Trace, Debug, Info, Warn, Error, Fatal
- **Automatic timestamps**: Each log message includes a high-resolution timestamp
- **Stream-style macros**: Use C++ stream operators for convenient formatting
- **Filtering**: Configure minimum log level
- **Storage**: Store logs in memory with configurable limits
- **Callbacks**: Register custom handlers for log processing
- **Console output**: Built-in formatted console output

## Message Types

The `Message` struct now uses a variant payload that can hold:

```cpp
std::variant<
    std::monostate,      // Empty
    PayloadMessage,      // Raw binary data
    EventMessage,        // String-based events
    LogMessage           // Log messages
>
```

### LogMessage Structure

```cpp
struct LogMessage {
    Level level;                                  // Severity level
    std::string text;                             // Log text
    std::chrono::system_clock::time_point timestamp;  // When logged
};
```

## Usage

### Basic Logging in Services

Services can log using built-in convenience methods:

```cpp
class MyService : public protoflow::service::Service {
protected:
    void handle(Message&& msg) override {
        log_info("Processing message");
        log_debug("Message ID: " + std::to_string(msg.header.id));
        
        if (error_condition) {
            log_error("Failed to process message");
        }
    }
};
```

### Stream-Style Logging with Macros

Use macros for formatted logging with stream operators:

```cpp
#include <protoflow/logging/macros.hpp>

class MyService : public protoflow::service::Service {
protected:
    void handle(Message&& msg) override {
        int counter = 42;
        PROTOFLOW_LOG_INFO(*this, "Counter value: " << counter);
        PROTOFLOW_LOG_DEBUG(*this, "Processing message " << msg.header.id);
    }
};
```

Available macros:
- `PROTOFLOW_LOG_TRACE(service, msg)`
- `PROTOFLOW_LOG_DEBUG(service, msg)`
- `PROTOFLOW_LOG_INFO(service, msg)`
- `PROTOFLOW_LOG_WARN(service, msg)`
- `PROTOFLOW_LOG_ERROR(service, msg)`
- `PROTOFLOW_LOG_FATAL(service, msg)`

### Setting Up the LoggingService

```cpp
#include <protoflow/logging/logging_service.hpp>

auto logging_service = std::make_shared<protoflow::logging::LoggingService>();

// Configure logging
logging_service->set_min_level(protoflow::logging::Level::Debug);
logging_service->set_console_output(true);
logging_service->set_max_stored_logs(5000);

// Add custom callback
logging_service->add_callback([](const auto& log, auto source) {
    // Custom log processing
    if (log.level >= protoflow::logging::Level::Error) {
        // Handle errors specially
    }
});

// Register with runtime
runtime.register_service(LOGGING_SERVICE_ID, logging_service);

// Configure other services to use this logging service
my_service->set_logging_service_id(LOGGING_SERVICE_ID);
```

### Retrieving Stored Logs

```cpp
const auto& logs = logging_service->get_logs();
for (const auto& log : logs) {
    std::cout << protoflow::logging::to_string(log.level) 
              << ": " << log.text << std::endl;
}

// Clear logs when done
logging_service->clear_logs();
```

## Log Levels

- **Trace**: Very detailed diagnostic information
- **Debug**: Debugging information useful during development
- **Info**: Informational messages about normal operation
- **Warn**: Warning messages about potential issues
- **Error**: Error messages about failures
- **Fatal**: Critical errors that may cause termination

## Console Output Format

```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [SERVICE_ID] Message text
```

Example:
```
[2026-02-09 14:30:45.123] [ INFO] [1] Service started successfully
[2026-02-09 14:30:45.456] [DEBUG] [1] Processing 10 items
[2026-02-09 14:30:46.789] [ERROR] [2] Connection failed
```

## Dependencies

- `protoflow-messaging`: For message infrastructure
- `protoflow-service`: For service base class

## CMake Integration

```cmake
target_link_libraries(my_service
    PRIVATE
        protoflow::service
        protoflow::logging
)
```

## Architecture Notes

The logging system is fully integrated with the protoflow service architecture:

1. Services emit log messages through the standard `write()` mechanism
2. LoggingService receives and processes logs like any other message
3. All logging is asynchronous and non-blocking
4. No global state or singleton loggers
5. Deterministic message ordering (logs are processed in order)

This design ensures logging doesn't interfere with the deterministic execution model of protoflow services.
