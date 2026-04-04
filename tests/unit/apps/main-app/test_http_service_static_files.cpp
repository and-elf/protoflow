#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include "services/http_service.hpp"
#include "services/handlers/static_handler.hpp"
#include <filesystem>
#include <fstream>

using namespace protoflow::mainapp;
using namespace protoflow::mainapp::handlers;

/// Test suite for HTTP service static file serving
class HttpServiceStaticFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<HTTPService>();
        
        // Create a temporary test static directory
        test_static_dir_ = std::filesystem::temp_directory_path() / 
                           ("protoflow_test_static_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(test_static_dir_);
        
        // Create test files
        create_test_file("style.json", R"({"color": "blue", "size": 12})");
        create_test_file("script.js", "console.log('test');");
        create_test_file("index.html", "<html><body>Test</body></html>");
        create_test_file("document.css", "body { margin: 0; }");
    }

    void TearDown() override {
        service_.reset();
        // Clean up test directory
        if (std::filesystem::exists(test_static_dir_)) {
            std::filesystem::remove_all(test_static_dir_);
        }
    }

    void create_test_file(const std::string& filename, const std::string& content) {
        std::ofstream file(test_static_dir_ / filename);
        file << content;
        file.close();
    }

    std::unordered_map<std::string, std::string> create_headers(const std::string& content_type = "") {
        std::unordered_map<std::string, std::string> headers;
        if (!content_type.empty()) {
            headers["Accept"] = content_type;
        }
        return headers;
    }

    std::unique_ptr<HTTPService> service_;
    std::filesystem::path test_static_dir_;
};

// Test 1: Verify static file is served with correct content
TEST_F(HttpServiceStaticFilesTest, ServeStaticJsonFile) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/style.json";
    req.headers = create_headers("application/json");

    // Manually set the static directory for testing
    service_->set_static_dir(test_static_dir_);

    // Act
    auto response = service_->handle_request(req);

    // Assert
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.headers["Content-Type"], "application/json; charset=utf-8");
    
    std::string body(reinterpret_cast<const char*>(response.body.data()), response.body.size());
    EXPECT_EQ(body, R"({"color": "blue", "size": 12})");
}

// Test 2: Verify different content types are set correctly
TEST_F(HttpServiceStaticFilesTest, ContentTypesForDifferentExtensions) {
    service_->set_static_dir(test_static_dir_.string());

    // Test CSS
    {
        HttpRequest req;
        req.method = "GET";
        req.path = "/static/document.css";
        auto response = service_->handle_request(req);
        EXPECT_EQ(response.status_code, 200);
        EXPECT_EQ(response.headers["Content-Type"], "text/css; charset=utf-8");
    }

    // Test JavaScript
    {
        HttpRequest req;
        req.method = "GET";
        req.path = "/static/script.js";
        auto response = service_->handle_request(req);
        EXPECT_EQ(response.status_code, 200);
        EXPECT_EQ(response.headers["Content-Type"], "application/javascript; charset=utf-8");
    }

    // Test HTML
    {
        HttpRequest req;
        req.method = "GET";
        req.path = "/static/index.html";
        auto response = service_->handle_request(req);
        EXPECT_EQ(response.status_code, 200);
        EXPECT_EQ(response.headers["Content-Type"], "text/html; charset=utf-8");
    }
}

// Test 3: Verify 404 for missing files
TEST_F(HttpServiceStaticFilesTest, Return404ForMissingFile) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/nonexistent.txt";

    service_->set_static_dir(test_static_dir_.string());

    // Act
    auto response = service_->handle_request(req);

    // Assert
    EXPECT_EQ(response.status_code, 404);
    std::string body(reinterpret_cast<const char*>(response.body.data()), response.body.size());
    EXPECT_THAT(body, ::testing::HasSubstr("Static file not found"));
}

// Test 4: Verify path traversal protection
TEST_F(HttpServiceStaticFilesTest, RejectPathTraversal) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/../../../etc/passwd";

    service_->set_static_dir(test_static_dir_.string());

    // Act
    auto response = service_->handle_request(req);

    // Assert
    EXPECT_EQ(response.status_code, 404);
}

// Test 5: Verify wildcard routing works for /static/* pattern
TEST_F(HttpServiceStaticFilesTest, WildcardRoutingMatchesStaticPath) {
    // The HTTPService should route requests to /static/* to the static handler
    // This test verifies that the routing actually works
    
    service_->set_static_dir(test_static_dir_.string());
    service_->start();

    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/style.json";

    // Act
    auto response = service_->handle_request(req);

    // Assert - should not be 404 (which would indicate routing failed)
    EXPECT_NE(response.status_code, 404) << "Static file routing failed - request was not routed to static handler";
    
    service_->stop();
}

// Test 6: Verify empty filename after /static/ is rejected
TEST_F(HttpServiceStaticFilesTest, RejectEmptyFilename) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/";

    service_->set_static_dir(test_static_dir_);

    // Act
    auto response = service_->handle_request(req);

    // Assert
    EXPECT_EQ(response.status_code, 400);
}

// Test 7: Verify invalid static path is rejected
TEST_F(HttpServiceStaticFilesTest, RejectInvalidStaticPath) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/api/data";  // Not a /static/* path

    service_->set_static_dir(test_static_dir_);

    // This test is about the security check in handle_static
    // It should reject paths that don't start with /static/
    auto response = handlers::handle_static(*service_, req);

    // Assert
    EXPECT_EQ(response.status_code, 400);
}

// Test 8: Verify static directory not found doesn't crash
TEST_F(HttpServiceStaticFilesTest, HandleEmptyStaticDirectory) {
    // Arrange
    HttpRequest req;
    req.method = "GET";
    req.path = "/static/style.json";

    // Set static dir to empty (as it would be if not found)
    service_->set_static_dir("");

    // Act - should not crash
    auto response = service_->handle_request(req);

    // Assert - should be 404 since no static dir
    EXPECT_EQ(response.status_code, 404);
}
