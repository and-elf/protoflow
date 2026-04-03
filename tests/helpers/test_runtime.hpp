#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include <protoflow/logging/logging_service.hpp>
#include "services/app_registration_service.hpp"
#include "services/rpc_server_service.hpp"
#include <protoflow/service/service.hpp>

namespace protoflow::tests {

class TestRuntime {
public:
    explicit TestRuntime(uint16_t port, std::chrono::milliseconds app_reg_check_interval = std::chrono::milliseconds(100));
    ~TestRuntime();

    void start();
    void stop();
    void poll_once();

    const mainapp::AppRegistrationService* app_registration_service() const { return app_reg_; }

private:
    void route_messages();

    std::vector<std::unique_ptr<service::Service>> services_;
    protoflow::logging::LoggingService* logger_ = nullptr;
    mainapp::AppRegistrationService* app_reg_ = nullptr;
    mainapp::RpcServerService* rpc_server_ = nullptr;
};

} // namespace protoflow::tests
