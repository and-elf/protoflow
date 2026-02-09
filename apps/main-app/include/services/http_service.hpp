#pragma once

#include <protoflow/service.hpp>
#include <protoflow/http.hpp>
#include <protoflow/html.hpp>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

namespace protoflow::mainapp {

/// HTTP request context
struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;
};

/// HTTP response
struct HttpResponse {
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;
    
    void set_html(const std::string& html) {
        headers["Content-Type"] = "text/html; charset=utf-8";
        body.resize(html.size());
        std::memcpy(body.data(), html.data(), html.size());
    }
    
    void set_json(const std::string& json) {
        headers["Content-Type"] = "application/json; charset=utf-8";
        body.resize(json.size());
        std::memcpy(body.data(), json.data(), json.size());
    }
};

/// HTTP endpoint handler type
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/// Service providing HTTP server functionality
/// Aggregates UI fragments and serves JSON API
class HTTPService : public service::Service {
public:
    HTTPService(const std::string& listen_address, uint16_t port);
    ~HTTPService() override;

    void start() override;
    void stop() override;
    void poll() override;

    /// Register HTTP endpoint handler
    void register_endpoint(const std::string& method, 
                          const std::string& path, 
                          HttpHandler handler);

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    /// Handle incoming HTTP request
    HttpResponse handle_request(const HttpRequest& request);

    /// Serve home page with navigation
    HttpResponse serve_home(const HttpRequest& request);

    /// Serve app endpoint (proxy to registered app)
    HttpResponse serve_app_endpoint(const HttpRequest& request);

    /// Serve aggregated state API
    HttpResponse serve_state_api(const HttpRequest& request);

    /// Check if client accepts HTML
    bool accepts_html(const HttpRequest& request) const;

    /// Check if client accepts JSON
    bool accepts_json(const HttpRequest& request) const;

    std::string listen_address_;
    uint16_t port_;
    bool running_ = false;
    
    // Endpoint registry
    std::unordered_map<std::string, HttpHandler> endpoints_;
    
    // HTTP server implementation (would be actual HTTP server)
    // For now, this is a placeholder
    struct HttpServerImpl;
    std::unique_ptr<HttpServerImpl> server_impl_;
};

} // namespace protoflow::mainapp
