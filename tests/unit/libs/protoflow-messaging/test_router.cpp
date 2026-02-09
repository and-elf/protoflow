#include <gtest/gtest.h>
#include <protoflow/messaging/router.hpp>

using namespace protoflow::messaging;

TEST(RouterTest, DefaultConstruction) {
    Router router;
    
    auto msg = MessageBuilder().id(1).build();
    auto routes = router.route(msg);
    
    EXPECT_TRUE(routes.empty());
}

TEST(RouterTest, AddDefaultRoute) {
    Router router;
    router.add_route(200);
    
    auto msg = MessageBuilder().id(1).build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], 200u);
}

TEST(RouterTest, MultipleDefaultRoutes) {
    Router router;
    router.add_route(100);
    router.add_route(200);
    router.add_route(300);
    
    auto msg = MessageBuilder().id(1).build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 3u);
    EXPECT_EQ(routes[0], 100u);
    EXPECT_EQ(routes[1], 200u);
    EXPECT_EQ(routes[2], 300u);
}

TEST(RouterTest, RouteByMessageId) {
    Router router;
    router.add_route_by_id(42, 500);
    
    auto msg1 = MessageBuilder().id(42).build();
    auto routes1 = router.route(msg1);
    
    ASSERT_EQ(routes1.size(), 1u);
    EXPECT_EQ(routes1[0], 500u);
    
    auto msg2 = MessageBuilder().id(43).build();
    auto routes2 = router.route(msg2);
    EXPECT_TRUE(routes2.empty());
}

TEST(RouterTest, RouteByMessageIdMultipleDestinations) {
    Router router;
    router.add_route_by_id(10, 100);
    router.add_route_by_id(10, 200);
    
    auto msg = MessageBuilder().id(10).build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 2u);
    EXPECT_EQ(routes[0], 100u);
    EXPECT_EQ(routes[1], 200u);
}

TEST(RouterTest, ConditionalRoute) {
    Router router;
    
    auto is_high_priority = [](const Message& msg) {
        return msg.header.priority == Priority::High;
    };
    
    router.add_route(is_high_priority, {300, 400});
    
    auto high_msg = MessageBuilder()
        .priority(Priority::High)
        .build();
    auto routes1 = router.route(high_msg);
    
    ASSERT_EQ(routes1.size(), 2u);
    EXPECT_EQ(routes1[0], 300u);
    EXPECT_EQ(routes1[1], 400u);
    
    auto normal_msg = MessageBuilder()
        .priority(Priority::Normal)
        .build();
    auto routes2 = router.route(normal_msg);
    EXPECT_TRUE(routes2.empty());
}

TEST(RouterTest, IdRouteOverConditional) {
    Router router;
    
    auto always_true = [](const Message&) { return true; };
    router.add_route(always_true, {100});
    router.add_route_by_id(5, 200);
    
    auto msg = MessageBuilder().id(5).build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], 200u);  // ID route wins over conditional
}

TEST(RouterTest, ConditionalRouteOverDefault) {
    Router router;
    
    router.add_route(999);  // Default
    
    auto match_id = [](const Message& msg) {
        return msg.header.id == 10;
    };
    router.add_route(match_id, {555});
    
    auto msg = MessageBuilder().id(10).build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], 555u);  // Conditional wins over default
}

TEST(RouterTest, MultipleConditionalRoutes) {
    Router router;
    
    auto is_high = [](const Message& msg) {
        return msg.header.priority == Priority::High;
    };
    auto is_low = [](const Message& msg) {
        return msg.header.priority == Priority::Low;
    };
    
    router.add_route(is_high, {100});
    router.add_route(is_low, {200});
    
    auto high_msg = MessageBuilder().priority(Priority::High).build();
    auto routes1 = router.route(high_msg);
    ASSERT_EQ(routes1.size(), 1u);
    EXPECT_EQ(routes1[0], 100u);
    
    auto low_msg = MessageBuilder().priority(Priority::Low).build();
    auto routes2 = router.route(low_msg);
    ASSERT_EQ(routes2.size(), 1u);
    EXPECT_EQ(routes2[0], 200u);
}

TEST(RouterTest, FirstMatchingConditionalWins) {
    Router router;
    
    auto always_true = [](const Message&) { return true; };
    
    router.add_route(always_true, {100});
    router.add_route(always_true, {200});
    
    auto msg = MessageBuilder().build();
    auto routes = router.route(msg);
    
    ASSERT_EQ(routes.size(), 1u);
    EXPECT_EQ(routes[0], 100u);  // First matching conditional wins
}

TEST(RouterTest, Clear) {
    Router router;
    
    router.add_route(100);
    router.add_route_by_id(5, 200);
    
    router.clear();
    
    auto msg = MessageBuilder().id(5).build();
    auto routes = router.route(msg);
    
    EXPECT_TRUE(routes.empty());
}

TEST(RouterTest, ComplexScenario) {
    Router router;
    
    // Default routes
    router.add_route(1000);
    router.add_route(2000);
    
    // ID-based route
    router.add_route_by_id(42, 3000);
    
    // Conditional route
    auto is_critical = [](const Message& msg) {
        return msg.header.priority == Priority::Critical;
    };
    router.add_route(is_critical, {4000, 5000});
    
    // Test default
    auto msg1 = MessageBuilder().id(1).build();
    auto r1 = router.route(msg1);
    EXPECT_EQ(r1.size(), 2u);
    
    // Test ID-based
    auto msg2 = MessageBuilder().id(42).build();
    auto r2 = router.route(msg2);
    ASSERT_EQ(r2.size(), 1u);
    EXPECT_EQ(r2[0], 3000u);
    
    // Test conditional
    auto msg3 = MessageBuilder().priority(Priority::Critical).build();
    auto r3 = router.route(msg3);
    ASSERT_EQ(r3.size(), 2u);
    EXPECT_EQ(r3[0], 4000u);
    EXPECT_EQ(r3[1], 5000u);
}
