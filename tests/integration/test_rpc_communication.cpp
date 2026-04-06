// Integration test: RPC Server & Client Communication
//
// Tests the core RPC message passing in a real client-server scenario:
//   1. Server accepts connections
//   2. Client sends hello and receives hello_ack
//   3. Messages exchange successfully
//   4. Connection failures are detected
//   5. Concurrent clients work correctly

#include <gtest/gtest.h>
#include <protoflow/rpc/rpc_server.hpp>
#include <protoflow/rpc/rpc_client.hpp>
#include <protoflow/transport/tcp.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <memory>
#include <algorithm>

using namespace protoflow::rpc;
using namespace protoflow::transport::tcp;
using namespace std::chrono_literals;

namespace {

// Test ports
constexpr uint16_t RPC_TEST_PORT = 19140;
constexpr const char* RPC_TEST_HOST = "127.0.0.1";

} // anonymous namespace

// ===========================================================================
// Test: RPC Server starts and listens on TCP port
// ===========================================================================
TEST(RpcCommunication, ServerListensOnPort) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT
    };
    
    tcp_server server(cfg);
    auto listen_result = server.listen();
    ASSERT_TRUE(listen_result.has_value()) << "Failed to start TCP server: " 
        << listen_result.error().to_string();
    
    // Verify we can connect to it
    std::thread accept_thread([&server]() {
        if (auto conn = server.accept_client()) {
            // Got a connection, just let it close
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT);
    auto connect_result = client.connect();
    ASSERT_TRUE(connect_result.has_value()) << "Failed to connect to RPC server: "
        << connect_result.error().to_string();
    
    client.disconnect();
    accept_thread.join();
}

// ===========================================================================
// Test: RPC Client and Server exchange hello/hello_ack
// ===========================================================================
TEST(RpcCommunication, HelloHandshake) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT + 1
    };
    
    tcp_server server(cfg);
    auto srv_listen = server.listen();
    ASSERT_TRUE(srv_listen.has_value());
    
    rpc_server_base rpc_server;
    rpc_client_base rpc_client;
    
    std::atomic<bool> server_done = false;
    std::atomic<bool> server_ok = false;
    
    // Server thread: accept connection, receive hello, send hello_ack
    std::thread server_thread([&]() {
        if (auto conn_result = server.accept_client()) {
            auto& conn = conn_result.value();
            // Receive hello
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                auto hdr = hdr_result.value();
                if (static_cast<cmd>(hdr.cmd) == cmd::hello) {
                    // Send hello_ack
                    if (rpc_server.send_hello_ack(conn)) {
                        server_ok = true;
                    }
                }
            }
        }
        server_done = true;
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Client: connect, send hello, receive hello_ack
    tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT + 1);
    if (auto connect_result = client.connect()) {
        // Send hello
        if (rpc_client.send_hello(client)) {
            // Receive hello_ack
            if (auto hdr_result = rpc_client.receive_header(client)) {
                auto hdr = hdr_result.value();
                EXPECT_EQ(static_cast<cmd>(hdr.cmd), cmd::hello_ack);
            }
        }
    }
    
    server_thread.join();
    EXPECT_TRUE(server_done) << "Server did not complete";
    EXPECT_TRUE(server_ok) << "Server did not complete handshake";
}

// ===========================================================================
// Test: RPC Message Exchange (client sends data, server receives)
// ===========================================================================
TEST(RpcCommunication, ClientSendServerReceive) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT + 2
    };
    
    tcp_server server(cfg);
    auto srv_listen = server.listen();
    ASSERT_TRUE(srv_listen.has_value());
    
    rpc_server_base rpc_server;
    rpc_client_base rpc_client;
    
    const std::string test_message = "Hello from RPC client";
    std::span<const std::byte> test_payload{
        reinterpret_cast<const std::byte*>(test_message.data()),
        test_message.size()
    };
    
    std::atomic<bool> payload_received = false;
    
    // Server thread: accept, recv header, recv payload
    std::thread server_thread([&]() {
        if (auto conn_result = server.accept_client()) {
            auto& conn = conn_result.value();
            
            // Receive hello first
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                auto hdr = hdr_result.value();
                if (static_cast<cmd>(hdr.cmd) == cmd::hello) {
                    // Send hello_ack
                    rpc_server.send_hello_ack(conn);
                }
            }
            
            // Now receive message
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                auto hdr = hdr_result.value();
                if (hdr.payload_size > 0) {
                    if (auto payload_result = rpc_server.receive_payload(conn, hdr.payload_size)) {
                        auto payload = payload_result.value();
                        std::string received(reinterpret_cast<const char*>(payload.data()), payload.size());
                        if (received == test_message) {
                            payload_received = true;
                        }
                    }
                }
            }
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Client: connect, send hello, receive ack, then send message
    tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT + 2);
    if (auto connect_result = client.connect()) {
        // Hello handshake
        rpc_client.send_hello(client);
        if (auto hdr_result = rpc_client.receive_header(client)) {
            auto hdr = hdr_result.value();
            if (static_cast<cmd>(hdr.cmd) == cmd::hello_ack) {
                // Now send test message
                rpc_client.send_message(client, cmd::hello, test_payload);
            }
        }
    }
    
    server_thread.join();
    EXPECT_TRUE(payload_received) << "Server did not receive correct message";
}

// ===========================================================================
// Test: Server detects client disconnection
// ===========================================================================
TEST(RpcCommunication, ClientDisconnectionDetected) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT + 3
    };
    
    tcp_server server(cfg);
    auto srv_listen = server.listen();
    ASSERT_TRUE(srv_listen.has_value());
    
    rpc_server_base rpc_server;
    rpc_client_base rpc_client;
    
    std::atomic<bool> detected_disconnect = false;
    
    // Server thread: accept connection, try to receive until disconnect
    std::thread server_thread([&]() {
        if (auto conn_result = server.accept_client()) {
            auto& conn = conn_result.value();
            
            // Receive hello
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                rpc_server.send_hello_ack(conn);
            }
            
            // Try to receive more, should get disconnect error
            for (int i = 0; i < 5; ++i) {
                if (auto hdr_result = rpc_server.receive_header(conn)) {
                    // Got something (might just be waiting)
                } else {
                    // Got error (expected after disconnect)
                    detected_disconnect = true;
                    break;
                }
                std::this_thread::sleep_for(100ms);
            }
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Client: connect, send hello, then close
    tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT + 3);
    if (auto connect_result = client.connect()) {
        rpc_client.send_hello(client);
        if (auto hdr_result = rpc_client.receive_header(client)) {
            // Got ack, now close
        }
        client.disconnect();
    }
    
    std::this_thread::sleep_for(500ms);
    server_thread.join();
    
    EXPECT_TRUE(detected_disconnect) << "Server should detect client disconnect";
}

// ===========================================================================
// Test: Concurrent clients connect to same server
// ===========================================================================
TEST(RpcCommunication, ConcurrentClientConnections) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT + 4
    };
    
    tcp_server server(cfg);
    auto srv_listen = server.listen();
    ASSERT_TRUE(srv_listen.has_value());
    
    const int NUM_CLIENTS = 5;
    std::vector<std::thread> accept_threads;
    std::atomic<int> successful_handshakes = 0;
    
    rpc_server_base rpc_server;
    
    // Spawn server accept threads
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        accept_threads.emplace_back([&, i]() {
            if (auto conn_result = server.accept_client()) {
                auto& conn = conn_result.value();
                if (auto hdr_result = rpc_server.receive_header(conn)) {
                    auto hdr = hdr_result.value();
                    if (static_cast<cmd>(hdr.cmd) == cmd::hello) {
                        if (rpc_server.send_hello_ack(conn)) {
                            successful_handshakes++;
                        }
                    }
                }
            }
        });
    }
    
    std::this_thread::sleep_for(100ms);
    
    // Spawn client threads
    rpc_client_base rpc_client;
    std::vector<std::thread> client_threads;
    
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        client_threads.emplace_back([&, i]() {
            tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT + 4);
            if (auto connect_result = client.connect()) {
                rpc_client.send_hello(client);
                if (auto hdr_result = rpc_client.receive_header(client)) {
                    // Got response
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : client_threads) t.join();
    for (auto& t : accept_threads) t.join();
    
    EXPECT_EQ(successful_handshakes, NUM_CLIENTS)
        << "Not all concurrent clients completed handshake";
}

// ===========================================================================
// Test: Large message payload transmission
// ===========================================================================
TEST(RpcCommunication, LargePayloadTransmission) {
    tcp_config cfg{
        .host = RPC_TEST_HOST,
        .port = RPC_TEST_PORT + 5
    };
    
    tcp_server server(cfg);
    auto srv_listen = server.listen();
    ASSERT_TRUE(srv_listen.has_value());
    
    rpc_server_base rpc_server;
    rpc_client_base rpc_client;
    
    // Create large payload (100KB)
    const size_t PAYLOAD_SIZE = 100 * 1024;
    std::vector<uint8_t> large_payload_data(PAYLOAD_SIZE);
    for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
        large_payload_data[i] = static_cast<uint8_t>(i % 256);
    }
    std::span<const std::byte> large_payload{
        reinterpret_cast<const std::byte*>(large_payload_data.data()),
        PAYLOAD_SIZE
    };
    
    std::atomic<bool> payload_verified = false;
    
    // Server thread
    std::thread server_thread([&]() {
        if (auto conn_result = server.accept_client()) {
            auto& conn = conn_result.value();
            
            // Hello handshake
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                auto hdr = hdr_result.value();
                if (static_cast<cmd>(hdr.cmd) == cmd::hello) {
                    rpc_server.send_hello_ack(conn);
                }
            }
            
            // Receive large payload
            if (auto hdr_result = rpc_server.receive_header(conn)) {
                auto hdr = hdr_result.value();
                if (hdr.payload_size == PAYLOAD_SIZE) {
                    if (auto payload_result = rpc_server.receive_payload(conn, hdr.payload_size)) {
                        auto received = payload_result.value();
                        payload_verified = std::equal(received.begin(), received.end(),
                                                      large_payload.begin(), large_payload.end());
                    }
                }
            }
        }
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Client thread
    tcp_client client(RPC_TEST_HOST, RPC_TEST_PORT + 5);
    if (auto connect_result = client.connect()) {
        // Hello handshake
        rpc_client.send_hello(client);
        if (auto hdr_result = rpc_client.receive_header(client)) {
            // Got ack, send large payload
            rpc_client.send_message(client, cmd::hello, large_payload);
        }
    }
    
    server_thread.join();
    EXPECT_TRUE(payload_verified) << "Large payload was not transmitted correctly";
}
