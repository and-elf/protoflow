#include <gtest/gtest.h>
#include "services/app_registration_service.hpp"
#include <protoflow/app_registration_protocol/messages.hpp>
#include <messages.hpp>

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

// Test 1: Registration via message should create app and respond
TEST_F(AppRegistrationServiceTest, HandleRegisterAppMessage) {
    // Arrange: Create registration message
    app_registration_protocol::register_app_msg reg_msg{};
    std::strncpy(reg_msg.name, "test-app", sizeof(reg_msg.name) - 1);
    reg_msg.endpoint_count = 2;
    
    std::vector<std::byte> payload;
    payload.resize(sizeof(reg_msg));
    std::memcpy(payload.data(), &reg_msg, sizeof(reg_msg));
    
    // Add endpoints
    std::string ep1 = "tcp://localhost:5000";
    std::string ep2 = "unix:///tmp/test.sock";
    size_t old_size = payload.size();
    payload.resize(old_size + ep1.size() + 1);
    std::memcpy(payload.data() + old_size, ep1.c_str(), ep1.size() + 1);
    old_size = payload.size();
    payload.resize(old_size + ep2.size() + 1);
    std::memcpy(payload.data() + old_size, ep2.c_str(), ep2.size() + 1);
    
    messaging::Message msg{
        .type = app_registration_protocol::MessageTypes::RegisterApp,
        .payload = std::move(payload)
    };
    
    // Act: Handle the message
    service_->handle(std::move(msg));
    
    // Assert: App should be registered
    auto app = service_->get_app("test-app");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app->name, "test-app");
    EXPECT_EQ(app->endpoints.size(), 2);
    EXPECT_EQ(app->fsm->state(), AppState::registered);
    
    // Assert: Should generate response message
    auto outbound = service_->generate_outbound();
    ASSERT_EQ(outbound.size(), 1);
    EXPECT_EQ(outbound[0].type, app_registration_protocol::MessageTypes::RegisterAppAck);
}

// Test 2: Heartbeat message should update keepalive and transition to alive
TEST_F(AppRegistrationServiceTest, HandleHeartbeatMessage) {
    // Arrange: First register an app
    service_->register_app(AppRegistration{
        .name = "test-app",
        .version = "1.0",
        .endpoints = {"tcp://localhost:5000"}
    });
    EXPECT_EQ(service_->get_app("test-app")->fsm->state(), AppState::registered);
    
    // Create heartbeat message
    app_registration_protocol::keepalive_msg ka_msg{};
    std::strncpy(ka_msg.name, "test-app", sizeof(ka_msg.name) - 1);
    
    std::vector<std::byte> payload(sizeof(ka_msg));
    std::memcpy(payload.data(), &ka_msg, sizeof(ka_msg));
    
    messaging::Message msg{
        .type = app_registration_protocol::MessageTypes::Keepalive,
        .payload = std::move(payload)
    };
    
    // Act: Handle heartbeat
    service_->handle(std::move(msg));
    
    // Assert: App should transition to alive
    auto app = service_->get_app("test-app");
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(app->fsm->state(), AppState::alive);
    
    // Assert: Should generate ack
    auto outbound = service_->generate_outbound();
    ASSERT_EQ(outbound.size(), 1);
    EXPECT_EQ(outbound[0].type, app_registration_protocol::MessageTypes::KeepaliveAck);
}

// Test 3: Multiple heartbeats should keep app alive
TEST_F(AppRegistrationServiceTest, MultipleHeartbeatsKeepAppAlive) {
    // Arrange
    service_->register_app(AppRegistration{
        .name = "test-app",
        .version = "1.0",
        .endpoints = {"tcp://localhost:5000"}
    });
    
    // Act: Send first heartbeat
    service_->update_keepalive("test-app");
    EXPECT_EQ(service_->get_app("test-app")->fsm->state(), AppState::alive);
    
    // Send second heartbeat via message
    app_registration_protocol::keepalive_msg ka_msg{};
    std::strncpy(ka_msg.name, "test-app", sizeof(ka_msg.name) - 1);
    std::vector<std::byte> payload(sizeof(ka_msg));
    std::memcpy(payload.data(), &ka_msg, sizeof(ka_msg));
    
    messaging::Message msg{
        .type = app_registration_protocol::MessageTypes::Keepalive,
        .payload = std::move(payload)
    };
    service_->handle(std::move(msg));
    
    // Assert: Should still be alive
    EXPECT_EQ(service_->get_app("test-app")->fsm->state(), AppState::alive);
}

// Test 4: Unregister message should remove app
TEST_F(AppRegistrationServiceTest, HandleUnregisterMessage) {
    // Arrange: Register app first
    AppRegistration reg{
    service_->register_app(AppRegistration{
        .name = "test-app",
        .version = "1.0",
        .endpoints = {"tcp://localhost:5000"}
    }p("test-app"), nullptr);
    
    // Create unregister message
    app_registration_protocol::unregister_app_msg unreg_msg{};
    std::strncpy(unreg_msg.name, "test-app", sizeof(unreg_msg.name) - 1);
    
    std::vector<std::byte> payload(sizeof(unreg_msg));
    std::memcpy(payload.data(), &unreg_msg, sizeof(unreg_msg));
    
    messaging::Message msg{
        .type = app_registration_protocol::MessageTypes::UnregisterApp,
        .payload = std::move(payload)
    };
    
    // Act: Handle unregister
    service_->handle(std::move(msg));
    
    // Assert: App should be removed
    EXPECT_EQ(service_->get_app("test-app"), nullptr);
    
    // Assert: Should generate ack
    auto outbound = service_->generate_outbound();
    ASSERT_EQ(outbound.size(), 1);
    EXPECT_EQ(outbound[0].type, app_registration_protocol::MessageTypes::UnregisterAppAck);
}

// Test 5: State query message should return JSON state
TEST_F(AppRegistrationServiceTest, HandleStateQueryMessage) {
    // Arrange: Register a couple of apps
    AppRegistration reg1{
    service_->register_app(AppRegistration{
        .name = "app1",
        .version = "1.0",
        .endpoints = {"tcp://localhost:5000"}
    });
    service_->register_app(AppRegistration{
        .name = "app2",
        .version = "2.0",
        .endpoints = {"unix:///tmp/app2.sock"}
    }"app1"); // Make app1 alive
    
    // Create state query message
    messaging::Message msg{
        .type = MessageTypes::StateRequest,
        .payload = {}
    };
    
    // Act: Handle state query
    service_->handle(std::move(msg));
    
    // Assert: Should generate state response
    auto outbound = service_->generate_outbound();
    ASSERT_EQ(outbound.size(), 1);
    EXPECT_EQ(outbound[0].type, MessageTypes::StateResponse);
    
    // Payload should contain JSON with both apps
    std::string json_str(
        reinterpret_cast<const char*>(outbound[0].payload.data()),
        outbound[0].payload.size()
    );
    EXPECT_TRUE(json_str.find("app1") != std::string::npos);
    EXPECT_TRUE(json_str.find("app2") != std::string::npos);
    EXPECT_TRUE(json_str.find("alive") != std::string::npos);
    EXPECT_TRUE(json_str.find("registered") != std::string::npos);
}

// Test 6: Unknown message type should be ignored
TEST_F(AppRegistrationServiceTest, UnknownMessageTypeIgnored) {
    // Arrange
    messaging::Message msg{
        .type = 9999, // Unknown type
        .payload = {std::byte{1}, std::byte{2}, std::byte{3}}
    };
    
    // Act
    service_->handle(std::move(msg));
    
    // Assert: No crash, no outbound messages
    auto outbound = service_->generate_outbound();
    EXPECT_EQ(outbound.size(), 0);
}

// Test 7: Malformed registration message should be rejected
TEST_F(AppRegistrationServiceTest, MalformedRegistrationMessageRejected) {
    // Arrange: Create message with insufficient payload
    messaging::Message msg{
        .type = app_registration_protocol::MessageTypes::RegisterApp,
        .payload = {std::byte{1}, std::byte{2}} // Too small
    };
    
    // Act
    service_->handle(std::move(msg));
    
    // Assert: App should not be registered
    auto apps = service_->get_registered_apps();
    EXPECT_EQ(apps.size(), 0);
}
