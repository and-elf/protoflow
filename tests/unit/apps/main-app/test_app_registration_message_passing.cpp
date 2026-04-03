#include <gtest/gtest.h>
#include "services/app_registration_service.hpp"
#include <protoflow/app_registration_client/app_registration_client.hpp>
#include "runtime.hpp"

using namespace protoflow;
using namespace protoflow::mainapp;
using namespace protoflow::app_registration_client;

TEST(MessagePassing, ClientProtocolIgnoredByMainApp) {
    // Arrange: create service and client
    auto config = Runtime::Config{};
    Runtime runtime(std::move(config));

    AppRegistrationClient::Config cfg;
    cfg.app_name = "test-client";
    cfg.version = 1;
    cfg.server_address = "localhost";
    cfg.server_port = 12345;
    cfg.endpoints = {"me"};
    cfg.heartbeat_interval = std::chrono::seconds(5);
    cfg.connection_timeout = std::chrono::milliseconds(500);
    cfg.max_reconnect_attempts = 3;

    app_registration_client::AppRegistrationClient client{cfg};

    // Start both
    runtime.start();
    client.start();

    // Act: run one poll cycle for client to produce outbound protocol messages
    client.poll();

    // Retrieve outbound from client and forward to service
    if (auto out = client.pop_outbound()) {
        // Forward raw protocol payload as a messaging::Message to the AppRegistrationService
        runtime.on_message(std::move(*out));
    }

    // Let service poll process the message (if any)
    runtime.poll();
    // Assert: main-app should NOT treat protocol payload as AppRegistrationEvent
    // (no app registered under client's name)
    const AppRegistrationService& const_service = service;
    EXPECT_FALSE(const_service.get_app("test-client").has_value());

    // Cleanup
    client.stop();
    service.stop();
}
