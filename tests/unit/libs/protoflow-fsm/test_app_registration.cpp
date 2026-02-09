#include <protoflow/fsm.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace protoflow::fsm;

// App registration states and events
enum class AppState {
    Unregistered,
    Registered,
    Alive,
    Dead
};

enum class AppEvent {
    Register,
    Heartbeat,
    Timeout,
    Disconnect
};

// String conversion
namespace protoflow::fsm {
    template<>
    inline std::string to_string(const AppState& state) {
        switch (state) {
            case AppState::Unregistered: return "Unregistered";
            case AppState::Registered: return "Registered";
            case AppState::Alive: return "Alive";
            case AppState::Dead: return "Dead";
        }
        return "Unknown";
    }

    template<>
    inline std::string to_string(const AppEvent& event) {
        switch (event) {
            case AppEvent::Register: return "Register";
            case AppEvent::Heartbeat: return "Heartbeat";
            case AppEvent::Timeout: return "Timeout";
            case AppEvent::Disconnect: return "Disconnect";
        }
        return "Unknown";
    }
}

TEST(AppRegistrationTest, RegistrationFlow) {
    NullSink sink;
    int register_count = 0;
    int heartbeat_count = 0;
    
    auto fsm = make_fsm(
        "app_registration",
        when<AppState::Unregistered, AppEvent::Register>()
            .then([&](){ register_count++; })
            .to<AppState::Registered>()
        | when<AppState::Registered, AppEvent::Heartbeat>()
            .then([&](){ heartbeat_count++; })
            .to<AppState::Alive>()
        | when<AppState::Alive, AppEvent::Heartbeat>()
            .then([&](){ heartbeat_count++; })
            .stay()
        | otherwise()
            .then([&](){ })
            .to<AppState::Unregistered>(),
        sink,
        AppState::Unregistered
    );
    
    // Register
    fsm.process(AppEvent::Register);
    EXPECT_EQ(fsm.state(), AppState::Registered);
    EXPECT_EQ(register_count, 1);
    
    // First heartbeat moves to Alive
    fsm.process(AppEvent::Heartbeat);
    EXPECT_EQ(fsm.state(), AppState::Alive);
    EXPECT_EQ(heartbeat_count, 1);
    
    // Subsequent heartbeats stay in Alive
    fsm.process(AppEvent::Heartbeat);
    EXPECT_EQ(fsm.state(), AppState::Alive);
    EXPECT_EQ(heartbeat_count, 2);
}

TEST(AppRegistrationTest, TimeoutFlow) {
    NullSink sink;
    int timeout_count = 0;
    
    auto fsm = make_fsm(
        "app_registration",
        when<AppState::Unregistered, AppEvent::Register>()
            .then([&](){ })
            .to<AppState::Registered>()
        | when<AppState::Registered, AppEvent::Heartbeat>()
            .then([&](){ })
            .to<AppState::Alive>()
        | when<AppState::Alive, AppEvent::Heartbeat>()
            .then([&](){ })
            .stay()
        | when<AppState::Alive, AppEvent::Timeout>()
            .then([&](){ timeout_count++; })
            .to<AppState::Dead>()
        | otherwise()
            .then([&](){ })
            .to<AppState::Unregistered>(),
        sink,
        AppState::Unregistered
    );
    
    // Get to Alive state
    fsm.process(AppEvent::Register);
    fsm.process(AppEvent::Heartbeat);
    EXPECT_EQ(fsm.state(), AppState::Alive);
    
    // Timeout
    fsm.process(AppEvent::Timeout);
    EXPECT_EQ(fsm.state(), AppState::Dead);
    EXPECT_EQ(timeout_count, 1);
}

TEST(AppRegistrationTest, Recovery) {
    NullSink sink;
    int register_count = 0;
    
    auto fsm = make_fsm(
        "app_registration",
        when<AppState::Unregistered, AppEvent::Register>()
            .then([&](){ register_count++; })
            .to<AppState::Registered>()
        | when<AppState::Registered, AppEvent::Heartbeat>()
            .then([&](){ })
            .to<AppState::Alive>()
        | when<AppState::Alive, AppEvent::Timeout>()
            .then([&](){ })
            .to<AppState::Dead>()
        | when<AppState::Dead, AppEvent::Register>()
            .then([&](){ register_count++; })
            .to<AppState::Registered>()
        | otherwise()
            .then([&](){ })
            .to<AppState::Unregistered>(),
        sink,
        AppState::Unregistered
    );
    
    // Normal flow to Dead
    fsm.process(AppEvent::Register);
    fsm.process(AppEvent::Heartbeat);
    fsm.process(AppEvent::Timeout);
    EXPECT_EQ(fsm.state(), AppState::Dead);
    EXPECT_EQ(register_count, 1);
    
    // Re-register from Dead
    fsm.process(AppEvent::Register);
    EXPECT_EQ(fsm.state(), AppState::Registered);
    EXPECT_EQ(register_count, 2);
}

TEST(AppRegistrationTest, InvalidTransitionTriggersOtherwise) {
    NullSink sink;
    int otherwise_count = 0;
    
    auto fsm = make_fsm(
        "app_registration",
        when<AppState::Unregistered, AppEvent::Register>()
            .then([&](){ })
            .to<AppState::Registered>()
        | when<AppState::Registered, AppEvent::Heartbeat>()
            .then([&](){ })
            .to<AppState::Alive>()
        | when<AppState::Alive, AppEvent::Timeout>()
            .then([&](){ })
            .to<AppState::Dead>()
        | otherwise()
            .then([&](){ otherwise_count++; })
            .to<AppState::Unregistered>(),
        sink,
        AppState::Unregistered
    );
    
    // Get to Dead state
    fsm.process(AppEvent::Register);
    fsm.process(AppEvent::Heartbeat);
    fsm.process(AppEvent::Timeout);
    EXPECT_EQ(fsm.state(), AppState::Dead);
    
    // Send heartbeat while Dead (invalid) -> triggers otherwise
    fsm.process(AppEvent::Heartbeat);
    EXPECT_EQ(fsm.state(), AppState::Unregistered);
    EXPECT_EQ(otherwise_count, 1);
}
