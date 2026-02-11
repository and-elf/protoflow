#include <gtest/gtest.h>
#include <protoflow/logging/logging.hpp>
#include <protoflow/service/service.hpp>

using namespace protoflow;

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

TEST(LoggingTest, LoggingServiceFiltering) {
    logging::LoggingService logger;
    logger.set_min_level(logging::Level::Warn);
    logger.set_console_output(false); // Don't spam console during test
    logger.set_max_stored_logs(10);
    
    // Send messages at different levels using the current bytes-based API
    auto send_log = [&](logging::Level level, const std::string& text) {
        auto log = logging::LogMessage(level, text);
        auto data = log.serialize();
        auto msg = messaging::MessageBuilder{}
            .from(1)
            .type(static_cast<uint32_t>(logging::LogMessageType::Log))
            .priority(messaging::Priority::Normal)
            .payload(std::as_bytes(std::span(data)))
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
    
    // Should have 2 stored logs (Warn and Error) with source info
    const auto& logs = logger.get_logs();
    EXPECT_EQ(2, logs.size());
    
    if (logs.size() >= 2) {
        EXPECT_EQ(logging::Level::Warn, logs[0].log.level);
        EXPECT_EQ("Warning - should pass", logs[0].log.text);
        EXPECT_EQ(1, logs[0].source_id);
        EXPECT_EQ(logging::Level::Error, logs[1].log.level);
        EXPECT_EQ("Error - should pass", logs[1].log.text);
        EXPECT_EQ(1, logs[1].source_id);
    }
}

TEST(LoggingTest, LoggingServiceMaxStorage) {
    logging::LoggingService logger;
    logger.set_min_level(logging::Level::Trace);
    logger.set_console_output(false);
    logger.set_max_stored_logs(3);
    
    // Send 5 messages
    for (int i = 0; i < 5; ++i) {
        auto log = logging::LogMessage(logging::Level::Info, "Message " + std::to_string(i));
        auto data = log.serialize();
        auto msg = messaging::MessageBuilder{}
            .from(1)
            .type(static_cast<uint32_t>(logging::LogMessageType::Log))
            .priority(messaging::Priority::Normal)
            .payload(std::as_bytes(std::span(data)))
            .build();
        logger.on_message(std::move(msg));
        logger.poll();
    }
    
    // Should only have 3 stored (most recent)
    const auto& logs = logger.get_logs();
    EXPECT_EQ(3, logs.size());
    
    if (logs.size() == 3) {
        EXPECT_EQ("Message 2", logs[0].log.text);
        EXPECT_EQ("Message 3", logs[1].log.text);
        EXPECT_EQ("Message 4", logs[2].log.text);
    }
}

TEST(LoggingTest, LoggingServiceClear) {
    logging::LoggingService logger;
    logger.set_console_output(false);
    
    // Add some logs
    for (int i = 0; i < 3; ++i) {
        auto log = logging::LogMessage(logging::Level::Info, "Test");
        auto data = log.serialize();
        auto msg = messaging::MessageBuilder{}
            .type(static_cast<uint32_t>(logging::LogMessageType::Log))
            .priority(messaging::Priority::Normal)
            .payload(std::as_bytes(std::span(data)))
            .build();
        logger.on_message(std::move(msg));
        logger.poll();
    }
    
    EXPECT_EQ(3, logger.get_logs().size());
    
    logger.clear_logs();
    EXPECT_EQ(0, logger.get_logs().size());
}
