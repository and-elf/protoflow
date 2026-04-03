#include <gtest/gtest.h>
#include "../../apps/main-app/include/services/http_service.hpp"
#include "../../apps/main-app/include/protoflow/messages.hpp"
#include <protoflow/logging/logging_service.hpp>
#include <thread>
#include <chrono>

using namespace protoflow::mainapp;
using namespace protoflow::logging;
using namespace std::chrono_literals;

class HTTPServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_service = std::make_unique<HTTPService>();
        logging_service = std::make_unique<LoggingService>();
        
        http_service->start();
        logging_service->start();
    }
    
    void TearDown() override {
        http_service->stop();
        logging_service->stop();
    }
    
    std::unique_ptr<HTTPService> http_service;
    std::unique_ptr<LoggingService> logging_service;
};

TEST_F(HTTPServiceTest, RequestTimeoutGenerates504Response) {
    // Manually insert a pending request (simulating what would happen when HTTP request arrives)
    HTTPService::PendingRequest pending{
        .request_id = 123,
        .request = HttpRequest{.method = "GET", .path = "/api/logs"},
        .type = HTTPService::PendingRequest::Type::Logs,
        .timestamp = std::chrono::steady_clock::now() - 10s  // 10 seconds ago (will timeout)
    };
    
    // Insert into pending_requests_
    http_service->pending_requests_[123] = pending;
    
    // Verify it's in the map
    ASSERT_EQ(http_service->pending_requests_.size(), 1);
    ASSERT_TRUE(http_service->pending_responses_.empty());
    
    // Poll - should detect timeout and generate 504 response
    http_service->poll();
    
    // Verify pending request was removed
    EXPECT_TRUE(http_service->pending_requests_.empty());
    
    // Verify 504 response was generated
    ASSERT_EQ(http_service->pending_responses_.size(), 1);
    EXPECT_EQ(http_service->pending_responses_[0].status_code, 504);
}

TEST_F(HTTPServiceTest, SuccessfulLogRequestResponseFlow) {
    // Insert a pending request
    HTTPService::PendingRequest pending{
        .request_id = 456,
        .request = HttpRequest{.method = "GET", .path = "/api/logs"},
        .type = HTTPService::PendingRequest::Type::Logs,
        .timestamp = std::chrono::steady_clock::now()
    };
    http_service->pending_requests_[456] = pending;
    
    // Create a LogRequest
    LogRequest log_request{
        .requester_id = http_service->get_id(),
        .request_id = 456,
        .service_id = std::nullopt,
        .min_level = std::nullopt,
        .start_time = std::nullopt,
        .end_time = std::nullopt,
        .text_filter = std::nullopt,
        .limit = 10,
        .offset = 0
    };
    
    // Send request to logging service
    protoflow::service::MessageHeader req_header{
        .source = http_service->get_id(),
        .destination = logging_service->get_id(),
        .timestamp = std::chrono::system_clock::now()
    };
    
    protoflow::service::Message request_msg{
        .header = std::move(req_header),
        .payload = std::move(log_request)
    };
    
    // Logging service handles the request
    logging_service->handle(std::move(request_msg));
    
    // Get response from logging service
    auto responses = logging_service->generate_outbound();
    ASSERT_EQ(responses.size(), 1);
    
    // Verify it's a LogResponse
    auto* log_response = std::get_if<LogResponse>(&responses[0].payload);
    ASSERT_NE(log_response, nullptr);
    EXPECT_EQ(log_response->request_id, 456);
    EXPECT_EQ(log_response->requester_id, http_service->get_id());
    
    // HTTP service handles the response
    http_service->handle(std::move(responses[0]));
    
    // Verify pending request was removed
    EXPECT_TRUE(http_service->pending_requests_.empty());
    
    // Verify HTTP response was generated
    ASSERT_EQ(http_service->pending_responses_.size(), 1);
    EXPECT_EQ(http_service->pending_responses_[0].status_code, 200);
}

TEST_F(HTTPServiceTest, MultipleRequestsWithDifferentTimeouts) {
    // Test that we can handle multiple pending requests
    // and only timeout the ones that exceed the threshold
    
    auto now = std::chrono::steady_clock::now();
    
    // Request 1: Will timeout (timestamp is 10 seconds old)
    HTTPService::PendingRequest old_request{
        .request_id = 100,
        .request = HttpRequest{.method = "GET", .path = "/api/logs?old=true"},
        .type = HTTPService::PendingRequest::Type::Logs,
        .timestamp = now - 10s
    };
    http_service->pending_requests_[100] = old_request;
    
    // Request 2: Will not timeout (timestamp is 1 second old)
    HTTPService::PendingRequest recent_request{
        .request_id = 101,
        .request = HttpRequest{.method = "GET", .path = "/api/logs?recent=true"},
        .type = HTTPService::PendingRequest::Type::Logs,
        .timestamp = now - 1s
    };
    http_service->pending_requests_[101] = recent_request;
    
    ASSERT_EQ(http_service->pending_requests_.size(), 2);
    
    // Poll - should timeout request 100 but not 101
    http_service->poll();
    
    // Verify only request 101 remains
    EXPECT_EQ(http_service->pending_requests_.size(), 1);
    EXPECT_TRUE(http_service->pending_requests_.count(101));
    EXPECT_FALSE(http_service->pending_requests_.count(100));
    
    // Verify one 504 response was generated for request 100
    ASSERT_EQ(http_service->pending_responses_.size(), 1);
    EXPECT_EQ(http_service->pending_responses_[0].status_code, 504);
}

TEST_F(HTTPServiceTest, LogResponseMatchesRequest) {
    // Add some log messages to the logging service
    LogMessage msg1{
        .timestamp = std::chrono::system_clock::now(),
        .level = Level::Info,
        .source = 1,
        .message = "Test log 1"
    };
    
    LogMessage msg2{
        .timestamp = std::chrono::system_clock::now(),
        .level = Level::Warn,
        .source = 2,
        .message = "Test log 2"
    };
    
    // Send log messages to logging service
    protoflow::service::MessageHeader header1{
        .source = 1,
        .destination = logging_service->get_id(),
        .timestamp = std::chrono::system_clock::now()
    };
    
    logging_service->handle(protoflow::service::Message{
        .header = std::move(header1),
        .payload = std::move(msg1)
    });
    
    protoflow::service::MessageHeader header2{
        .source = 2,
        .destination = logging_service->get_id(),
        .timestamp = std::chrono::system_clock::now()
    };
    
    logging_service->handle(protoflow::service::Message{
        .header = std::move(header2),
        .payload = std::move(msg2)
    });
    
    // Now request logs with level filter
    LogRequest log_request{
        .requester_id = http_service->get_id(),
        .request_id = 789,
        .service_id = std::nullopt,
        .min_level = Level::Warn,  // Filter for Warn and above
        .start_time = std::nullopt,
        .end_time = std::nullopt,
        .text_filter = std::nullopt,
        .limit = 100,
        .offset = 0
    };
    
    protoflow::service::MessageHeader req_header{
        .source = http_service->get_id(),
        .destination = logging_service->get_id(),
        .timestamp = std::chrono::system_clock::now()
    };
    
    logging_service->handle(protoflow::service::Message{
        .header = std::move(req_header),
        .payload = std::move(log_request)
    });
    
    auto responses = logging_service->generate_outbound();
    ASSERT_EQ(responses.size(), 1);
    
    auto* log_response = std::get_if<LogResponse>(&responses[0].payload);
    ASSERT_NE(log_response, nullptr);
    
    // Should only return msg2 (Warn level), not msg1 (Info level)
    EXPECT_EQ(log_response->logs.size(), 1);
    EXPECT_EQ(log_response->total_matches, 1);
    EXPECT_EQ(log_response->logs[0].level, Level::Warn);
    EXPECT_EQ(log_response->logs[0].message, "Test log 2");
}
