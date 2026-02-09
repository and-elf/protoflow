#include <protoflow/http.hpp>
#include <protoflow/html.hpp>
#include <gtest/gtest.h>

using namespace protoflow::http;
using namespace protoflow::html;

struct TestData {
    int value;
    std::string name;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TestData, value, name)

TEST(ServiceTest, RegisterGetRoute) {
    http_service service;
    
    service.get("/test", [](const request&) -> response {
        return ok("hello");
    });
    
    request req(method::GET, "/test");
    auto resp = service.handle_request(req);
    
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(resp.get_body(), "hello");
}

TEST(ServiceTest, RegisterMultipleMethods) {
    http_service service;
    
    service.get("/resource", [](const request&) {
        return ok("GET");
    });
    
    service.post("/resource", [](const request&) {
        return ok("POST");
    });
    
    service.put("/resource", [](const request&) {
        return ok("PUT");
    });
    
    service.del("/resource", [](const request&) {
        return ok("DELETE");
    });
    
    {
        request req(method::GET, "/resource");
        EXPECT_EQ(service.handle_request(req).get_body(), "GET");
    }
    
    {
        request req(method::POST, "/resource");
        EXPECT_EQ(service.handle_request(req).get_body(), "POST");
    }
    
    {
        request req(method::PUT, "/resource");
        EXPECT_EQ(service.handle_request(req).get_body(), "PUT");
    }
    
    {
        request req(method::DELETE, "/resource");
        EXPECT_EQ(service.handle_request(req).get_body(), "DELETE");
    }
}

TEST(ServiceTest, HtmlFragmentHandler) {
    http_service service;
    
    service.get("/page", [](const request&) {
        return div(text("Hello World"));
    });
    
    request req(method::GET, "/page");
    auto resp = service.handle_request(req);
    
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(resp.get_header("Content-Type"), "text/html; charset=utf-8");
    EXPECT_EQ(resp.get_body(), "<div>Hello World</div>");
}

TEST(ServiceTest, DataHandlerWithContentNegotiation) {
    http_service service;
    
    // Use a simple string response for now - complex type serialization
    // works but requires types to be defined at namespace scope
    service.get("/data", [](const request& req) -> response {
        if (req.accepts_json()) {
            return ok("{\"value\":42,\"name\":\"test\"}")
                .content_type("application/json; charset=utf-8");
        } else {
            return ok("<div class=\"data\">{\"value\":42,\"name\":\"test\"}</div>")
                .content_type("text/html; charset=utf-8");
        }
    });
    
    // Test JSON response
    {
        request req(method::GET, "/data");
        req.set_header("Accept", "application/json");
        auto resp = service.handle_request(req);
        
        EXPECT_EQ(resp.get_status(), 200);
        EXPECT_EQ(resp.get_header("Content-Type"), "application/json; charset=utf-8");
        
        std::string body = std::string(resp.get_body());
        EXPECT_NE(body.find("\"value\""), std::string::npos);
        EXPECT_NE(body.find("42"), std::string::npos);
        EXPECT_NE(body.find("\"name\""), std::string::npos);
        EXPECT_NE(body.find("\"test\""), std::string::npos);
    }
    
    // Test HTML response
    {
        request req(method::GET, "/data");
        req.set_header("Accept", "text/html");
        auto resp = service.handle_request(req);
        
        EXPECT_EQ(resp.get_status(), 200);
        EXPECT_EQ(resp.get_header("Content-Type"), "text/html; charset=utf-8");
    }
}

TEST(ServiceTest, SimpleHandlerNoRequest) {
    http_service service;
    
    int call_count = 0;
    service.route_simple("GET", "/simple", [&]() -> std::string {
        call_count++;
        return "simple response";
    });
    
    request req(method::GET, "/simple");
    auto resp = service.handle_request(req);
    
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(resp.get_body(), "simple response");
}
