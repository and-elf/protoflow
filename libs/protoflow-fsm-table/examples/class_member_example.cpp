#include <protoflow/fsm_table.hpp>
#include <iostream>

enum class State { idle, running };
enum class Event { start, stop };

// Tracer for observing transitions
struct tracer {
    void on_transition(State, Event, State) {
        std::cout << "Transition occurred\n";
    }
    void on_invalid(State, Event) {
        std::cout << "Invalid event\n";
    }
};

struct MyClass {
    void on_start() {
        std::cout << "Starting!\n";
    }
    
    void on_stop() {
        std::cout << "Stopping!\n";
    }
    
    // Table as static constexpr member - clean and simple!
    static constexpr auto state_table = 
        protoflow::fsm::table<State, Event>(
            protoflow::fsm::state<State::idle>(
                protoflow::fsm::on<Event::start> = protoflow::fsm::to<State::running>(&MyClass::on_start)
            ),
            protoflow::fsm::state<State::running>(
                protoflow::fsm::on<Event::stop> = protoflow::fsm::to<State::idle>(&MyClass::on_stop)
            )
        );
    
    void run() {
        tracer tr;
        protoflow::fsm::machine m{State::idle, *this, tr, state_table};
        
        m.dispatch(Event::start);
        m.dispatch(Event::stop);
    }
};

int main() {
    MyClass obj;
    obj.run();
    return 0;
}
