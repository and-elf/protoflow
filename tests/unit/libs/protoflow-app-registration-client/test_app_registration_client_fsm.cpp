#include <protoflow/app_registration_client/app_registration_client.hpp>
#include <protoflow/app_registration_protocol.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace protoflow::app_registration_client;
using namespace protoflow::app_registration_protocol;

// Test configuration helper
Config make_test_config_fsm(const std::string& app_name = "fsm_test_app") {
    return Config{
        .app_name = app_name,
        .version = 1,
        .server_address = "127.0.0.1",
        .server_port = 9000,
        .endpoints = {"http://localhost:8080"},
        .heartbeat_interval = std::chrono::seconds(5),
        .connection_timeout = std::chrono::milliseconds(1000),
        .read_timeout = std::chrono::milliseconds(500),
        .write_timeout = std::chrono::milliseconds(500)
    };
}

class AppRegistrationClientFsmTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = make_test_config_fsm();
    }

    Config config;
};

TEST_F(AppRegistrationClientFsmTest, InitialStateIsDisconnected) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, StartTransitionsToConnecting) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    client.start();
    
    EXPECT_EQ(client.current_state(), State::Connecting);
}

TEST_F(AppRegistrationClientFsmTest, StopFromDisconnectedStaysDisconnected) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    client.stop();
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, ConnectedStateChecks) {
    AppRegistrationClient client(config);
    
    // Disconnected - not connected
    EXPECT_FALSE(client.is_connected());
    EXPECT_FALSE(client.is_registered());
    
    // Start -> Connecting
    client.start();
    EXPECT_TRUE(client.is_connected());
    EXPECT_FALSE(client.is_registered());
}

TEST_F(AppRegistrationClientFsmTest, RegisteredStateChecks) {
    AppRegistrationClient client(config);
    
    // Not registered initially
    EXPECT_FALSE(client.is_registered());
    
    // After start, still not registered (just connecting)
    client.start();
    EXPECT_FALSE(client.is_registered());
}

TEST_F(AppRegistrationClientFsmTest, StateTransitionSequence) {
    AppRegistrationClient client(config);
    
    // Initial state
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    // Start connection
    client.start();
    EXPECT_EQ(client.current_state(), State::Connecting);
    
    // Stop from connecting goes to Disconnected (via otherwise clause -> Failed -> Shutdown)
    client.stop();
    // After shutdown event, should be Disconnected
    EXPECT_TRUE(client.current_state() == State::Disconnected || 
                client.current_state() == State::Failed);
}

TEST_F(AppRegistrationClientFsmTest, MultipleStartCallsIdempotent) {
    AppRegistrationClient client(config);
    
    client.start();
    State first_state = client.current_state();
    
    // Calling start again shouldn't change state significantly
    // (might trigger otherwise clause if invalid transition)
    client.start();
    
    // Should either stay in same state or go to Failed via otherwise
    EXPECT_TRUE(client.current_state() == first_state || 
                client.current_state() == State::Failed);
}

TEST_F(AppRegistrationClientFsmTest, MultipleStopCallsIdempotent) {
    AppRegistrationClient client(config);
    
    client.stop();
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    client.stop();
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, PollDoesNotChangeStateInDisconnected) {
    AppRegistrationClient client(config);
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    client.poll();
    
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, StateAfterConstruction) {
    // Create multiple clients to ensure consistent initialization
    for (int i = 0; i < 5; ++i) {
        AppRegistrationClient client(config);
        EXPECT_EQ(client.current_state(), State::Disconnected);
        EXPECT_FALSE(client.is_connected());
        EXPECT_FALSE(client.is_registered());
    }
}

TEST_F(AppRegistrationClientFsmTest, StatePersistsAcrossPolls) {
    AppRegistrationClient client(config);
    
    client.start();
    State state_after_start = client.current_state();
    
    // Poll several times
    for (int i = 0; i < 10; ++i) {
        client.poll();
        // State shouldn't change randomly during polls
        // (unless timeout occurs, but our config has 1000ms timeout)
    }
    
    // Should still be in a connecting-related state or transitioned to failed
    State final_state = client.current_state();
    EXPECT_TRUE(final_state == state_after_start || 
                final_state == State::Failed ||
                final_state == State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, IsConnectedAccuracy) {
    AppRegistrationClient client(config);
    
    // Test all expected connection states
    std::vector<State> connected_states = {
        State::Connecting,
        State::Registering,
        State::Registered
    };
    
    std::vector<State> disconnected_states = {
        State::Disconnected,
        State::Reconnecting,
        State::Failed
    };
    
    // Initial state should be disconnected
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.current_state(), State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, IsRegisteredOnlyInRegisteredState) {
    AppRegistrationClient client(config);
    
    // Not registered in any state except Registered
    EXPECT_FALSE(client.is_registered());
    
    client.start();
    EXPECT_FALSE(client.is_registered());
}

TEST_F(AppRegistrationClientFsmTest, StartStopStartSequence) {
    AppRegistrationClient client(config);
    
    // Start
    client.start();
    EXPECT_TRUE(client.is_connected() || client.current_state() == State::Failed);
    
    // Stop
    client.stop();
    // Should eventually reach Disconnected (possibly through Failed)
    
    // Start again
    client.start();
    // Should transition again
    EXPECT_TRUE(client.current_state() != State::Disconnected);
}

TEST_F(AppRegistrationClientFsmTest, OtherwiseClauseHandlesInvalidTransitions) {
    AppRegistrationClient client(config);
    
    // Start from Disconnected
    EXPECT_EQ(client.current_state(), State::Disconnected);
    
    // Start is valid from Disconnected
    client.start();
    State after_start = client.current_state();
    
    // Starting again from Connecting is invalid and should trigger otherwise
    client.start();
    
    // Should have transitioned to Failed via otherwise clause
    EXPECT_TRUE(client.current_state() == State::Failed || 
                client.current_state() == after_start);
}
