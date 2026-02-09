#pragma once

#include "sink.hpp"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace protoflow::fsm::sinks {

// Logging sink - emits FSM events to a logging system
// Note: This is a placeholder that outputs to a simple interface
// In a real system, this would integrate with protoflow-logging
struct LoggingSink {
    enum class LogLevel {
        Debug,
        Info,
        Warn,
        Error
    };

    std::string fsm_name;
    LogLevel min_level = LogLevel::Warn;

    // Placeholder log function - in real implementation would use protoflow-logging
    void log(LogLevel level, const FsmEvent& e) const {
        // In real implementation, this would call protoflow::logging::log()
        // For now, this is a placeholder that can be replaced later
        (void)level;
        (void)e;
    }

    void emit(const FsmEvent& e) const {
        if (e.kind == FsmEvent::Kind::Otherwise) {
            log(LogLevel::Error, e);
        } else {
            log(LogLevel::Debug, e);
        }
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
