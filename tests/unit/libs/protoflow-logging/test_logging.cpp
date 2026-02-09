#include <gtest/gtest.h>
#include <protoflow/logging/logging.hpp>
#include <protoflow/service/service.hpp>

using namespace protoflow;

// Test service for logging tests
class TestService : public service::Service {
public:
    void handle(service::Message&& /* msg */) override {
        // Not used in these tests
    }
    
    // Expose protected logging methods for testing
    using Service::log_trace;
    using Service::log_debug;
    using Service::log_info;
    using Service::log_warn;
    using Service::log_error;
    using Service::log_fatal;
    using Service::set_service_id;
    using Service::set_logging_service_id;
};

TEST(LoggingTest, LogLevelToString) {
    EXPECT_STREQ("TRACE", logging::to_string(logging::Level::Trace));
    EXPECT_STREQ("DEBUG", logging::to_string(logging::Level::Debug));
    EXPECT_STREQ("INFO", logging::to_string(logging::Level::Info));
    EXPECT_STREQ("WARN", logging::to_string(logging::Level::Warn));
    EXPECT_STREQ("ERROR", logging::to_string(logging::Level::Error));
    EXPECT_STREQ("FATAL", logging::to_string(logging::Level::Fatal));
}

TEST(LoggingTest, LogMessageCreation) {
    logging::LogMessage msg(logging::Level::Info, "Test message");
    
    EXPECT_EQ(logging::Level::Info, msg.level);
    EXPECT_EQ("Test message", msg.text);
    
    // Timestamp should be recent
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - msg.timestamp);
    EXPECT_LT(diff.count(), 100); // Should be created within last 100ms
}

TEST(LoggingTest, MessageVariantTypes) {
    using namespace messaging;
    
    // Test PayloadMessage
    {
        std::vector<std::byte> data{std::byte{0x01}, std::byte{0x02}};
        Message msg{MessageHeader{}, PayloadMessage{std::move(data)}};
        EXPECT_TRUE(msg.is_payload());
        EXPECT_FALSE(msg.is_event());
        EXPECT_FALSE(msg.is_log());
        EXPECT_EQ(2, msg.size());
    }
    
    // Test EventMessage
    {
        Message msg{MessageHeader{}, EventMessage{"test.event", "data"}};
        EXPECT_TRUE(msg.is_event());
        EXPECT_FALSE(msg.is_payload());
        EXPECT_FALSE(msg.is_log());
    }
    
    // Test LogMessage
    {
        logging::LogMessage log(logging::Level::Info, "Test");
        Message msg{MessageHeader{}, std::move(log)};
        EXPECT_TRUE(msg.is_log());
        EXPECT_FALSE(msg.is_payload());
        EXPECT_FALSE(msg.is_event());
    }
}

TEST(LoggingTest, MessageBuilderWithLog) {
    using namespace messaging;
    
    auto log = logging::LogMessage(logging::Level::Error, "Error occurred");
    auto msg = MessageBuilder{}
        .id(123)
        .from(1)
        .to(2)
        .log(std::move(log))
        .build();
    
    EXPECT_EQ(123, msg.header.id);
    EXPECT_EQ(1, msg.header.source);
    EXPECT_EQ(2, msg.header.destination);
    EXPECT_TRUE(msg.is_log());
    
    auto* log_ptr = std::get_if<logging::LogMessage>(&msg.payload);
    ASSERT_NE(nullptr, log_ptr);
    EXPECT_EQ(logging::Level::Error, log_ptr->level);
    EXPECT_EQ("Error occurred", log_ptr->text);
}

TEST(LoggingTest, ServiceLoggingMethods) {
    TestService service;
    service.set_service_id(42);
    service.set_logging_service_id(100);
    
    // These should create log messages without crashing
    service.log_trace("Trace");
    service.log_debug("Debug");
    service.log_info("Info");
    service.log_warn("Warn");
    service.log_error("Error");
    service.log_fatal("Fatal");
    
    // Should have 6 outbound messages
    int count = 0;
    while (auto msg = service.pop_outbound()) {
        EXPECT_TRUE(msg->is_log());
        EXPECT_EQ(42, msg->header.source);
        EXPECT_EQ(100, msg->header.destination);
        count++;
    }
    EXPECT_EQ(6, count);
}

TEST(LoggingTest, LoggingServiceFiltering) {
    logging::LoggingService logger;
    logger.set_min_level(logging::Level::Warn);
    logger.set_console_output(false); // Don't spam console during test
    logger.set_max_stored_logs(10);
    
    // Send messages at different levels
    auto send_log = [&](logging::Level level, const std::string& text) {
        auto msg = messaging::MessageBuilder{}
            .from(1)
            .log(logging::LogMessage(level, text))
            .build();
        logger.on_message(std::move(msg));
    };
    
    send_log(logging::Level::Debug, "Debug - should be filtered");
    send_log(logging::Level::Info, "Info - should be filtered");
    send_log(logging::Level::Warn, "Warning - should pass");
    send_log(logging::Level::Error, "Error - should pass");
    
    // Process messages
    for (int i = 0; i < 4; ++i) {
        logger.poll();
    }
    
    // Should have 2 stored logs (Warn and Error)
    const auto& logs = logger.get_logs();
    EXPECT_EQ(2, logs.size());
    
    if (logs.size() >= 2) {
        EXPECT_EQ(logging::Level::Warn, logs[0].level);
        EXPECT_EQ("Warning - should pass", logs[0].text);
        EXPECT_EQ(logging::Level::Error, logs[1].level);
        EXPECT_EQ("Error - should pass", logs[1].text);
    }
}

TEST(LoggingTest, LoggingServiceMaxStorage) {
    logging::LoggingService logger;
    logger.set_min_level(logging::Level::Trace);
    logger.set_console_output(false);
    logger.set_max_stored_logs(3);
    
    // Send 5 messages
    for (int i = 0; i < 5; ++i) {
        auto msg = messaging::MessageBuilder{}
            .from(1)
            .log(logging::LogMessage(logging::Level::Info, "Message " + std::to_string(i)))
            .build();
        logger.on_message(std::move(msg));
        logger.poll();
    }
    
    // Should only have 3 stored (most recent)
    const auto& logs = logger.get_logs();
    EXPECT_EQ(3, logs.size());
    
    if (logs.size() == 3) {
        EXPECT_EQ("Message 2", logs[0].text);
        EXPECT_EQ("Message 3", logs[1].text);
        EXPECT_EQ("Message 4", logs[2].text);
    }
}

TEST(LoggingTest, LoggingServiceCallback) {
    logging::LoggingService logger;
    logger.set_console_output(false);
    
    int callback_count = 0;
    logging::Level last_level = logging::Level::Trace;
    
    logger.add_callback([&](const logging::LogMessage& log, messaging::ServiceId /* source */) {
        callback_count++;
        last_level = log.level;
    });
    
    auto msg = messaging::MessageBuilder{}
        .from(42)
        .log(logging::LogMessage(logging::Level::Error, "Test"))
        .build();
    logger.on_message(std::move(msg));
    logger.poll();
    
    EXPECT_EQ(1, callback_count);
    EXPECT_EQ(logging::Level::Error, last_level);
}

TEST(LoggingTest, LoggingServiceClear) {
    logging::LoggingService logger;
    logger.set_console_output(false);
    
    // Add some logs
    for (int i = 0; i < 3; ++i) {
        auto msg = messaging::MessageBuilder{}
            .log(logging::LogMessage(logging::Level::Info, "Test"))
            .build();
        logger.on_message(std::move(msg));
        logger.poll();
    }
    
    EXPECT_EQ(3, logger.get_logs().size());
    
    logger.clear_logs();
    EXPECT_EQ(0, logger.get_logs().size());
}
