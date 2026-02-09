#include <protoflow/fsm.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>

using namespace protoflow::fsm;

// Test states and events
enum class State {
    A,
    B,
    C
};

enum class Event {
    Go,
    Stop
};

// String conversion for testing
namespace protoflow::fsm {
    template<>
    inline std::string to_string(const State& s) {
        switch (s) {
            case State::A: return "A";
            case State::B: return "B";
            case State::C: return "C";
        }
        return "?";
    }

    template<>
    inline std::string to_string(const Event& e) {
        switch (e) {
            case Event::Go: return "Go";
            case Event::Stop: return "Stop";
        }
        return "?";
    }
}

// Test sink that captures events via shared pointer
struct TestSink {
    std::shared_ptr<std::vector<FsmEvent>> events;
    
    TestSink() : events(std::make_shared<std::vector<FsmEvent>>()) {}
    
    void emit(const FsmEvent& e) const {
        events->push_back(e);
    }
};

TEST(FsmBasicTest, InitialState) {
    TestSink sink;
    int action_count = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ action_count++; })
            .to<State::B>()
        | otherwise()
            .then([&](){ action_count++; })
            .to<State::A>(),
        sink,
        State::A
    );
    
    EXPECT_EQ(fsm.state(), State::A);
    EXPECT_EQ(action_count, 0);
    EXPECT_TRUE(sink.events->empty());
}

TEST(FsmBasicTest, SimpleTransition) {
    TestSink sink;
    int action_count = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ action_count++; })
            .to<State::B>()
        | otherwise()
            .then([&](){ action_count++; })
            .to<State::A>(),
        sink,
        State::A
    );
    
    fsm.process(Event::Go);
    
    EXPECT_EQ(fsm.state(), State::B);
    EXPECT_EQ(action_count, 1);
    ASSERT_EQ(sink.events->size(), 1);
    EXPECT_EQ((*sink.events)[0].fsm_name, "test");
    EXPECT_EQ((*sink.events)[0].from_state, "A");
    EXPECT_EQ((*sink.events)[0].to_state, "B");
    EXPECT_EQ((*sink.events)[0].event, "Go");
    EXPECT_EQ((*sink.events)[0].kind, FsmEvent::Kind::Transition);
}

TEST(FsmBasicTest, MultipleTransitions) {
    TestSink sink;
    int action_count = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ action_count++; })
            .to<State::B>()
        | when<State::B, Event::Go>()
            .then([&](){ action_count++; })
            .to<State::C>()
        | when<State::C, Event::Stop>()
            .then([&](){ action_count++; })
            .to<State::A>()
        | otherwise()
            .then([&](){ action_count++; })
            .to<State::A>(),
        sink,
        State::A
    );
    
    fsm.process(Event::Go);
    EXPECT_EQ(fsm.state(), State::B);
    EXPECT_EQ(action_count, 1);
    
    fsm.process(Event::Go);
    EXPECT_EQ(fsm.state(), State::C);
    EXPECT_EQ(action_count, 2);
    
    fsm.process(Event::Stop);
    EXPECT_EQ(fsm.state(), State::A);
    EXPECT_EQ(action_count, 3);
    
    EXPECT_EQ(sink.events->size(), 3);
}

TEST(FsmBasicTest, StayInSameState) {
    TestSink sink;
    int action_count = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ action_count++; })
            .stay()
        | otherwise()
            .then([&](){ action_count++; })
            .to<State::A>(),
        sink,
        State::A
    );
    
    fsm.process(Event::Go);
    
    EXPECT_EQ(fsm.state(), State::A);
    EXPECT_EQ(action_count, 1);
    ASSERT_EQ(sink.events->size(), 1);
    EXPECT_EQ((*sink.events)[0].from_state, "A");
    EXPECT_EQ((*sink.events)[0].to_state, "A");
}

TEST(FsmBasicTest, OtherwiseTransition) {
    TestSink sink;
    int regular_actions = 0;
    int otherwise_actions = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ regular_actions++; })
            .to<State::B>()
        | otherwise()
            .then([&](){ otherwise_actions++; })
            .to<State::A>(),
        sink,
        State::A
    );
    
    // Invalid event triggers otherwise
    fsm.process(Event::Stop);
    
    EXPECT_EQ(fsm.state(), State::A);
    EXPECT_EQ(regular_actions, 0);
    EXPECT_EQ(otherwise_actions, 1);
    ASSERT_EQ(sink.events->size(), 1);
    EXPECT_EQ((*sink.events)[0].kind, FsmEvent::Kind::Otherwise);
    EXPECT_EQ((*sink.events)[0].from_state, "A");
    EXPECT_EQ((*sink.events)[0].to_state, "A");
}

TEST(FsmBasicTest, EventValueMatching) {
    TestSink sink;
    int go_count = 0;
    int stop_count = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ go_count++; })
            .to<State::B>()
        | when<State::A, Event::Stop>()
            .then([&](){ stop_count++; })
            .to<State::C>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        sink,
        State::A
    );
    
    fsm.process(Event::Go);
    EXPECT_EQ(fsm.state(), State::B);
    EXPECT_EQ(go_count, 1);
    EXPECT_EQ(stop_count, 0);
    
    // Reset to A using otherwise
    fsm.process(Event::Go);
    
    fsm.process(Event::Stop);
    EXPECT_EQ(fsm.state(), State::C);
    EXPECT_EQ(go_count, 1);
    EXPECT_EQ(stop_count, 1);
}

TEST(FsmBasicTest, StateMatching) {
    TestSink sink;
    int a_actions = 0;
    int b_actions = 0;
    
    auto fsm = make_fsm(
        "test",
        when<State::A, Event::Go>()
            .then([&](){ a_actions++; })
            .to<State::B>()
        | when<State::B, Event::Go>()
            .then([&](){ b_actions++; })
            .to<State::A>()
        | otherwise()
            .then([&](){ })
            .to<State::A>(),
        sink,
        State::A
    );
    
    fsm.process(Event::Go);
    EXPECT_EQ(a_actions, 1);
    EXPECT_EQ(b_actions, 0);
    
    fsm.process(Event::Go);
    EXPECT_EQ(a_actions, 1);
    EXPECT_EQ(b_actions, 1);
}
