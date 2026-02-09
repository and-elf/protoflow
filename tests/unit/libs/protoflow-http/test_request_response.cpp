#include <protoflow/http.hpp>
#include <gtest/gtest.h>

using namespace protoflow::http;

TEST(RequestTest, MethodConversion) {
    EXPECT_EQ(to_string(method::GET), "GET");
    EXPECT_EQ(to_string(method::POST), "POST");
    EXPECT_EQ(to_string(method::PUT), "PUT");
    EXPECT_EQ(to_string(method::DELETE), "DELETE");
    
    EXPECT_EQ(from_string("GET"), method::GET);
    EXPECT_EQ(from_string("POST"), method::POST);
    EXPECT_FALSE(from_string("INVALID").has_value());
}

TEST(RequestTest, Headers) {
    request req;
    req.set_header("Content-Type", "application/json");
    req.set_header("Accept", "text/html");
    
    EXPECT_EQ(req.header("Content-Type"), "application/json");
    EXPECT_EQ(req.header("Accept"), "text/html");
    EXPECT_FALSE(req.header("Missing").has_value());
}

TEST(RequestTest, QueryParameters) {
    request req;
    req.set_query("page", "1");
    req.set_query("limit", "10");
    
    EXPECT_EQ(req.query("page"), "1");
    EXPECT_EQ(req.query("limit"), "10");
    EXPECT_FALSE(req.query("missing").has_value());
}

TEST(RequestTest, PathParameters) {
    request req;
    req.set_path_param("id", "123");
    req.set_path_param("name", "test");
    
    EXPECT_EQ(req.path_param("id"), "123");
    EXPECT_EQ(req.path_param("name"), "test");
    EXPECT_FALSE(req.path_param("missing").has_value());
}

TEST(RequestTest, ContentNegotiation) {
    request req;
    
    req.set_header("Accept", "text/html");
    EXPECT_TRUE(req.accepts_html());
    EXPECT_FALSE(req.accepts_json());
    EXPECT_EQ(req.preferred_content_type(), content_type::html);
    
    req.set_header("Accept", "application/json");
    EXPECT_FALSE(req.accepts_html());
    EXPECT_TRUE(req.accepts_json());
    EXPECT_EQ(req.preferred_content_type(), content_type::json);
    
    req.set_header("Accept", "*/*");
    EXPECT_TRUE(req.accepts_html());
    EXPECT_EQ(req.preferred_content_type(), content_type::html);
}

TEST(ResponseTest, BasicResponse) {
    response resp(200, "OK");
    
    EXPECT_EQ(resp.get_status(), 200);
    EXPECT_EQ(resp.get_body(), "OK");
}

TEST(ResponseTest, Headers) {
    response resp;
    resp.header("Content-Type", "application/json")
        .header("Cache-Control", "no-cache");
    
    EXPECT_EQ(resp.get_header("Content-Type"), "application/json");
    EXPECT_EQ(resp.get_header("Cache-Control"), "no-cache");
}

TEST(ResponseTest, Helpers) {
    auto resp1 = ok();
    EXPECT_EQ(resp1.get_status(), 200);
    
    auto resp2 = ok("success");
    EXPECT_EQ(resp2.get_status(), 200);
    EXPECT_EQ(resp2.get_body(), "success");
    
    auto resp3 = created();
    EXPECT_EQ(resp3.get_status(), 201);
    
    auto resp4 = bad_request();
    EXPECT_EQ(resp4.get_status(), 400);
    
    auto resp5 = not_found();
    EXPECT_EQ(resp5.get_status(), 404);
    
    auto resp6 = internal_error();
    EXPECT_EQ(resp6.get_status(), 500);
}
