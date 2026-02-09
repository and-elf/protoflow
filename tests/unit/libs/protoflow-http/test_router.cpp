#include <protoflow/http.hpp>
#include <gtest/gtest.h>

using namespace protoflow::http;

TEST(RouterTest, ExactPathMatch) {
    router r;
    
    bool handler_called = false;
    r.add_route(method::GET, "/test", [&](const request&) -> response {
        handler_called = true;
        return ok("matched");
    });
    
    request req(method::GET, "/test");
    auto resp = r.handle(req);
    
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(resp.get_body(), "matched");
}

TEST(RouterTest, PathNotFound) {
    router r;
    
    r.add_route(method::GET, "/test", [](const request&) {
        return ok("matched");
    });
    
    request req(method::GET, "/other");
    auto resp = r.handle(req);
    
    EXPECT_EQ(resp.get_status(), 404);
}

TEST(RouterTest, MethodNotMatching) {
    router r;
    
    r.add_route(method::GET, "/test", [](const request&) {
        return ok("matched");
    });
    
    request req(method::POST, "/test");
    auto resp = r.handle(req);
    
    EXPECT_EQ(resp.get_status(), 404);
}

TEST(RouterTest, PathParameters) {
    router r;
    
    std::string captured_id;
    r.add_route(method::GET, "/users/:id", [&](const request& req) -> response {
        auto id = req.path_param("id");
        if (id) {
            captured_id = *id;
            return ok("user " + std::string(*id));
        }
        return bad_request();
    });
    
    request req(method::GET, "/users/123");
    auto resp = r.handle(req);
    
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(captured_id, "123");
    EXPECT_EQ(resp.get_body(), "user 123");
}

TEST(RouterTest, MultiplePathParameters) {
    router r;
    
    std::string captured_category;
    std::string captured_id;
    
    r.add_route(method::GET, "/api/:category/:id", [&](const request& req) -> response {
        auto cat = req.path_param("category");
        auto id = req.path_param("id");
        
        if (cat && id) {
            captured_category = *cat;
            captured_id = *id;
            return ok("OK");
        }
        return bad_request();
    });
    
    request req(method::GET, "/api/users/456");
    auto resp = r.handle(req);
    
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(captured_category, "users");
    EXPECT_EQ(captured_id, "456");
}

TEST(RouterTest, MultipleRoutes) {
    router r;
    
    r.add_route(method::GET, "/route1", [](const request&) {
        return ok("route1");
    });
    
    r.add_route(method::GET, "/route2", [](const request&) {
        return ok("route2");
    });
    
    r.add_route(method::POST, "/route1", [](const request&) {
        return ok("post-route1");
    });
    
    {
        request req(method::GET, "/route1");
        auto resp = r.handle(req);
        EXPECT_EQ(resp.get_body(), "route1");
    }
    
    {
        request req(method::GET, "/route2");
        auto resp = r.handle(req);
        EXPECT_EQ(resp.get_body(), "route2");
    }
    
    {
        request req(method::POST, "/route1");
        auto resp = r.handle(req);
        EXPECT_EQ(resp.get_body(), "post-route1");
    }
}
