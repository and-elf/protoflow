#include <protoflow/transport/tcp.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <array>

using namespace protoflow::transport::tcp;
using namespace std::chrono_literals;

class TcpConfigTest : public ::testing::Test {};

TEST_F(TcpConfigTest, DefaultValues) {
    tcp_config config;
    
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.connect_timeout_ms, 5000u);
    EXPECT_EQ(config.read_timeout_ms, 1000u);
    EXPECT_EQ(config.write_timeout_ms, 1000u);
    EXPECT_EQ(config.buffer_size, 8192u);
    EXPECT_TRUE(config.reuse_addr);
    EXPECT_TRUE(config.nodelay);
}

TEST_F(TcpConfigTest, CustomValues) {
    tcp_config config{
        .host = "192.168.1.1",
        .port = 9000,
        .connect_timeout_ms = 10000,
        .read_timeout_ms = 2000,
        .write_timeout_ms = 2000,
        .buffer_size = 16384,
        .reuse_addr = false,
        .nodelay = false
    };

    EXPECT_EQ(config.host, "192.168.1.1");
    EXPECT_EQ(config.port, 9000);
    EXPECT_EQ(config.connect_timeout_ms, 10000u);
    EXPECT_EQ(config.read_timeout_ms, 2000u);
    EXPECT_EQ(config.write_timeout_ms, 2000u);
    EXPECT_EQ(config.buffer_size, 16384u);
    EXPECT_FALSE(config.reuse_addr);
    EXPECT_FALSE(config.nodelay);
}

class TcpErrorTest : public ::testing::Test {};

TEST_F(TcpErrorTest, ErrorConstruction) {
    tcp_error err{42, "Test error"};
    
    EXPECT_EQ(err.error_code, 42);
    EXPECT_EQ(err.message, "Test error");
}

TEST_F(TcpErrorTest, ErrorToString) {
    tcp_error err{42, "Test error"};
    auto str = err.to_string();
    
    EXPECT_NE(str.find("42"), std::string::npos);
    EXPECT_NE(str.find("Test error"), std::string::npos);
}

class TcpClientTest : public ::testing::Test {
protected:
    tcp_config config;

    void SetUp() override {
        config.port = 9999;
        config.connect_timeout_ms = 100;  // Short timeout for tests
    }
};

TEST_F(TcpClientTest, DefaultConstruction) {
    tcp_client client;
    
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.state(), connection_state::disconnected);
    EXPECT_EQ(client.socket_fd(), -1);
}

TEST_F(TcpClientTest, ConfigConstruction) {
    tcp_client client(config);
    
    EXPECT_EQ(client.config().port, 9999);
    EXPECT_FALSE(client.is_connected());
}

TEST_F(TcpClientTest, MoveConstructor) {
    tcp_client client1(config);
    tcp_client client2(std::move(client1));
    
    EXPECT_EQ(client2.config().port, 9999);
    EXPECT_EQ(client1.socket_fd(), -1);  // Moved from
}

TEST_F(TcpClientTest, MoveAssignment) {
    tcp_client client1(config);
    tcp_client client2;
    
    client2 = std::move(client1);
    
    EXPECT_EQ(client2.config().port, 9999);
    EXPECT_EQ(client1.socket_fd(), -1);  // Moved from
}

TEST_F(TcpClientTest, ConnectFailureNoServer) {
    tcp_client client(config);
    
    auto result = client.connect("127.0.0.1", 9998);  // Nothing listening
    
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(client.state(), connection_state::connected);
}

TEST_F(TcpClientTest, ConnectInvalidAddress) {
    tcp_client client;
    
    auto result = client.connect("invalid.address", 8080);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, SendWhenNotConnected) {
    tcp_client client;
    
    std::array<std::byte, 10> data{};
    auto result = client.send(data);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, ReceiveWhenNotConnected) {
    tcp_client client;
    
    auto result = client.receive(100);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, DisconnectWhenNotConnected) {
    tcp_client client;
    
    EXPECT_NO_THROW(client.disconnect());
    EXPECT_EQ(client.state(), connection_state::disconnected);
}

TEST_F(TcpClientTest, PeerInfoWhenNotConnected) {
    tcp_client client;
    
    EXPECT_TRUE(client.peer_address().empty());
    EXPECT_EQ(client.peer_port(), 0);
}

class TcpServerTest : public ::testing::Test {
protected:
    tcp_config config;

    void SetUp() override {
        config.port = 10000;
    }
};

TEST_F(TcpServerTest, DefaultConstruction) {
    tcp_server server;
    
    EXPECT_FALSE(server.is_listening());
    EXPECT_EQ(server.socket_fd(), -1);
}

TEST_F(TcpServerTest, ConfigConstruction) {
    tcp_server server(config);
    
    EXPECT_EQ(server.config().port, 10000);
    EXPECT_FALSE(server.is_listening());
}

TEST_F(TcpServerTest, MoveConstructor) {
    tcp_server server1(config);
    tcp_server server2(std::move(server1));
    
    EXPECT_EQ(server2.config().port, 10000);
    EXPECT_EQ(server1.socket_fd(), -1);  // Moved from
}

TEST_F(TcpServerTest, MoveAssignment) {
    tcp_server server1(config);
    tcp_server server2;
    
    server2 = std::move(server1);
    
    EXPECT_EQ(server2.config().port, 10000);
    EXPECT_EQ(server1.socket_fd(), -1);  // Moved from
}

TEST_F(TcpServerTest, ListenSuccess) {
    tcp_server server;
    
    auto result = server.listen(10001);  // Specific port
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(server.is_listening());
    EXPECT_EQ(server.listening_port(), 10001);
    
    server.stop();
}

TEST_F(TcpServerTest, ListenTwice) {
    tcp_server server;
    
    EXPECT_TRUE(server.listen(10002).has_value());
    
    // Try to listen again
    auto result = server.listen(10003);
    EXPECT_FALSE(result.has_value());
    
    server.stop();
}

TEST_F(TcpServerTest, StopWhenNotListening) {
    tcp_server server;
    
    EXPECT_NO_THROW(server.stop());
}

TEST_F(TcpServerTest, AcceptWhenNotListening) {
    tcp_server server;
    
    auto result = server.accept();
    
    EXPECT_FALSE(result.has_value());
}

// Integration tests - client and server together
class TcpIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint16_t test_port = 10100;
    
    tcp_server server;
    tcp_client client;

    void SetUp() override {
        tcp_config config;
        config.port = test_port;
        server.set_config(config);
        client.set_config(config);
    }

    void TearDown() override {
        client.disconnect();
        server.stop();
    }
};

TEST_F(TcpIntegrationTest, ClientServerConnection) {
    // Start server
    ASSERT_TRUE(server.listen(test_port).has_value());

    // Connect client in separate thread
    std::thread client_thread([this]() {
        std::this_thread::sleep_for(50ms);
        auto result = client.connect("127.0.0.1", test_port);
        EXPECT_TRUE(result.has_value());
    });

    // Accept connection
    auto accepted_client = server.accept();
    EXPECT_TRUE(accepted_client.has_value());

    if (accepted_client) {
        EXPECT_TRUE(accepted_client->is_connected());
    }

    client_thread.join();
}

TEST_F(TcpIntegrationTest, SendReceive) {
    // Start server
    ASSERT_TRUE(server.listen(test_port).has_value());

    // Data to send
    const std::vector<std::byte> test_data = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}
    };

    // Client thread
    std::thread client_thread([this, &test_data]() {
        std::this_thread::sleep_for(50ms);
        ASSERT_TRUE(client.connect("127.0.0.1", test_port).has_value());
        
        auto sent = client.send(test_data);
        EXPECT_TRUE(sent.has_value());
        EXPECT_EQ(*sent, test_data.size());
    });

    // Server accepts and receives
    auto accepted_client = server.accept();
    ASSERT_TRUE(accepted_client.has_value());

    auto received = accepted_client->receive(test_data.size());
    EXPECT_TRUE(received.has_value());
    
    if (received) {
        EXPECT_EQ(received->size(), test_data.size());
        EXPECT_EQ(*received, test_data);
    }

    client_thread.join();
}

TEST_F(TcpIntegrationTest, BidirectionalCommunication) {
    // Start server
    ASSERT_TRUE(server.listen(test_port).has_value());

    const std::vector<std::byte> client_data = {std::byte{0xAA}, std::byte{0xBB}};
    const std::vector<std::byte> server_data = {std::byte{0xCC}, std::byte{0xDD}};

    std::vector<std::byte> server_received;
    std::vector<std::byte> client_received;

    // Client thread
    std::thread client_thread([this, &client_data, &client_received]() {
        std::this_thread::sleep_for(50ms);
        ASSERT_TRUE(client.connect("127.0.0.1", test_port).has_value());
        
        // Send to server
        auto sent = client.send(client_data);
        EXPECT_TRUE(sent.has_value());

        // Receive from server
        auto received = client.receive(2);
        EXPECT_TRUE(received.has_value());
        if (received) {
            client_received = *received;
        }
    });

    // Server accepts, receives, and sends
    auto accepted_client = server.accept();
    ASSERT_TRUE(accepted_client.has_value());

    // Receive from client
    auto received = accepted_client->receive(2);
    EXPECT_TRUE(received.has_value());
    if (received) {
        server_received = *received;
    }

    // Send to client
    auto sent = accepted_client->send(server_data);
    EXPECT_TRUE(sent.has_value());

    client_thread.join();

    // Verify data
    EXPECT_EQ(server_received, client_data);
    EXPECT_EQ(client_received, server_data);
}

TEST_F(TcpIntegrationTest, MultipleClients) {
    // Start server
    ASSERT_TRUE(server.listen(test_port).has_value());

    const int num_clients = 3;
    std::vector<std::thread> client_threads;
    std::vector<tcp_client> clients;

    // Create clients
    for (int i = 0; i < num_clients; ++i) {
        client_threads.emplace_back([this, i]() {
            std::this_thread::sleep_for(50ms);
            tcp_client local_client;
            auto result = local_client.connect("127.0.0.1", test_port);
            EXPECT_TRUE(result.has_value());
            
            std::vector<std::byte> data = {std::byte(i)};
            if (result) {
                (void)local_client.send(data);
            }
        });
    }

    // Accept connections
    std::vector<tcp_client> accepted_clients;
    for (int i = 0; i < num_clients; ++i) {
        auto accepted = server.accept();
        EXPECT_TRUE(accepted.has_value());
        if (accepted) {
            accepted_clients.push_back(std::move(*accepted));
        }
    }

    EXPECT_EQ(accepted_clients.size(), num_clients);

    for (auto& thread : client_threads) {
        thread.join();
    }
}

TEST_F(TcpIntegrationTest, PeerInformation) {
    // Start server
    ASSERT_TRUE(server.listen(test_port).has_value());

    std::thread client_thread([this]() {
        std::this_thread::sleep_for(50ms);
        [[maybe_unused]] auto result = client.connect("127.0.0.1", test_port);
    });

    auto accepted_client = server.accept();
    ASSERT_TRUE(accepted_client.has_value());

    if (accepted_client) {
        auto addr = accepted_client->peer_address();
        auto port = accepted_client->peer_port();
        
        EXPECT_FALSE(addr.empty());
        EXPECT_NE(port, 0);
    }

    client_thread.join();
}
