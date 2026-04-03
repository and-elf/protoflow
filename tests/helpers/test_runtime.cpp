#include "test_runtime.hpp"

#include <protoflow/logging/logging_service.hpp>
#include <protoflow/messaging/message.hpp>
#include <protoflow/transport/tcp.hpp>
#include <protoflow/rpc/protocol.hpp>
#include <thread>

namespace protoflow::tests {

TestRuntime::TestRuntime(uint16_t port, std::chrono::milliseconds app_reg_check_interval) {
    (void)port;
    (void)app_reg_check_interval;
    // Create logging service
    auto logging = std::make_unique<protoflow::logging::LoggingService>();
    logger_ = logging.get();
    services_.push_back(std::move(logging));

    // Create app registration service
    auto app = std::make_unique<mainapp::AppRegistrationService>();
    app_reg_ = app.get();
    services_.push_back(std::move(app));

    // Create RPC server service with TCP transport on provided port
    // struct TcpServerAdapter : public protoflow::rpc::transport_interface {
    //     protoflow::transport::tcp::tcp_server server;
    //     explicit TcpServerAdapter(const protoflow::transport::tcp::tcp_config& cfg) : server(cfg) {}

    //     bool send(std::span<const std::byte>) override { return false; }
    //     std::expected<std::vector<std::byte>, std::string> receive(size_t) override {
    //         return std::unexpected(std::string("receive() not supported on server transport"));
    //     }
    //     void close() override { server.stop(); }
    //     bool is_connected() const override { return server.is_listening(); }
    //     std::unique_ptr<protoflow::rpc::transport_interface> accept() override {
    //         auto r = server.accept();
    //         if (!r) return nullptr;
    //         struct TcpClientAdapter : public protoflow::rpc::transport_interface {
    //             protoflow::transport::tcp::tcp_client client;
    //             explicit TcpClientAdapter(protoflow::transport::tcp::tcp_client c) noexcept : client(std::move(c)) {}
    //             bool send(std::span<const std::byte> data) override {
    //                 auto res = client.send(data);
    //                 return res && *res > 0;
    //             }
    //             std::expected<std::vector<std::byte>, std::string> receive(size_t max_size) override {
    //                 auto r = client.receive(max_size);
    //                 if (r) return *r;
    //                 return std::unexpected(r.error().to_string());
    //             }
    //             void close() override { client.disconnect(); }
    //             bool is_connected() const override { return client.is_connected(); }
    //         };
    //         return std::make_unique<TcpClientAdapter>(std::move(*r));
    //     }
    // };

    // protoflow::transport::tcp::tcp_config cfg{};
    // cfg.host = "127.0.0.1";
    // cfg.port = port;
    // // Construct adapter, start listening, then move into RpcServerService
    // TcpServerAdapter adapter(cfg);
    // auto lres = adapter.server.listen(cfg.port);
    // if (!lres) throw std::runtime_error("Failed to start tcp_server: " + lres.error().to_string());
    // auto transport = std::make_unique<TcpServerAdapter>(std::move(adapter));
    // auto rpc = std::make_unique<mainapp::RpcServerService>(std::move(transport));
    // rpc_server_ = rpc.get();
    // services_.push_back(std::move(rpc));
}

TestRuntime::~TestRuntime() {
    stop();
}

void TestRuntime::start() {
    for (auto &s : services_) s->start();
}

void TestRuntime::stop() {
    for (auto &s : services_) s->stop();
}

void TestRuntime::poll_once() {
    // Call poll() on each service once
    for (auto &s : services_) s->poll();
    // Route outbound messages between services
    route_messages();
}

void TestRuntime::route_messages() {
    // Collect outbound messages
    for (auto &src : services_) {
        while (true) {
            auto opt = src->pop_outbound();
            if (!opt.has_value()) break;
            auto msg = *opt; // copyable
            // deliver to all other services (copy per destination)
            for (auto &dst : services_) {
                if (dst.get() == src.get()) continue;
                auto copy = msg; // make a copy for this destination
                dst->on_message(std::move(copy));
            }
        }
    }
}

} // namespace protoflow::tests
