#pragma once

#include "sink.hpp"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <iostream>

namespace protoflow::fsm::sinks {

// Null sink - does nothing (for production)
struct NullSink {
    void emit(const FsmEvent&) const noexcept {}
};

// Logging sink - emits FSM events to logging output (for debugging/testing)
struct LoggingSink {
    std::string fsm_name;

    void emit(const FsmEvent& e) const {
        std::string kind = e.kind == FsmEvent::Kind::Otherwise ? "INVALID" : "TRANSITION";
        std::cout << "[FSM:" << e.fsm_name << "] " 
                  << kind << ": " 
                  << e.from_state << " --[" << e.event << "]--> " << e.to_state 
                  << std::endl;
    }
};

// Metrics sink - tracks transition counts
struct MetricsSink {
    std::shared_ptr<std::map<std::string, std::map<FsmEvent::Kind, size_t>>> counters;

    MetricsSink() : counters(std::make_shared<std::map<std::string, std::map<FsmEvent::Kind, size_t>>>()) {}

    void emit(const FsmEvent& e) const {
        (*counters)[e.fsm_name][e.kind]++;
    }

    size_t get_count(const std::string& fsm_name, FsmEvent::Kind kind) const {
        auto fsm_it = counters->find(fsm_name);
        if (fsm_it == counters->end()) return 0;
        
        auto kind_it = fsm_it->second.find(kind);
        if (kind_it == fsm_it->second.end()) return 0;
        
        return kind_it->second;
    }

    void reset() const {
        counters->clear();
    }
};

// Fanout sink - broadcasts events to multiple sinks
template<FsmSink... Sinks>
struct FanoutSink {
    std::tuple<Sinks...> sinks;

    explicit FanoutSink(Sinks... s) : sinks(std::move(s)...) {}

    void emit(const FsmEvent& e) const {
        std::apply([&](const auto&... s) {
            (s.emit(e), ...);
        }, sinks);
    }
};

// Helper to create a fanout sink
template<FsmSink... Sinks>
auto make_fanout_sink(Sinks... sinks) {
    return FanoutSink<Sinks...>{std::move(sinks)...};
}

} // namespace protoflow::fsm::sinks
