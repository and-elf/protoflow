#pragma once

#include <protoflow/service.hpp>
#include <protoflow/messages.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace protoflow::mainapp {

/// TCP listener service for HTTP.
/// Owns the listening socket, accepts connections, parses raw HTTP into
/// HttpRequestEvent messages, and writes HttpResponseEvent data back
/// to the client socket.
///
/// The HTTPService is the consumer of request events and producer of
/// response events – this service only handles I/O.
class HttpListenerService : public service::Service {
public:
    HttpListenerService(std::string listen_address, uint16_t port);
    ~HttpListenerService() override;

    void start() override;
    void stop() override;
    void poll() override;

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    /// Accept any pending TCP connections, read HTTP, emit request events.
    void accept_connections();

    /// Write a raw HTTP response to a client socket and close it.
    void send_response(int fd, const HttpResponseEvent& ev);

    std::string listen_address_;
    uint16_t port_;
    int listen_fd_ = -1;
    bool running_ = false;

    /// Open client sockets waiting for a response, keyed by connection id.
    std::unordered_map<uint64_t, int> pending_clients_;
    uint64_t next_connection_id_ = 1;

    /// Outbound request messages produced in this poll cycle (to be routed
    /// by the runtime to HTTPService).
    std::vector<messaging::Message> outbound_requests_;
};

} // namespace protoflow::mainapp
