// Example usage of protoflow logging
#include <protoflow/logging/logging.hpp>
#include <protoflow/service/service.hpp>
#include <memory>

using namespace protoflow;

// Example service that uses logging
class ExampleService : public service::Service {
protected:
    void handle(service::Message&& msg) override {
        // Direct logging methods
        log_info("Handling message");
        
        // Stream-style logging with macros
        PROTOFLOW_LOG_DEBUG(*this, "Message ID: " << msg.header.id);
        PROTOFLOW_LOG_INFO(*this, "From service: " << msg.header.source);
        
        // Check message type
        if (msg.is_log()) {
            log_warn("Received a log message (unexpected)");
        } else if (msg.is_event()) {
            auto* event = std::get_if<messaging::EventMessage>(&msg.payload);
            if (event) {
                PROTOFLOW_LOG_INFO(*this, "Event: " << event->event_type);
            }
        }
    }
};

int main() {
    // Create logging service
    auto logger = std::make_shared<logging::LoggingService>();
    logger->set_min_level(logging::Level::Debug);
    logger->set_console_output(true);
    logger->set_max_stored_logs(100);
    
    // Create example service
    auto example = std::make_shared<ExampleService>();
    example->set_service_id(1);
    example->set_logging_service_id(0);
    
    // Log some messages
    example->log_info("Service initialized");
    example->log_debug("Debug information");
    
    PROTOFLOW_LOG_INFO(*example, "Counter: " << 42);
    
    return 0;
}
