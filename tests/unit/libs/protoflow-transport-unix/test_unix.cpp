#include <protoflow/transport/unix.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <array>
#include <filesystem>

using namespace protoflow::transport::unix;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

class UnixConfigTest : public ::testing::Test {};

TEST_F(UnixConfigTest, DefaultValues) {
    unix_config config;
    
    EXPECT_EQ(config.socket_path, "/tmp/protoflow.sock");
    EXPECT_EQ(config.connect_timeout_ms, 5000u);
    EXPECT_EQ(config.read_timeout_ms, 1000u);
    EXPECT_EQ(config.write_timeout_ms, 1000u);
    EXPECT_EQ(config.buffer_size, 8192u);
    EXPECT_EQ(config.socket_permissions, static_cast<mode_t>(0600));
    EXPECT_FALSE(config.abstract_namespace);
}

TEST_F(UnixConfigTest, CustomValues) {
    unix_config config{
        .socket_path = "/tmp/custom.sock",
        .connect_timeout_ms = 10000,
        .read_timeout_ms = 2000,
        .write_timeout_ms = 2000,
        .buffer_size = 16384,
        .socket_permissions = 0660,
        .abstract_namespace = true
    };

    EXPECT_EQ(config.socket_path, "/tmp/custom.sock");
    EXPECT_EQ(config.connect_timeout_ms, 10000u);
    EXPECT_EQ(config.read_timeout_ms, 2000u);
    EXPECT_EQ(config.write_timeout_ms, 2000u);
    EXPECT_EQ(config.buffer_size, 16384u);
    EXPECT_EQ(config.socket_permissions, static_cast<mode_t>(0660));
    EXPECT_TRUE(config.abstract_namespace);
}

class UnixErrorTest : public ::testing::Test {};

TEST_F(UnixErrorTest, ErrorConstruction) {
    unix_error err{42, "Test error"};
    
    EXPECT_EQ(err.error_code, 42);
    EXPECT_EQ(err.message, "Test error");
}

TEST_F(UnixErrorTest, ErrorToString) {
    unix_error err{42, "Test error"};
    auto str = err.to_string();
    
    EXPECT_NE(str.find("42"), std::string::npos);
    EXPECT_NE(str.find("Test error"), std::string::npos);
}

class UnixClientTest : public ::testing::Test {
protected:
    unix_config config;

    void SetUp() override {
        config.socket_path = "/tmp/test_client.sock";
        config.connect_timeout_ms = 100;
    }

    void TearDown() override {
        fs::remove(config.socket_path);
    }
};

TEST_F(UnixClientTest, DefaultConstruction) {
    unix_client client;
    
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.state(), connection_state::disconnected);
    EXPECT_EQ(client.socket_fd(), -1);
}

TEST_F(UnixClientTest, ConfigConstruction) {
    unix_client client(config);
    
    EXPECT_EQ(client.config().socket_path, "/tmp/test_client.sock");
    EXPECT_FALSE(client.is_connected());
}

TEST_F(UnixClientTest, MoveConstructor) {
    unix_client client1(config);
    unix_client client2(std::move(client1));
    
    EXPECT_EQ(client2.config().socket_path, "/tmp/test_client.sock");
    EXPECT_EQ(client1.socket_fd(), -1);
}

TEST_F(UnixClientTest, MoveAssignment) {
    unix_client client1(config);
    unix_client client2;
    
    client2 = std::move(client1);
    
    EXPECT_EQ(client2.config().socket_path, "/tmp/test_client.sock");
    EXPECT_EQ(client1.socket_fd(), -1);
}

TEST_F(UnixClientTest, ConnectFailureNoServer) {
    unix_client client(config);
    
    auto result = client.connect("/tmp/nonexistent.sock");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(client.state(), connection_state::connected);
}

TEST_F(UnixClientTest, SendWhenNotConnected) {
    unix_client client;
    
    std::array<std::byte, 10> data{};
    auto result = client.send(data);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(UnixClientTest, ReceiveWhenNotConnected) {
    unix_client client;
    
    auto result = client.receive(100);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(UnixClientTest, DisconnectWhenNotConnected) {
    unix_client client;
    
    EXPECT_NO_THROW(client.disconnect());
    EXPECT_EQ(client.state(), connection_state::disconnected);
}

class UnixServerTest : public ::testing::Test {
protected:
    unix_config config;

    void SetUp() override {
        config.socket_path = "/tmp/test_server.sock";
    }

    void TearDown() override {
        fs::remove(config.socket_path);
    }
};

TEST_F(UnixServerTest, DefaultConstruction) {
    unix_server server;
    
    EXPECT_FALSE(server.is_listening());
    EXPECT_EQ(server.socket_fd(), -1);
}

TEST_F(UnixServerTest, ConfigConstruction) {
    unix_server server(config);
    
    EXPECT_EQ(server.config().socket_path, "/tmp/test_server.sock");
    EXPECT_FALSE(server.is_listening());
}

TEST_F(UnixServerTest, MoveConstructor) {
    unix_server server1(config);
    unix_server server2(std::move(server1));
    
    EXPECT_EQ(server2.config().socket_path, "/tmp/test_server.sock");
    EXPECT_EQ(server1.socket_fd(), -1);
}

TEST_F(UnixServerTest, MoveAssignment) {
    unix_server server1(config);
    unix_server server2;
    
    server2 = std::move(server1);
    
    EXPECT_EQ(server2.config().socket_path, "/tmp/test_server.sock");
    EXPECT_EQ(server1.socket_fd(), -1);
}

TEST_F(UnixServerTest, ListenSuccess) {
    unix_server server;
    
    auto result = server.listen("/tmp/test_listen.sock");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(server.is_listening());
    EXPECT_EQ(server.socket_path(), "/tmp/test_listen.sock");
    
    server.stop();
    fs::remove("/tmp/test_listen.sock");
}

TEST_F(UnixServerTest, ListenTwice) {
    unix_server server;
    
    EXPECT_TRUE(server.listen("/tmp/test_twice.sock").has_value());
    
    auto result = server.listen("/tmp/test_twice2.sock");
    EXPECT_FALSE(result.has_value());
    
    server.stop();
    fs::remove("/tmp/test_twice.sock");
}

TEST_F(UnixServerTest, StopWhenNotListening) {
    unix_server server;
    
    EXPECT_NO_THROW(server.stop());
}

TEST_F(UnixServerTest, AcceptWhenNotListening) {
    unix_server server;
    
    auto result = server.accept();
    
    EXPECT_FALSE(result.has_value());
}

// Integration tests
class UnixIntegrationTest : public ::testing::Test {
protected:
    static constexpr const char* test_socket = "/tmp/test_integration.sock";
    
    unix_server server;
    unix_client client;

    void SetUp() override {
        unix_config config;
        config.socket_path = test_socket;
        server.set_config(config);
        client.set_config(config);
        
        fs::remove(test_socket);
    }

    void TearDown() override {
        client.disconnect();
        server.stop();
        fs::remove(test_socket);
    }
};

TEST_F(UnixIntegrationTest, ClientServerConnection) {
    ASSERT_TRUE(server.listen(test_socket).has_value());

    std::thread client_thread([this]() {
        std::this_thread::sleep_for(50ms);
        auto result = client.connect(test_socket);
        EXPECT_TRUE(result.has_value());
    });

    auto accepted_client = server.accept();
    EXPECT_TRUE(accepted_client.has_value());

    if (accepted_client) {
        EXPECT_TRUE(accepted_client->is_connected());
    }

    client_thread.join();
}

TEST_F(UnixIntegrationTest, SendReceive) {
    ASSERT_TRUE(server.listen(test_socket).has_value());

    const std::vector<std::byte> test_data = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}
    };

    std::thread client_thread([this, &test_data]() {
        std::this_thread::sleep_for(50ms);
        ASSERT_TRUE(client.connect(test_socket).has_value());
        
        auto sent = client.send(test_data);
        EXPECT_TRUE(sent.has_value());
        EXPECT_EQ(*sent, test_data.size());
    });

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

TEST_F(UnixIntegrationTest, BidirectionalCommunication) {
    ASSERT_TRUE(server.listen(test_socket).has_value());

    const std::vector<std::byte> client_data = {std::byte{0xAA}, std::byte{0xBB}};
    const std::vector<std::byte> server_data = {std::byte{0xCC}, std::byte{0xDD}};

    std::vector<std::byte> server_received;
    std::vector<std::byte> client_received;

    std::thread client_thread([this, &client_data, &client_received]() {
        std::this_thread::sleep_for(50ms);
        ASSERT_TRUE(client.connect(test_socket).has_value());
        
        auto sent = client.send(client_data);
        EXPECT_TRUE(sent.has_value());

        auto received = client.receive(2);
        EXPECT_TRUE(received.has_value());
        if (received) {
            client_received = *received;
        }
    });

    auto accepted_client = server.accept();
    ASSERT_TRUE(accepted_client.has_value());

    auto received = accepted_client->receive(2);
    EXPECT_TRUE(received.has_value());
    if (received) {
        server_received = *received;
    }

    auto sent = accepted_client->send(server_data);
    EXPECT_TRUE(sent.has_value());

    client_thread.join();

    EXPECT_EQ(server_received, client_data);
    EXPECT_EQ(client_received, server_data);
}

TEST_F(UnixIntegrationTest, AbstractNamespace) {
    unix_config config;
    config.socket_path = "test_abstract";
    config.abstract_namespace = true;

    unix_server abstract_server(config);
    unix_client abstract_client(config);

    ASSERT_TRUE(abstract_server.listen("test_abstract").has_value());

    std::thread client_thread([&abstract_client]() {
        std::this_thread::sleep_for(50ms);
        auto result = abstract_client.connect("test_abstract");
        EXPECT_TRUE(result.has_value());
    });

    auto accepted = abstract_server.accept();
    EXPECT_TRUE(accepted.has_value());

    client_thread.join();
    
    abstract_client.disconnect();
    abstract_server.stop();
}

#ifdef __linux__
TEST_F(UnixIntegrationTest, CredentialPassing) {
    ASSERT_TRUE(server.listen(test_socket).has_value());

    std::thread client_thread([this]() {
        std::this_thread::sleep_for(50ms);
        ASSERT_TRUE(client.connect(test_socket).has_value());
        
        auto result = client.send_credentials();
        EXPECT_TRUE(result.has_value());
    });

    auto accepted_client = server.accept();
    ASSERT_TRUE(accepted_client.has_value());

    auto creds = accepted_client->receive_credentials();
    EXPECT_TRUE(creds.has_value());
    
    if (creds) {
        EXPECT_EQ(creds->pid, getpid());
        EXPECT_EQ(creds->uid, getuid());
        EXPECT_EQ(creds->gid, getgid());
    }

    client_thread.join();
}
#endif

TEST_F(UnixIntegrationTest, SocketFilePermissions) {
    unix_config config;
    config.socket_path = "/tmp/test_perms.sock";
    config.socket_permissions = 0660;

    unix_server perm_server(config);
    ASSERT_TRUE(perm_server.listen().has_value());

    auto perms = fs::status(config.socket_path).permissions();
    auto expected = fs::perms::owner_read | fs::perms::owner_write | 
                   fs::perms::group_read | fs::perms::group_write;
    
    EXPECT_EQ(perms & expected, expected);

    perm_server.stop();
    fs::remove(config.socket_path);
}
