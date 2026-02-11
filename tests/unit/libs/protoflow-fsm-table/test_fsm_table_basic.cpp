#include <gtest/gtest.h>
#include <protoflow/fsm_table.hpp>
#include <string>
#include <vector>

using namespace protoflow;

namespace {

// Example: Traffic light controller using FSM table as class member
enum class TrafficLightState {
    red,
    yellow,
    green
};

enum class TrafficLightEvent {
    timer_expired,
    emergency_override,
    reset
};

class TrafficLightController {
public:
    // This is the key pattern: static constexpr auto for the FSM table
    // No complex decltype(...) needed - clean and simple!
    static constexpr auto state_table = 
        table<TrafficLightState, TrafficLightEvent>(
            state<TrafficLightState::red>(
                on<TrafficLightEvent::timer_expired> = to<TrafficLightState::green>(&TrafficLightController::on_go_green),
                on<TrafficLightEvent::emergency_override> = to<TrafficLightState::red>(&TrafficLightController::on_stay_red),
                on<TrafficLightEvent::reset> = to<TrafficLightState::red>(&TrafficLightController::on_stay_red)
            ),
            state<TrafficLightState::yellow>(
                on<TrafficLightEvent::timer_expired> = to<TrafficLightState::red>(&TrafficLightController::on_go_red),
                on<TrafficLightEvent::emergency_override> = to<TrafficLightState::red>(&TrafficLightController::on_go_red),
                on<TrafficLightEvent::reset> = to<TrafficLightState::red>(&TrafficLightController::on_go_red)
            ),
            state<TrafficLightState::green>(
                on<TrafficLightEvent::timer_expired> = to<TrafficLightState::yellow>(&TrafficLightController::on_go_yellow),
                on<TrafficLightEvent::emergency_override> = to<TrafficLightState::red>(&TrafficLightController::on_emergency),
                on<TrafficLightEvent::reset> = to<TrafficLightState::red>(&TrafficLightController::on_emergency)
            )
        );

    TrafficLightController() 
        : fsm_{TrafficLightState::red, this, sink_, state_table}
    {}

    void process(TrafficLightEvent event) {
        fsm_.process(event);
    }

    TrafficLightState current_state() const {
        return fsm_.state();
    }

    const std::vector<std::string>& get_log() const {
        return log_;
    }

private:
    void on_go_green() {
        log_.push_back("Switching to GREEN");
    }

    void on_go_yellow() {
        log_.push_back("Switching to YELLOW");
    }

    void on_go_red() {
        log_.push_back("Switching to RED");
    }

    void on_stay_red() {
        log_.push_back("Staying RED");
    }

    void on_emergency() {
        log_.push_back("EMERGENCY - Switching to RED");
    }

    struct Sink {
        void on_transition(TrafficLightState, TrafficLightEvent, TrafficLightState) {}
        void on_reject(TrafficLightState, TrafficLightEvent) {}
    };

    Sink sink_;
    std::vector<std::string> log_;
    machine<TrafficLightState, TrafficLightEvent, TrafficLightController, Sink> fsm_;
};

// Example: Protocol handler with explicit reject transitions
enum class ProtocolState {
    idle,
    authenticating,
    connected,
    error
};

enum class ProtocolEvent {
    connect,
    auth_success,
    auth_failure,
    data_received,
    disconnect,
    timeout
};

class ProtocolHandler {
public:
    // Another clean example: FSM table as static constexpr auto member
    static constexpr auto state_table = 
        table<ProtocolState, ProtocolEvent>(
            state<ProtocolState::idle>(
                on<ProtocolEvent::connect> = to<ProtocolState::authenticating>(&ProtocolHandler::start_auth),
                on<ProtocolEvent::disconnect> = reject,
                on<ProtocolEvent::data_received> = reject
            ),
            state<ProtocolState::authenticating>(
                on<ProtocolEvent::auth_success> = to<ProtocolState::connected>(&ProtocolHandler::establish_connection),
                on<ProtocolEvent::auth_failure> = to<ProtocolState::error>(&ProtocolHandler::handle_auth_error),
                on<ProtocolEvent::timeout> = to<ProtocolState::idle>(&ProtocolHandler::timeout_reset),
                on<ProtocolEvent::disconnect> = to<ProtocolState::idle>(&ProtocolHandler::cleanup)
            ),
            state<ProtocolState::connected>(
                on<ProtocolEvent::data_received> = to<ProtocolState::connected>(&ProtocolHandler::process_data),
                on<ProtocolEvent::disconnect> = to<ProtocolState::idle>(&ProtocolHandler::cleanup),
                on<ProtocolEvent::timeout> = to<ProtocolState::error>(&ProtocolHandler::handle_timeout)
            ),
            state<ProtocolState::error>(
                on<ProtocolEvent::disconnect> = to<ProtocolState::idle>(&ProtocolHandler::cleanup),
                on<ProtocolEvent::timeout> = to<ProtocolState::idle>(&ProtocolHandler::timeout_reset)
            )
        );

    ProtocolHandler() 
        : fsm_{ProtocolState::idle, this, sink_, state_table}
    {}

    void process(ProtocolEvent event) {
        fsm_.process(event);
    }

    ProtocolState current_state() const {
        return fsm_.state();
    }

    int get_data_count() const {
        return data_count_;
    }

    int get_reject_count() const {
        return reject_count_;
    }

private:
    void start_auth() {
        // Initialize authentication
    }

    void establish_connection() {
        // Setup connection
    }

    void handle_auth_error() {
        // Log authentication failure
    }

    void timeout_reset() {
        // Reset on timeout
    }

    void cleanup() {
        data_count_ = 0;
    }

    void process_data() {
        data_count_++;
    }

    void handle_timeout() {
        // Handle connection timeout
    }

    struct Sink {
        Sink(ProtocolHandler* handler) : handler_(handler) {}
        
        void on_transition(ProtocolState, ProtocolEvent, ProtocolState) {}
        
        void on_reject(ProtocolState, ProtocolEvent) {
            handler_->reject_count_++;
        }

        ProtocolHandler* handler_;
    };

    Sink sink_{this};
    int data_count_ = 0;
    int reject_count_ = 0;
    machine<ProtocolState, ProtocolEvent, ProtocolHandler, Sink> fsm_;
};

} // namespace

TEST(FsmTableTest, TrafficLightBasicTransitions) {
    TrafficLightController controller;
    
    EXPECT_EQ(controller.current_state(), TrafficLightState::red);
    
    // Red -> Green
    controller.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller.current_state(), TrafficLightState::green);
    EXPECT_EQ(controller.get_log().size(), 1);
    EXPECT_EQ(controller.get_log()[0], "Switching to GREEN");
    
    // Green -> Yellow
    controller.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller.current_state(), TrafficLightState::yellow);
    EXPECT_EQ(controller.get_log().size(), 2);
    EXPECT_EQ(controller.get_log()[1], "Switching to YELLOW");
    
    // Yellow -> Red
    controller.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller.current_state(), TrafficLightState::red);
    EXPECT_EQ(controller.get_log().size(), 3);
    EXPECT_EQ(controller.get_log()[2], "Switching to RED");
}

TEST(FsmTableTest, TrafficLightEmergencyOverride) {
    TrafficLightController controller;
    
    // Red -> Green
    controller.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller.current_state(), TrafficLightState::green);
    
    // Green -> Red (emergency)
    controller.process(TrafficLightEvent::emergency_override);
    EXPECT_EQ(controller.current_state(), TrafficLightState::red);
    EXPECT_EQ(controller.get_log().size(), 2);
    EXPECT_EQ(controller.get_log()[1], "EMERGENCY - Switching to RED");
}

TEST(FsmTableTest, ProtocolHandlerAuthSuccess) {
    ProtocolHandler handler;
    
    EXPECT_EQ(handler.current_state(), ProtocolState::idle);
    
    // Idle -> Authenticating
    handler.process(ProtocolEvent::connect);
    EXPECT_EQ(handler.current_state(), ProtocolState::authenticating);
    
    // Authenticating -> Connected
    handler.process(ProtocolEvent::auth_success);
    EXPECT_EQ(handler.current_state(), ProtocolState::connected);
    
    // Process data
    handler.process(ProtocolEvent::data_received);
    EXPECT_EQ(handler.current_state(), ProtocolState::connected);
    EXPECT_EQ(handler.get_data_count(), 1);
    
    handler.process(ProtocolEvent::data_received);
    EXPECT_EQ(handler.get_data_count(), 2);
}

TEST(FsmTableTest, ProtocolHandlerAuthFailure) {
    ProtocolHandler handler;
    
    handler.process(ProtocolEvent::connect);
    EXPECT_EQ(handler.current_state(), ProtocolState::authenticating);
    
    // Authenticating -> Error
    handler.process(ProtocolEvent::auth_failure);
    EXPECT_EQ(handler.current_state(), ProtocolState::error);
    
    // Error -> Idle
    handler.process(ProtocolEvent::disconnect);
    EXPECT_EQ(handler.current_state(), ProtocolState::idle);
}

TEST(FsmTableTest, ProtocolHandlerRejectsInvalidEvents) {
    ProtocolHandler handler;
    
    EXPECT_EQ(handler.current_state(), ProtocolState::idle);
    EXPECT_EQ(handler.get_reject_count(), 0);
    
    // Try to receive data while idle - should be rejected
    handler.process(ProtocolEvent::data_received);
    EXPECT_EQ(handler.current_state(), ProtocolState::idle);
    EXPECT_EQ(handler.get_reject_count(), 1);
    
    // Try to disconnect while idle - should be rejected
    handler.process(ProtocolEvent::disconnect);
    EXPECT_EQ(handler.current_state(), ProtocolState::idle);
    EXPECT_EQ(handler.get_reject_count(), 2);
}

TEST(FsmTableTest, ProtocolHandlerConnectionTimeout) {
    ProtocolHandler handler;
    
    handler.process(ProtocolEvent::connect);
    handler.process(ProtocolEvent::auth_success);
    EXPECT_EQ(handler.current_state(), ProtocolState::connected);
    
    // Connected -> Error (timeout)
    handler.process(ProtocolEvent::timeout);
    EXPECT_EQ(handler.current_state(), ProtocolState::error);
}

TEST(FsmTableTest, MultipleControllerInstances) {
    TrafficLightController controller1;
    TrafficLightController controller2;
    
    // Both start in red
    EXPECT_EQ(controller1.current_state(), TrafficLightState::red);
    EXPECT_EQ(controller2.current_state(), TrafficLightState::red);
    
    // Advance controller1 to green
    controller1.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller1.current_state(), TrafficLightState::green);
    EXPECT_EQ(controller2.current_state(), TrafficLightState::red);
    
    // Advance controller2 to green
    controller2.process(TrafficLightEvent::timer_expired);
    EXPECT_EQ(controller1.current_state(), TrafficLightState::green);
    EXPECT_EQ(controller2.current_state(), TrafficLightState::green);
    
    // Each has independent log
    EXPECT_EQ(controller1.get_log().size(), 1);
    EXPECT_EQ(controller2.get_log().size(), 1);
}
