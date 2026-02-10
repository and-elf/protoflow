#include <protoflow/app_registration_client/app_registration_client.hpp>
#include <protoflow/app_registration_protocol.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace protoflow::app_registration_client;
using namespace protoflow::app_registration_protocol;

// Test configuration helper
Config make_test_config(const std::string& app_name = "test_app") {
    return Config{
        .app_name = app_name,
        .version = 1,
        .endpoints = {"http://localhost:8080", "ws://localhost:8081"},
        .server_address = "127.0.0.1",
        .server_port = 9000,
        .heartbeat_interval = std::chrono::seconds(5),
        .connection_timeout = std::chrono::milliseconds(1000),
        .read_timeout = std::chrono::milliseconds(500),
        .write_timeout = std::chrono::milliseconds(500)
    };
}

class AppRegistrationClientBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = make_test_config();
    }

    Config config;
};

TEST_F(AppRegistrationClientBasicTest, ConstructorInitializesCorrectly) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.config().app_name, "test_app");
    EXPECT_EQ(client.config().version, 1u);
    EXPECT_EQ(client.config().server_address, "127.0.0.1");
    EXPECT_EQ(client.config().server_port, 9000);
    EXPECT_EQ(client.config().endpoints.size(), 2);
    EXPECT_FALSE(client.is_registered());
    EXPECT_FALSE(client.is_connected());
}

TEST_F(AppRegistrationClientBasicTest, InitialStateIsDisconnected) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientBasicTest, NameIncludesAppName) {
    AppRegistrationClient client(config);
    
    std::string name = client.name();
    EXPECT_NE(name.find("test_app"), std::string::npos);
}

TEST_F(AppRegistrationClientBasicTest, StartTriggersConnection) {
    AppRegistrationClient client(config);
    
    client.start();
    
    // Should transition to Connecting state
    EXPECT_EQ(client.current_state(), State::Connecting);
    EXPECT_FALSE(client.is_registered());
}

TEST_F(AppRegistrationClientBasicTest, StopFromDisconnectedState) {
    AppRegistrationClient client(config);
    
    client.stop();
    
    // Should stay in Disconnected state
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientBasicTest, IsConnectedReturnsTrueForActiveStates) {
    AppRegistrationClient client(config);
    
    // Disconnected
    EXPECT_FALSE(client.is_connected());
    
    // After start (Connecting)
    client.start();
    EXPECT_TRUE(client.is_connected());
}

TEST_F(AppRegistrationClientBasicTest, ConfigurationIsAccessible) {
    AppRegistrationClient client(config);
    
    const Config& retrieved = client.config();
    
    EXPECT_EQ(retrieved.app_name, config.app_name);
    EXPECT_EQ(retrieved.version, config.version);
    EXPECT_EQ(retrieved.server_address, config.server_address);
    EXPECT_EQ(retrieved.server_port, config.server_port);
    EXPECT_EQ(retrieved.endpoints, config.endpoints);
}

TEST_F(AppRegistrationClientBasicTest, MultipleEndpointsSupported) {
    config.endpoints = {"http://api1", "http://api2", "ws://ws1", "tcp://tcp1"};
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.config().endpoints.size(), 4);
    EXPECT_EQ(client.config().endpoints[0], "http://api1");
    EXPECT_EQ(client.config().endpoints[3], "tcp://tcp1");
}

TEST_F(AppRegistrationClientBasicTest, DifferentAppNames) {
    auto config1 = make_test_config("app1");
    auto config2 = make_test_config("app2");
    
    AppRegistrationClient client1(config1);
    AppRegistrationClient client2(config2);
    
    EXPECT_NE(client1.name(), client2.name());
    EXPECT_NE(client1.name().find("app1"), std::string::npos);
    EXPECT_NE(client2.name().find("app2"), std::string::npos);
}

TEST_F(AppRegistrationClientBasicTest, CustomServerConfiguration) {
    config.server_address = "192.168.1.100";
    config.server_port = 12345;
    
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.config().server_address, "192.168.1.100");
    EXPECT_EQ(client.config().server_port, 12345);
}

TEST_F(AppRegistrationClientBasicTest, TimingConfiguration) {
    config.heartbeat_interval = std::chrono::seconds(10);
    config.connection_timeout = std::chrono::milliseconds(2000);
    
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.config().heartbeat_interval, std::chrono::seconds(10));
    EXPECT_EQ(client.config().connection_timeout, std::chrono::milliseconds(2000));
}

TEST_F(AppRegistrationClientBasicTest, PollDoesNotCrashInDisconnectedState) {
    AppRegistrationClient client(config);
    
    EXPECT_NO_THROW(client.poll());
}

TEST_F(AppRegistrationClientBasicTest, PollDoesNotCrashInConnectingState) {
    AppRegistrationClient client(config);
    client.start();
    
    EXPECT_NO_THROW(client.poll());
}

// Note: generate_outbound() is protected, cannot test directly from unit tests
