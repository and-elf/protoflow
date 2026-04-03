// Black-box integration test: start main app as a subprocess, then make
// raw HTTP requests to verify it responds (HttpListenerService → HTTPService
// event pipeline).

#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// MAIN_APP_EXECUTABLE is provided by CMake as the absolute path to the
// built main-app executable.
#ifndef MAIN_APP_EXECUTABLE
#error "MAIN_APP_EXECUTABLE not defined"
#endif

static pid_t spawn_main_app(const std::string& exe, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child: prepare argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        // If execv returns, fail fast
        _exit(127);
    }
    return pid;
}

static void terminate_child(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    int status = 0;
    // Wait with timeout
    for (int i=0;i<50;i++) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // Force kill
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

static int http_get_status(const std::string& host, uint16_t port, const std::string& path) {
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

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nAccept: text/html\r\nConnection: close\r\n\r\n";
    ssize_t sent = send(sock, req.data(), req.size(), 0);
    if (sent != static_cast<ssize_t>(req.size())) { close(sock); return -1; }

    // Read first line
    std::string line;
    char buf[1024];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) { close(sock); return -1; }
    std::string resp(buf, static_cast<size_t>(n));
    close(sock);

    // Parse status code from first line: HTTP/1.1 200 OK
    auto pos = resp.find('\n');
    std::string first = (pos == std::string::npos) ? resp : resp.substr(0, pos);
    int code = -1;
    if (sscanf(first.c_str(), "HTTP/%*s %d", &code) == 1) return code;
    return -1;
}

TEST(MainAppBlackbox, HttpRespondsToGetRoot) {
    
    // Launch main app with HTTP on 18080 and RPC on 19123 (non-default ports)
    std::vector<std::string> args = {
        "-a", "127.0.0.1", "-p", "18080",
        "--rpc-server-address", "127.0.0.1", "--rpc-server-port", "19123"
    };
    pid_t pid = spawn_main_app(MAIN_APP_EXECUTABLE, args);
    ASSERT_GT(pid, 0);

    // Retry connecting until the listener is ready (up to ~6 s)
    int status = -1;
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        status = http_get_status("127.0.0.1", 18080, "/");
        if (status != -1) break;
    }

    EXPECT_EQ(status, 200) << "Expected HTTP 200 for GET /";

    // Also check a non-existing path returns 404
    int status_404 = http_get_status("127.0.0.1", 18080, "/does-not-exist");
    EXPECT_EQ(status_404, 404) << "Expected HTTP 404 for unknown path";

    terminate_child(pid);
}
