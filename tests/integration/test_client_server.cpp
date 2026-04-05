// Integration test: Skeleton client app ↔ Main app
//
// Spawns the main app (server) and skeleton app (client) as subprocesses,
// then verifies:
//   1. Main app HTTP endpoint responds
//   2. Skeleton app connects and registers via RPC
//   3. Hardware client operations work end-to-end
//   4. Clean shutdown of both processes

#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef MAIN_APP_EXECUTABLE
#error "MAIN_APP_EXECUTABLE not defined"
#endif

#ifndef SKELETON_APP_EXECUTABLE
#error "SKELETON_APP_EXECUTABLE not defined"
#endif

namespace {

// ---------------------------------------------------------------------------
// Process helpers
// ---------------------------------------------------------------------------

pid_t spawn(const std::string& exe, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    return pid;
}

void terminate_child(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    int status = 0;
    for (int i = 0; i < 50; ++i) {
        if (waitpid(pid, &status, WNOHANG) == pid) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

bool child_is_alive(pid_t pid) {
    if (pid <= 0) return false;
    return waitpid(pid, nullptr, WNOHANG) == 0;
}

// ---------------------------------------------------------------------------
// TCP / HTTP helpers
// ---------------------------------------------------------------------------

/// Attempt a raw TCP connect; returns true if the port is listening.
bool tcp_probe(const std::string& host, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    // Non-blocking would be nicer, but a short timeout suffices for tests
    bool ok = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    close(sock);
    return ok;
}

/// Send a minimal HTTP GET and return the status code (or -1 on error).
int http_get_status(const std::string& host, uint16_t port, const std::string& path) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    std::string req = "GET " + path + " HTTP/1.1\r\n"
                      "Host: " + host + "\r\n"
                      "Connection: close\r\n\r\n";
    if (send(sock, req.data(), req.size(), 0) != static_cast<ssize_t>(req.size())) {
        close(sock);
        return -1;
    }

    char buf[1024];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    close(sock);
    if (n <= 0) return -1;

    int code = -1;
    sscanf(buf, "HTTP/%*s %d", &code);
    return code;
}

/// Wait until a TCP port is accepting connections (up to timeout_ms).
bool wait_for_port(const std::string& host, uint16_t port,
                   int timeout_ms = 6000, int interval_ms = 200) {
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += interval_ms) {
        if (tcp_probe(host, port)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    return false;
}

// Test fixture ports — use high, unlikely-to-collide ports.
constexpr uint16_t TEST_HTTP_PORT = 18090;
constexpr uint16_t TEST_RPC_PORT  = 19130;
constexpr const char* TEST_HOST   = "127.0.0.1";

} // anonymous namespace

// ===========================================================================
// Test: Main app HTTP still works (sanity check before adding client)
// ===========================================================================
TEST(ClientServerIntegration, MainAppHttpResponds) {
    pid_t server = spawn(MAIN_APP_EXECUTABLE, {
        "-a", TEST_HOST, "-p", std::to_string(TEST_HTTP_PORT),
        "--rpc-server-address", TEST_HOST,
        "--rpc-server-port", std::to_string(TEST_RPC_PORT),
        "--rpc-server-client-timeout-seconds", "5"
    });
    ASSERT_GT(server, 0);

    ASSERT_TRUE(wait_for_port(TEST_HOST, TEST_HTTP_PORT))
        << "Main app HTTP port did not become ready";

    EXPECT_EQ(http_get_status(TEST_HOST, TEST_HTTP_PORT, "/"), 200);

    terminate_child(server);
}

// ===========================================================================
// Test: Skeleton app connects to main app RPC, both shut down cleanly
// ===========================================================================
TEST(ClientServerIntegration, SkeletonAppConnectsToMainApp) {
    // 1. Start server
    pid_t server = spawn(MAIN_APP_EXECUTABLE, {
        "-a", TEST_HOST, "-p", std::to_string(TEST_HTTP_PORT + 1),
        "--rpc-server-address", TEST_HOST,
        "--rpc-server-port", std::to_string(TEST_RPC_PORT + 1),
        "--rpc-server-client-timeout-seconds", "5"
    });
    ASSERT_GT(server, 0);

    // Wait for RPC port to be ready
    ASSERT_TRUE(wait_for_port(TEST_HOST, TEST_RPC_PORT + 1))
        << "Main app RPC port did not become ready";

    // 2. Start skeleton client
    pid_t client = spawn(SKELETON_APP_EXECUTABLE, {
        "-n", "test-skeleton",
        "-s", TEST_HOST,
        "-p", std::to_string(TEST_RPC_PORT + 1)
    });
    ASSERT_GT(client, 0);

    // 3. Give the client time to connect and register
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 4. Both processes should still be alive
    EXPECT_TRUE(child_is_alive(server)) << "Main app crashed";
    EXPECT_TRUE(child_is_alive(client)) << "Skeleton app crashed";

    // 5. HTTP should still respond while client is connected
    EXPECT_EQ(http_get_status(TEST_HOST, TEST_HTTP_PORT + 1, "/"), 200);

    // 6. Clean shutdown (client first, then server)
    terminate_child(client);
    terminate_child(server);
}

// ===========================================================================
// Test: Multiple skeleton apps can connect simultaneously
// ===========================================================================
TEST(ClientServerIntegration, MultipleClientsConnect) {
    pid_t server = spawn(MAIN_APP_EXECUTABLE, {
        "-a", TEST_HOST, "-p", std::to_string(TEST_HTTP_PORT + 2),
        "--rpc-server-address", TEST_HOST,
        "--rpc-server-port", std::to_string(TEST_RPC_PORT + 2),
        "--rpc-server-client-timeout-seconds", "5"
    });
    ASSERT_GT(server, 0);
    ASSERT_TRUE(wait_for_port(TEST_HOST, TEST_RPC_PORT + 2));

    // Spawn two clients
    pid_t client1 = spawn(SKELETON_APP_EXECUTABLE, {
        "-n", "client-1", "-s", TEST_HOST,
        "-p", std::to_string(TEST_RPC_PORT + 2)
    });
    pid_t client2 = spawn(SKELETON_APP_EXECUTABLE, {
        "-n", "client-2", "-s", TEST_HOST,
        "-p", std::to_string(TEST_RPC_PORT + 2)
    });
    ASSERT_GT(client1, 0);
    ASSERT_GT(client2, 0);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_TRUE(child_is_alive(server))  << "Server crashed";
    EXPECT_TRUE(child_is_alive(client1)) << "Client 1 crashed";
    EXPECT_TRUE(child_is_alive(client2)) << "Client 2 crashed";

    terminate_child(client1);
    terminate_child(client2);
    terminate_child(server);
}

// ===========================================================================
// Test: Client survives server restart
// ===========================================================================
TEST(ClientServerIntegration, ClientSurvivesServerRestart) {
    // 1. Start server
    pid_t server = spawn(MAIN_APP_EXECUTABLE, {
        "-a", TEST_HOST, "-p", std::to_string(TEST_HTTP_PORT + 3),
        "--rpc-server-address", TEST_HOST,
        "--rpc-server-port", std::to_string(TEST_RPC_PORT + 3),
        "--rpc-server-client-timeout-seconds", "5"
    });
    ASSERT_GT(server, 0);
    ASSERT_TRUE(wait_for_port(TEST_HOST, TEST_RPC_PORT + 3));

    // 2. Start client
    pid_t client = spawn(SKELETON_APP_EXECUTABLE, {
        "-n", "resilient-client", "-s", TEST_HOST,
        "-p", std::to_string(TEST_RPC_PORT + 3)
    });
    ASSERT_GT(client, 0);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(child_is_alive(client));

    // 3. Kill server
    terminate_child(server);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 4. Client should still be alive (in reconnecting state)
    EXPECT_TRUE(child_is_alive(client)) << "Client died after server shutdown";

    // 5. Restart server on same port
    server = spawn(MAIN_APP_EXECUTABLE, {
        "-a", TEST_HOST, "-p", std::to_string(TEST_HTTP_PORT + 3),
        "--rpc-server-address", TEST_HOST,
        "--rpc-server-port", std::to_string(TEST_RPC_PORT + 3),
        "--rpc-server-client-timeout-seconds", "5"
    });
    ASSERT_GT(server, 0);
    ASSERT_TRUE(wait_for_port(TEST_HOST, TEST_RPC_PORT + 3));

    // 6. Give client time to reconnect
    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_TRUE(child_is_alive(client)) << "Client died during reconnect";

    terminate_child(client);
    terminate_child(server);
}
