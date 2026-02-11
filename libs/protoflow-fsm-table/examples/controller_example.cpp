#include <protoflow/fsm_table.hpp>
#include <iostream>
#include <string_view>

// Example types from the specification
enum class State { idle, running, stopped };
enum class Event { start, stop, tick };

// Helper to convert enum to string
constexpr std::string_view to_string(State s) {
    switch (s) {
        case State::idle: return "idle";
        case State::running: return "running";
        case State::stopped: return "stopped";
    }
    return "unknown";
}

constexpr std::string_view to_string(Event e) {
    switch (e) {
        case Event::start: return "start";
        case Event::stop: return "stop";
        case Event::tick: return "tick";
    }
    return "unknown";
}

// Context object with action handlers
struct Controller {
    void start() {
        std::cout << "  [Controller] Starting...\n";
    }
    
    void stop() {
        std::cout << "  [Controller] Stopping...\n";
    }
    
    void tick() {
        std::cout << "  [Controller] Tick!\n";
    }
};

// Sink/tracer for observing transitions
struct tracer {
    void on_transition(State from, Event ev, State to) {
        std::cout << "  [Tracer] Transition: " 
                  << to_string(from) << " --[" << to_string(ev) << "]--> " 
                  << to_string(to) << "\n";
    }
    
    void on_invalid(State from, Event ev) {
        std::cout << "  [Tracer] INVALID: " 
                  << to_string(from) << " --[" << to_string(ev) << "]--> (rejected)\n";
    }
};

int main() {
    using namespace protoflow::fsm;
    
    // Define the state machine table - can easily be a class member!
    constexpr auto state_table =
        table<State, Event>(
            state<State::idle>(
                on<Event::start> = to<State::running>(&Controller::start),
                on<Event::stop>  = reject
            ),
            state<State::running>(
                on<Event::tick>  = to<State::running>(&Controller::tick),
                on<Event::stop>  = to<State::stopped>(&Controller::stop)
            ),
            state<State::stopped>(
                on<Event::start> = to<State::running>(&Controller::start),
                on<Event::tick>  = reject,
                on<Event::stop>  = reject
            )
        );
    
    // Create context and tracer
    Controller ctrl;
    tracer tr;
    
    // Create the machine (using CTAD)
    machine m{State::idle, ctrl, tr, state_table};
    
    std::cout << "Initial state: " << to_string(m.current_state()) << "\n\n";
    
    // Test sequence from specification
    std::cout << "Dispatching Event::start:\n";
    m.dispatch(Event::start);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    std::cout << "Dispatching Event::tick:\n";
    m.dispatch(Event::tick);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    std::cout << "Dispatching Event::stop:\n";
    m.dispatch(Event::stop);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    // Test some invalid transitions
    std::cout << "Dispatching Event::tick (should be invalid):\n";
    m.dispatch(Event::tick);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    std::cout << "Dispatching Event::stop (should be invalid):\n";
    m.dispatch(Event::stop);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    std::cout << "Dispatching Event::start:\n";
    m.dispatch(Event::start);
    std::cout << "Current state: " << to_string(m.current_state()) << "\n\n";
    
    return 0;
}
