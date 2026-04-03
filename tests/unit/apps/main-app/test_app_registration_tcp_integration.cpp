#include <gtest/gtest.h>
// #include "../../../tests/helpers/test_runtime.hpp"
// #include <protoflow/transport/tcp.hpp>
// #include <protoflow/app_registration_protocol/messages.hpp>
// #include <protoflow/rpc/protocol.hpp>
#include <thread>
#include <chrono>
#include <protoflow/rpc.hpp>

using namespace protoflow;
using namespace protoflow::mainapp;
using namespace protoflow::rpc;

// // Helper to build RPC wire buffer for register_app
// static std::vector<std::byte> make_register_app_wire(const std::string& name, const std::vector<std::string>& endpoints) {
//     app_registration_protocol::register_app_msg msg{};
//     std::strncpy(msg.name, name.c_str(), sizeof(msg.name) - 1);
//     msg.endpoint_count = static_cast<uint32_t>(endpoints.size());

//     std::vector<std::byte> payload(sizeof(msg));
//     std::memcpy(payload.data(), &msg, sizeof(msg));

//     for (const auto& e : endpoints) {
//         size_t old = payload.size();
//         payload.resize(old + e.size() + 1);
//         std::memcpy(payload.data() + old, e.c_str(), e.size() + 1);
//     }

//     rpc_header hdr = make_header(cmd::register_app, static_cast<uint32_t>(payload.size()));
//     std::vector<std::byte> wire;
//     wire.resize(rpc_header::wire_size + payload.size());
//     std::memcpy(wire.data(), &hdr, rpc_header::wire_size);
//     std::memcpy(wire.data() + rpc_header::wire_size, payload.data(), payload.size());
//     return wire;
// }

TEST(MainAppTcpIntegration, RegisterAppOverTcp) {
    const uint16_t port = 9123;

    
}
