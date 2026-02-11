#include <gtest/gtest.h>
#include "services/app_registration_service.hpp"
#include "messages.hpp"

using namespace protoflow;
using namespace protoflow::mainapp;

class AppRegistrationServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<AppRegistrationService>();
        service_->start();
    }

    void TearDown() override {
        service_->stop();
    }

    std::unique_ptr<AppRegistrationService> service_;
};

// Test 1: Registration via message should create app
TEST_F(AppRegistrationServiceTest, HandleRegisterAppMessage) {
    // Arrange: Create registration event
    AppRegistrationEvent event;
    event.app_name = "test-app";
    event.version = "1.0";
    event.endpoints = {"tcp://localhost:5000", "unix:///tmp/test.sock"};
    
    auto payload = event.serialize();
    
    messaging::MessageHeader header;
    header.type = MessageTypes::AppRegistrationEvent;
    
    messaging::Message msg{header, std::move(payload)};
    
    // Act: Send message and process via poll
    service_->on_message(std::move(msg));
    service_->poll(); // Process the message
    
    // Assert: App should be registered (use const reference to call public const get_app)
    const AppRegistrationService& const_service = *service_;
    auto app = const_service.get_app("test-app");
    ASSERT_TRUE(app.has_value());
    EXPECT_EQ(app->get().name, "test-app");
    EXPECT_EQ(app->get().endpoints.size(), 2);
    EXPECT_EQ(app->get().fsm->state(), AppState::registered);
}

// Test 2: Unregister message should remove app
TEST_F(AppRegistrationServiceTest, HandleUnregisterMessage) {
    // Arrange: Register app first
    AppRegistrationEvent reg_event;
    reg_event.app_name = "test-app";
    reg_event.version = "1.0";
    reg_event.endpoints = {"tcp://localhost:5000"};
    
    auto reg_payload = reg_event.serialize();
    messaging::MessageHeader reg_header;
    reg_header.type = MessageTypes::AppRegistrationEvent;
    service_->on_message(messaging::Message{reg_header, std::move(reg_payload)});
    service_->poll();
    
    const AppRegistrationService& const_service = *service_;
    ASSERT_TRUE(const_service.get_app("test-app").has_value());
    
    // Create unregister message
    AppUnregistrationEvent unreg_event;
    unreg_event.app_name = "test-app";
    
    auto unreg_payload = unreg_event.serialize();
    messaging::MessageHeader unreg_header;
    unreg_header.type = MessageTypes::AppUnregistrationEvent;
    
    messaging::Message msg{unreg_header, std::move(unreg_payload)};
    
    // Act: Send unregister message and process
    service_->on_message(std::move(msg));
    service_->poll();
    
    // Assert: App should be removed
    EXPECT_FALSE(const_service.get_app("test-app").has_value());
}

// Test 3: Unknown message type should be ignored
TEST_F(AppRegistrationServiceTest, UnknownMessageTypeIgnored) {
    // Arrange
    messaging::MessageHeader header;
    header.type = 9999; // Unknown type
    std::vector<std::byte> payload{std::byte{1}, std::byte{2}, std::byte{3}};
    messaging::Message msg{header, std::move(payload)};
    
    // Act: Send message and process
    service_->on_message(std::move(msg));
    service_->poll();
    
    // Assert: No crash, handled gracefully
    SUCCEED();
}

// Test 4: Malformed registration message should be rejected
TEST_F(AppRegistrationServiceTest, MalformedRegistrationMessageRejected) {
    // Arrange: Create message with insufficient payload
    messaging::MessageHeader header;
    header.type = MessageTypes::AppRegistrationEvent;
    std::vector<std::byte> payload{std::byte{1}, std::byte{2}}; // Too small
    messaging::Message msg{header, std::move(payload)};
    
    // Act: Send malformed message and process
    service_->on_message(std::move(msg));
    service_->poll();
    
    // Assert: App should not be registered (message parsing should fail)
    const AppRegistrationService& const_service = *service_;
    EXPECT_FALSE(const_service.get_app("test-app").has_value());
}

// Test 5: Multiple registrations and unregistrations
TEST_F(AppRegistrationServiceTest, MultipleApps) {
    // Register app1
    AppRegistrationEvent event1;
    event1.app_name = "app1";
    event1.version = "1.0";
    event1.endpoints = {"tcp://localhost:5000"};
    
    messaging::MessageHeader header1;
    header1.type = MessageTypes::AppRegistrationEvent;
    service_->on_message(messaging::Message{header1, event1.serialize()});
    service_->poll();
    
    // Register app2
    AppRegistrationEvent event2;
    event2.app_name = "app2";
    event2.version = "2.0";
    event2.endpoints = {"unix:///tmp/app2.sock"};
    
    messaging::MessageHeader header2;
    header2.type = MessageTypes::AppRegistrationEvent;
    service_->on_message(messaging::Message{header2, event2.serialize()});
    service_->poll();
    
    // Assert: Both apps registered
    const AppRegistrationService& const_service = *service_;
    EXPECT_TRUE(const_service.get_app("app1").has_value());
    EXPECT_TRUE(const_service.get_app("app2").has_value());
    
    // Unregister app1
    AppUnregistrationEvent unreg_event;
    unreg_event.app_name = "app1";
    
    messaging::MessageHeader unreg_header;
    unreg_header.type = MessageTypes::AppUnregistrationEvent;
    service_->on_message(messaging::Message{unreg_header, unreg_event.serialize()});
    service_->poll();
    
    // Assert: app1 removed, app2 still there
    EXPECT_FALSE(const_service.get_app("app1").has_value());
    EXPECT_TRUE(const_service.get_app("app2").has_value());
}
