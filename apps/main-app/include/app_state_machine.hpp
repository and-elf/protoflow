#pragma once

#include <protoflow/fsm_table.hpp>
#include <string>

namespace protoflow::mainapp {

/// App lifecycle states
enum class AppState {
    unregistered,
    registered,
    alive,
    dead
};

/// App lifecycle events
enum class AppEvent {
    register_app,
    heartbeat,
    timeout,
    disconnect
};

/// FSM for managing individual app lifecycle state
/// This is a clean example of using FSM table as a class member
template<typename Sink>
class AppStateMachine {
public:
    AppStateMachine(const std::string& app_name, Sink sink)
        : app_name_(app_name)
        , sink_(std::move(sink))
        , machine_{AppState::unregistered, *this, sink_, state_table}
    {}

    void process(AppEvent event) {
        machine_.dispatch(event);
    }
    
    AppState state() const {
        return machine_.current_state();
    }

    const std::string& app_name() const {
        return app_name_;
    }

private:
    // Transition actions - just simple state updates
    // No service coupling - all business logic stays in the service layer
    void on_register() {
        // App just registered, waiting for first heartbeat
    }

    void on_reregister() {
        // App re-registered while already registered
    }

    void on_first_heartbeat() {
        // First heartbeat received, app is now alive
    }

    void on_heartbeat() {
        // Regular heartbeat, app still alive
    }

    void on_timeout() {
        // Heartbeat timeout, app is now dead
    }

    void on_disconnect() {
        // App disconnected
    }

    void on_recovery() {
        // Dead app re-registered
    }

    void on_recovery_heartbeat() {
        // Dead app sent heartbeat, recovering
    }

    std::string app_name_;
    Sink sink_;
    
    // FSM state table - the key pattern: static constexpr auto
    // No complex decltype(...) ceremony needed!
    static constexpr auto state_table = 
        fsm::table<AppState, AppEvent>(
            fsm::state<AppState::unregistered>(
                fsm::on<AppEvent::register_app> = fsm::to<AppState::registered>(&AppStateMachine::on_register)
            ),
            fsm::state<AppState::registered>(
                fsm::on<AppEvent::heartbeat> = fsm::to<AppState::alive>(&AppStateMachine::on_first_heartbeat),
                fsm::on<AppEvent::register_app> = fsm::to<AppState::registered>(&AppStateMachine::on_reregister),
                fsm::on<AppEvent::disconnect> = fsm::to<AppState::unregistered>(&AppStateMachine::on_disconnect)
            ),
            fsm::state<AppState::alive>(
                fsm::on<AppEvent::heartbeat> = fsm::to<AppState::alive>(&AppStateMachine::on_heartbeat),
                fsm::on<AppEvent::timeout> = fsm::to<AppState::dead>(&AppStateMachine::on_timeout),
                fsm::on<AppEvent::disconnect> = fsm::to<AppState::unregistered>(&AppStateMachine::on_disconnect)
            ),
            fsm::state<AppState::dead>(
                fsm::on<AppEvent::register_app> = fsm::to<AppState::registered>(&AppStateMachine::on_recovery),
                fsm::on<AppEvent::heartbeat> = fsm::to<AppState::alive>(&AppStateMachine::on_recovery_heartbeat),
                fsm::on<AppEvent::disconnect> = fsm::to<AppState::unregistered>(&AppStateMachine::on_disconnect)
            )
        );
    
    fsm::machine<AppState, AppEvent, AppStateMachine, Sink, decltype(state_table)> machine_;
};

} // namespace protoflow::mainapp
