#include "services/http_listener_service.hpp"
#include <protoflow/logging/macros.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <cerrno>
#include <cstring>
#include <sstream>

namespace protoflow::mainapp {

HttpListenerService::HttpListenerService(std::string listen_address, uint16_t port)
    : Service({HttpMessageTypes::HttpResponse})   // subscribe to response events
    , listen_address_(std::move(listen_address))
    , port_(port)
{}

HttpListenerService::~HttpListenerService() {
    if (listen_fd_ >= 0) ::close(listen_fd_);
    for (auto& [id, fd] : pending_clients_) {
        if (fd >= 0) ::close(fd);
    }
}

void HttpListenerService::start() {
    PROTOFLOW_LOG_INFO(*this, "Starting HTTP listener on "
                       << listen_address_ << ":" << port_);

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        PROTOFLOW_LOG_WARN(*this, "Failed to create listen socket");
        return;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, listen_address_.c_str(), &addr.sin_addr) <= 0) {
        PROTOFLOW_LOG_WARN(*this, "Invalid listen address: " << listen_address_);
        ::close(listen_fd_); listen_fd_ = -1;
        return;
    }

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        PROTOFLOW_LOG_WARN(*this, "bind() failed: " << strerror(errno));
        ::close(listen_fd_); listen_fd_ = -1;
        return;
    }

    if (listen(listen_fd_, 16) < 0) {
        PROTOFLOW_LOG_WARN(*this, "listen() failed: " << strerror(errno));
        ::close(listen_fd_); listen_fd_ = -1;
        return;
    }

    // Non-blocking accept
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    running_ = true;
    PROTOFLOW_LOG_INFO(*this, "HTTP listener started");
}

void HttpListenerService::stop() {
    PROTOFLOW_LOG_INFO(*this, "Stopping HTTP listener");
    running_ = false;
    if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }
    for (auto& [id, fd] : pending_clients_) {
        if (fd >= 0) ::close(fd);
    }
    pending_clients_.clear();
}

void HttpListenerService::poll() {
    if (!running_) return;

    // 1. Accept new connections and produce HttpRequestEvent messages
    accept_connections();

    // 2. Deliver any pending outbound request messages via the normal
    //    generate_outbound path (handled by Service base class)
    // 3. Process one inbound response message (from HTTPService)
    service::Service::poll();
}

// ───────────────── Handle incoming response events ─────────────────

void HttpListenerService::handle(messaging::Message&& msg) {
    if (msg.type() != HttpMessageTypes::HttpResponse) return;

    auto ev = HttpResponseEvent::deserialize(msg.bytes());
    if (!ev) {
        PROTOFLOW_LOG_WARN(*this, "Failed to deserialize HttpResponseEvent");
        return;
    }

    auto it = pending_clients_.find(ev->connection_id);
    if (it == pending_clients_.end()) {
        PROTOFLOW_LOG_WARN(*this, "No pending client for connection "
                           << ev->connection_id);
        return;
    }

    send_response(it->second, *ev);
    ::shutdown(it->second, SHUT_RDWR);
    ::close(it->second);
    pending_clients_.erase(it);
}

std::vector<messaging::Message> HttpListenerService::generate_outbound() {
    return std::move(outbound_requests_);
}

// ───────────────── TCP accept + HTTP parse ─────────────────

void HttpListenerService::accept_connections() {
    if (listen_fd_ < 0) return;

    while (true) {
        int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0) break;   // EWOULDBLOCK or error

        // Wait briefly for the request header
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(client, &rfds);
        timeval tv{};
        tv.tv_sec  = 0;
        tv.tv_usec = 200000;   // 200 ms
        int rv = select(client + 1, &rfds, nullptr, nullptr, &tv);

        std::string raw;
        if (rv > 0 && FD_ISSET(client, &rfds)) {
            char buf[4096];
            ssize_t r = recv(client, buf, sizeof(buf), 0);
            if (r > 0) raw.assign(buf, static_cast<size_t>(r));
        }

        if (raw.empty()) {
            ::close(client);
            continue;
        }

        // ---- Minimal HTTP/1.x parsing ----
        HttpRequestEvent ev;
        ev.connection_id = next_connection_id_++;

        // First line: METHOD PATH HTTP/x.y
        auto first_end = raw.find("\r\n");
        std::string first_line = (first_end == std::string::npos) ? raw : raw.substr(0, first_end);
        {
            std::istringstream iss(first_line);
            std::string version;
            iss >> ev.method >> ev.path >> version;
        }

        // Headers
        size_t pos = (first_end == std::string::npos) ? raw.size() : first_end + 2;
        while (pos < raw.size()) {
            auto line_end = raw.find("\r\n", pos);
            if (line_end == std::string::npos || line_end == pos) break;
            std::string line = raw.substr(pos, line_end - pos);
            pos = line_end + 2;
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                ev.headers[std::move(key)] = std::move(val);
            }
        }

        // Store client fd for later response
        pending_clients_[ev.connection_id] = client;

        // Serialize and queue as a message
        auto payload = ev.serialize();
        auto message = messaging::MessageBuilder{}
            .from(service_id_)
            .type(HttpMessageTypes::HttpRequest)
            .payload(std::move(payload))
            .build();
        outbound_requests_.push_back(std::move(message));
    }
}

// ───────────────── Write raw HTTP response ─────────────────

void HttpListenerService::send_response(int fd, const HttpResponseEvent& ev) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << ev.status_code << " OK\r\n";

    // Ensure Content-Length
    auto hdrs = ev.headers;
    if (hdrs.find("Content-Length") == hdrs.end()) {
        hdrs["Content-Length"] = std::to_string(ev.body.size());
    }
    for (const auto& [k, v] : hdrs) oss << k << ": " << v << "\r\n";
    oss << "\r\n";

    std::string header_bytes = oss.str();
    send(fd, header_bytes.data(), header_bytes.size(), 0);
    if (!ev.body.empty()) {
        send(fd, reinterpret_cast<const char*>(ev.body.data()), ev.body.size(), 0);
    }
}

} // namespace protoflow::mainapp
