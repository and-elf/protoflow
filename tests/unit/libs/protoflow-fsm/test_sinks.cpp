#include <protoflow/fsm.hpp>
#include <protoflow/fsm/sinks.hpp>
#include <gtest/gtest.h>
#include <string>

using namespace protoflow::fsm;
using namespace protoflow::fsm::sinks;

enum class State {
    A,
    B
};

enum class Event {
    Go
};

namespace protoflow::fsm {
    template<>
    inline std::string to_string(const State& s) {
        return s == State::A ? "A" : "B";
    }

    template<>
    inline std::string to_string(const Event&) {
        return "Go";
    }
}

TEST(SinkTest, NullSink) {
    sinks::NullSink sink;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ })
            .to<State::B>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        sink,
        State::A
    );
    
    // Should not crash or throw
    fsm.process(Event::Go);
    EXPECT_EQ(fsm.state(), State::B);
}

TEST(SinkTest, MetricsSink) {
    MetricsSink metrics;
    
    auto fsm = make_fsm(
        "metrics_test",
        when<State::A, Event::Go>()
            .then([&](){ })
            .to<State::B>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        metrics,
        State::A
    );
    
    // Valid transition
    fsm.process(Event::Go);
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Transition), 1);
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Otherwise), 0);
    
    // Invalid transition (triggers otherwise)
    fsm.process(Event::Go);
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Transition), 1);
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Otherwise), 1);
    
    // Reset and verify
    metrics.reset();
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Transition), 0);
    EXPECT_EQ(metrics.get_count("metrics_test", FsmEvent::Kind::Otherwise), 0);
}

TEST(SinkTest, FanoutSink) {
    MetricsSink metrics1;
    MetricsSink metrics2;
    
    auto fanout = make_fanout_sink(metrics1, metrics2);
    
    auto fsm = make_fsm(
        "fanout_test",
        when<State::A, Event::Go>()
            .then([&](){ })
            .to<State::B>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        fanout,
        State::A
    );
    
    fsm.process(Event::Go);
    
    // Both sinks should receive the event
    EXPECT_EQ(metrics1.get_count("fanout_test", FsmEvent::Kind::Transition), 1);
    EXPECT_EQ(metrics2.get_count("fanout_test", FsmEvent::Kind::Transition), 1);
    
    fsm.process(Event::Go); // Triggers otherwise
    
    EXPECT_EQ(metrics1.get_count("fanout_test", FsmEvent::Kind::Otherwise), 1);
    EXPECT_EQ(metrics2.get_count("fanout_test", FsmEvent::Kind::Otherwise), 1);
}

struct CustomSink {
    std::shared_ptr<int> emission_count;
    
    CustomSink() : emission_count(std::make_shared<int>(0)) {}
    
    void emit(const FsmEvent&) const {
        (*emission_count)++;
    }
};

TEST(SinkTest, CustomSink) {
    CustomSink sink;
    
    auto fsm = make_fsm(
        "custom_test",
        when<State::A, Event::Go>()
            .then([&](){ })
            .to<State::B>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        sink,
        State::A
    );
    
    EXPECT_EQ(*sink.emission_count, 0);
    
    fsm.process(Event::Go);
    EXPECT_EQ(*sink.emission_count, 1);
    
    fsm.process(Event::Go);
    EXPECT_EQ(*sink.emission_count, 2);
}

TEST(SinkTest, MultipleFanoutSinks) {
    CustomSink sink1;
    CustomSink sink2;
    CustomSink sink3;
    
    auto fanout = make_fanout_sink(sink1, sink2, sink3);
    
    auto fsm = make_fsm(
        "multi_fanout",
        when<State::A, Event::Go>()
            .then([&](){ })
            .to<State::B>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        fanout,
        State::A
    );
    
    fsm.process(Event::Go);
    
    EXPECT_EQ(*sink1.emission_count, 1);
    EXPECT_EQ(*sink2.emission_count, 1);
    EXPECT_EQ(*sink3.emission_count, 1);
}
