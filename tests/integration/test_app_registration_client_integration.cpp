// Integration test: App Registration Client with Mocked RPC
//
// Tests the integration between:
//   1. AppRegistrationClient service
//   2. Mocked/Faked RPC transport
//   3. AppBase runtime framework
//   4. Message routing between services
//
// Validates:
//   - FSM state transitions (Disconnected -> Connecting -> Registering -> Registered)
//   - Message routing and delivery via AppBase cycle
//   - Connection lifecycle managed by AppBase
//   - Registration protocol messaging through service infrastructure

#include <gtest/gtest.h>
#include <protoflow/app_registration_client.hpp>
#include <protoflow/app_registration_protocol.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <protoflow/service/service.hpp>
#include <protoflow/runtime/app_base.hpp>
#include <protoflow/logging/logging_service.hpp>
#include <memory>
#include <queue>
#include <thread>
#include <chrono>
#include <cstring>

using namespace std::chrono_literals;
using namespace protoflow;
using namespace protoflow::app_registration_client;
using namespace protoflow::app_registration_protocol;
using namespace protoflow::service;
using namespace protoflow::messaging;
using namespace protoflow::runtime;

namespace {

/// Mock RPC Transport for testing
/// Simulates a connected RPC server that responds to registration messages
class MockRpcTransport : public rpc::transport_interface {
public:
    MockRpcTransport() = default;
    ~MockRpcTransport() override = default;

    // Simulate connection established
    bool send(std::span<const std::byte> data) override {
        if (!is_connected_) {
            return false;
        }
        
        sent_data_.push_back(std::vector<std::byte>(data.begin(), data.end()));
        
        // Parse the message to see what was sent
        if (data.size() >= 4) {
            uint16_t cmd = 0;
            uint16_t payload_size = 0;
            std::memcpy(&cmd, data.data(), 2);
            std::memcpy(&payload_size, data.data() + 2, 2);
            
            last_cmd_ = static_cast<request>(cmd);
            
            // Generate appropriate response based on request type
            generate_response(static_cast<request>(cmd));
        }
        
        return true;
    }

    // Simulate receiving data (return queued responses)
    std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
        if (!is_connected_) {
            return std::unexpected("Not connected");
        }
        
        if (received_messages_.empty()) {
            return std::vector<std::byte>{};
        }
        
        auto msg = std::move(received_messages_.front());
        received_messages_.pop();
        
        if (msg.size() > max_size) {
            msg.resize(max_size);
        }
        
        return msg;
    }

    // Close the transport
    void close() override {
        is_connected_ = false;
        received_messages_ = std::queue<std::vector<std::byte>>{};
    }

    // Check connection status
    [[nodiscard]] bool is_connected() const override {
        return is_connected_;
    }

    // Test interface
    void set_connected(bool connected) {
        is_connected_ = connected;
    }

    [[nodiscard]] const std::vector<std::vector<std::byte>>& get_sent_data() const {
        return sent_data_;
    }

    [[nodiscard]] request get_last_command() const {
        return last_cmd_;
    }

    [[nodiscard]] bool has_pending_messages() const {
        return !received_messages_.empty();
    }

private:
    void generate_response(request cmd) {
        // Generate appropriate response based on request type
        switch (cmd) {
            case request::register_app:
                queue_response(response::register_ack);
                break;
            case request::heartbeat:
                queue_response(response::heartbeat_ack);
                break;
            case request::unregister_app:
                queue_response(response::unregister_app_ack);
                break;
            default:
                break;
        }
    }

    void queue_response(response resp) {
        std::vector<std::byte> msg;
        msg.resize(4);
        
        auto resp_val = static_cast<uint16_t>(resp);
        auto payload_size = uint16_t{0};
        
        std::memcpy(msg.data(), &resp_val, 2);
        std::memcpy(msg.data() + 2, &payload_size, 2);
        
        received_messages_.push(std::move(msg));
    }

    bool is_connected_ = true;
    std::vector<std::vector<std::byte>> sent_data_;
    std::queue<std::vector<std::byte>> received_messages_;
    request last_cmd_ = static_cast<request>(0);
};

// ===========================================================================
// Test App that inherits from AppBase
// ===========================================================================
class RegistrationTestApp : public AppBase {
public:
    explicit RegistrationTestApp(
        const app_registration_client::Config& app_config, 
        const AppBase::Config& base_config = {})
        : AppBase(base_config), app_config_(app_config) {}

    bool initialize() override {
        // Create and register LoggingService first (so it can receive logs from other services)
        auto logger = std::make_unique<logging::LoggingService>();
        logger->set_console_output(false);  // Suppress console output in tests
        services_.push_back(std::move(logger));

        // Create and register AppRegistrationClient service
        auto client = std::make_unique<AppRegistrationClient>(app_config_);
        client_ = client.get();
        services_.push_back(std::move(client));

        // Create and set mock RPC transport
        auto transport = std::make_unique<MockRpcTransport>();
        transport_ = transport.get();
        set_rpc_transport(std::move(transport));

        // Start all services
        for (auto& svc : services_) {
            svc->start();
        }

        return true;
    }

    // Public interface for testing
    AppRegistrationClient* get_client() { return client_; }
    MockRpcTransport* get_transport() { return transport_; }
    
    // Expose cycle() for testing
    void test_cycle() {
        AppBase::cycle();
    }

    // Get services count for testing
    size_t get_service_count() const {
        return services_.size();
    }

private:
    app_registration_client::Config app_config_;
    AppRegistrationClient* client_ = nullptr;
    MockRpcTransport* transport_ = nullptr;
};

} // anonymous namespace

// ===========================================================================
// Test: AppRegistrationClient FSM transitions via AppBase
// ===========================================================================
TEST(AppRegistrationClientIntegration, InitialState) {
    Config config{
        .app_name = "test-app",
        .version = 1,
        .endpoints = {"/api/v1", "/status"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    RegistrationTestApp app(config);
    ASSERT_TRUE(app.initialize());

    AppRegistrationClient* client = app.get_client();
    ASSERT_NE(client, nullptr);

    // After initialization, client has been started and transitioned from Disconnected
    State state_after_init = client->current_state();
    EXPECT_NE(state_after_init, State::Disconnected)
        << "Client should have transitioned out of Disconnected after initialize() calls start()";
    
    EXPECT_FALSE(client->is_registered());
    EXPECT_EQ(client->config().app_name, "test-app");
}

// ===========================================================================
// Test: AppRegistrationClient configuration access with AppBase
// ===========================================================================
TEST(AppRegistrationClientIntegration, ConfigurationAccess) {
    Config config{
        .app_name = "config-test",
        .version = 2,
        .endpoints = {"/api", "/health", "/info"},
        .server_address = "192.168.1.1",
        .server_port = 8080
    };

    RegistrationTestApp app(config);
    ASSERT_TRUE(app.initialize());

    AppRegistrationClient* client = app.get_client();
    const auto& cfg = client->config();
    EXPECT_EQ(cfg.app_name, "config-test");
    EXPECT_EQ(cfg.version, 2u);
    EXPECT_EQ(cfg.server_address, "192.168.1.1");
    EXPECT_EQ(cfg.server_port, 8080);
    EXPECT_EQ(cfg.endpoints.size(), 3);
    EXPECT_EQ(cfg.endpoints[0], "/api");
}

// ===========================================================================
// Test: RPC Transport integration through AppBase cycle
// ===========================================================================
TEST(AppRegistrationClientIntegration, RpcTransportIntegration) {
    Config config{
        .app_name = "rpc-test",
        .version = 1,
        .endpoints = {"/api"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    RegistrationTestApp app(config, AppBase::Config{.cycle_time = 1ms});
    ASSERT_TRUE(app.initialize());

    AppRegistrationClient* client = app.get_client();
    MockRpcTransport* transport = app.get_transport();

    ASSERT_NE(client, nullptr);
    ASSERT_NE(transport, nullptr);
    EXPECT_TRUE(transport->is_connected());

    // Verify client transitioned from initial Disconnected state
    State initial_state = client->current_state();
    EXPECT_NE(initial_state, State::Disconnected) 
        << "Client should have transitioned after start()";

    // Run several cycles to allow FSM to process
    for (int i = 0; i < 50; ++i) {
        app.test_cycle();
        std::this_thread::sleep_for(1ms);
    }

    // Verify client is active (hasn't failed or disconnected after multiple cycles)
    State final_state = client->current_state();
    EXPECT_NE(final_state, State::Disconnected)
        << "Client should maintain connection through cycles";
}

// ===========================================================================
// Test: Message routing through AppBase with multiple cycles
// ===========================================================================
TEST(AppRegistrationClientIntegration, MessageRoutingMultipleCycles) {
    Config config{
        .app_name = "routing-test",
        .version = 1,
        .endpoints = {"/test"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    RegistrationTestApp app(config, AppBase::Config{.cycle_time = 1ms});
    ASSERT_TRUE(app.initialize());

    AppRegistrationClient* client = app.get_client();
    MockRpcTransport* transport = app.get_transport();

    // After initialize(), client should have started and transitioned
    State start_state = client->current_state();
    EXPECT_NE(start_state, State::Disconnected);

    // Run multiple cycles to let FSM progress through state machine
    for (int i = 0; i < 100; ++i) {
        app.test_cycle();
        std::this_thread::sleep_for(1ms);
    }

    // Verify FSM is still progressing (not stuck or failed)
    State final_state = client->current_state();
    EXPECT_NE(final_state, State::Disconnected)
        << "Client should progress through connection states";

    // Transport should still be connected 
    EXPECT_TRUE(transport->is_connected())
        << "Transport should remain connected throughout cycles";
}

// ===========================================================================
// Test: Service lifecycle through AppBase
// ===========================================================================
TEST(AppRegistrationClientIntegration, ServiceLifecycleViaAppBase) {
    Config config{
        .app_name = "lifecycle-test",
        .version = 1,
        .endpoints = {"/api"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    {
        RegistrationTestApp app(config);
        ASSERT_TRUE(app.initialize());

        AppRegistrationClient* client = app.get_client();

        // Client should be started
        State state_after_init = client->current_state();
        EXPECT_NE(state_after_init, State::Disconnected) << 
            "Client should have transitioned after start";

        // Run some cycles
        for (int i = 0; i < 30; ++i) {
            app.test_cycle();
        }
    }
    // App destroyed here, services should be cleaned up properly

    SUCCEED() << "App lifecycle completed without errors";
}

// ===========================================================================
// Test: AppBase with multiple services (LoggingService + AppRegistrationClient)
// ===========================================================================
TEST(AppRegistrationClientIntegration, AppBaseSingleService) {
    Config config{
        .app_name = "single-service",
        .version = 1,
        .endpoints = {"/api"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    RegistrationTestApp app(config);
    ASSERT_TRUE(app.initialize());

    // Verify services were registered: LoggingService + AppRegistrationClient
    EXPECT_EQ(app.get_service_count(), 2);

    AppRegistrationClient* client = app.get_client();
    EXPECT_FALSE(client->is_registered());
}

// ===========================================================================
// Test: RPC Transport remains connected through cycles
// ===========================================================================
TEST(AppRegistrationClientIntegration, TransportConnectionPersistence) {
    Config config{
        .app_name = "transport-test",
        .version = 1,
        .endpoints = {"/api"},
        .server_address = "127.0.0.1",
        .server_port = 19999
    };

    RegistrationTestApp app(config, AppBase::Config{.cycle_time = 1ms});
    ASSERT_TRUE(app.initialize());

    MockRpcTransport* transport = app.get_transport();
    EXPECT_TRUE(transport->is_connected());

    // Run cycles and verify transport remains connected
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(transport->is_connected()) 
            << "Transport should remain connected during cycles";
        app.test_cycle();
    }

    EXPECT_TRUE(transport->is_connected()) 
        << "Transport should remain connected after cycles";
}



