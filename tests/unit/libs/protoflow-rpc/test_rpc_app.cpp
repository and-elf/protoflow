#include <protoflow/rpc/rpc_app.hpp>
#include <gtest/gtest.h>

using namespace protoflow::rpc;

// Mock RPC app for testing
class mock_rpc_app : public rpc_app {
public:
    app_registration registration() const override {
        return app_registration{
            .name = "test-app",
            .endpoints = {"/api/data", "/ui/dashboard"},
            .description = "Test application"
        };
    }

    std::string render_fragment(std::string_view fragment_id) override {
        if (fragment_id == "dashboard") {
            return "<div>Dashboard</div>";
        }
        if (fragment_id == "status") {
            return "<div>Status: OK</div>";
        }
        return "";
    }

    std::string get_state_json() const override {
        return R"({"status":"running","count":42})";
    }

    bool handle_custom_command(uint16_t command,
                              std::span<const std::byte> payload,
                              std::vector<std::byte>& response) override {
        if (command == 100) {
            // Echo command - copy payload to response
            response.assign(payload.begin(), payload.end());
            return true;
        }
        return false;
    }
};

class RpcAppTest : public ::testing::Test {
protected:
    mock_rpc_app app;
};

TEST_F(RpcAppTest, Registration) {
    auto reg = app.registration();

    EXPECT_EQ(reg.name, "test-app");
    EXPECT_EQ(reg.description, "Test application");
    ASSERT_EQ(reg.endpoints.size(), 2u);
    EXPECT_EQ(reg.endpoints[0], "/api/data");
    EXPECT_EQ(reg.endpoints[1], "/ui/dashboard");
}

TEST_F(RpcAppTest, RenderFragmentDashboard) {
    auto html = app.render_fragment("dashboard");
    EXPECT_EQ(html, "<div>Dashboard</div>");
}

TEST_F(RpcAppTest, RenderFragmentStatus) {
    auto html = app.render_fragment("status");
    EXPECT_EQ(html, "<div>Status: OK</div>");
}

TEST_F(RpcAppTest, RenderFragmentUnknown) {
    auto html = app.render_fragment("unknown");
    EXPECT_EQ(html, "");
}

TEST_F(RpcAppTest, GetStateJson) {
    auto json = app.get_state_json();
    EXPECT_EQ(json, R"({"status":"running","count":42})");
}

TEST_F(RpcAppTest, HandleCustomCommandSupported) {
    std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3}};
    std::vector<std::byte> response;

    bool handled = app.handle_custom_command(100, payload, response);

    EXPECT_TRUE(handled);
    ASSERT_EQ(response.size(), 3u);
    EXPECT_EQ(response[0], std::byte{1});
    EXPECT_EQ(response[1], std::byte{2});
    EXPECT_EQ(response[2], std::byte{3});
}

TEST_F(RpcAppTest, HandleCustomCommandUnsupported) {
    std::vector<std::byte> payload = {std::byte{1}};
    std::vector<std::byte> response;

    bool handled = app.handle_custom_command(999, payload, response);

    EXPECT_FALSE(handled);
    EXPECT_TRUE(response.empty());
}

TEST_F(RpcAppTest, AppRegistrationDefaultValues) {
    app_registration reg;
    
    EXPECT_TRUE(reg.name.empty());
    EXPECT_TRUE(reg.endpoints.empty());
    EXPECT_TRUE(reg.description.empty());
}

TEST_F(RpcAppTest, AppRegistrationCustomValues) {
    app_registration reg{
        .name = "custom-app",
        .endpoints = {"/api/v1", "/api/v2"},
        .description = "Custom application"
    };

    EXPECT_EQ(reg.name, "custom-app");
    ASSERT_EQ(reg.endpoints.size(), 2u);
    EXPECT_EQ(reg.endpoints[0], "/api/v1");
    EXPECT_EQ(reg.endpoints[1], "/api/v2");
    EXPECT_EQ(reg.description, "Custom application");
}
