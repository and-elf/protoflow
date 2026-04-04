#pragma once

#include <protoflow/service.hpp>
#include <protoflow/http.hpp>
#include <protoflow/html.hpp>
#include <protoflow/messages.hpp>
#include <string>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
#include <optional>
#include <chrono>

namespace protoflow::mainapp {

/// HTTP request context (internal representation used by endpoint handlers)
struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;
};

/// HTTP response (internal representation produced by endpoint handlers)
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

/// Event-driven HTTP routing service.
/// Receives HttpRequestEvent messages (from HttpListenerService),
/// dispatches them through registered endpoint handlers, and emits
/// HttpResponseEvent messages back.
/// Does NOT own any network sockets.
class HTTPService : public service::Service {
public:
    /// Default constructor – subscribes to HttpRequest events.
    HTTPService();

    void start() override;
    void stop() override;
    void poll() override;

    /// Register HTTP endpoint handler
    void register_endpoint(const std::string& method, 
                          const std::string& path, 
                          HttpHandler handler);

    // Getters for handlers to access data members
    const std::unordered_map<std::string, std::vector<std::string>>& get_registered_apps() const {
        return registered_apps_;
    }

    struct AppInfo {
        std::vector<std::string> endpoints;
        std::string http_listener;
    };

    const std::unordered_map<std::string, AppInfo>& get_registered_apps_info() const {
        return registered_apps_info_;
    }

    const std::string& get_static_dir() const {
        return static_dir_;
    }

protected:
    void handle(messaging::Message&& msg) override;
    std::vector<messaging::Message> generate_outbound() override;

private:
    friend class HTTPServiceTest;  // Allow tests to access private members
    
    /// Handle incoming HTTP request (routing only)
    HttpResponse handle_request(const HttpRequest& request);

    /// Content negotiation helpers
    bool accepts_html(const HttpRequest& request) const;
    bool accepts_json(const HttpRequest& request) const;

    /// Message handlers for app registration lifecycle
    void handle_app_registration(const AppRegistrationEvent& event);
    void handle_app_unregistration(const AppUnregistrationEvent& event);

    /// Context for tracking pending HTTP requests waiting for service responses
    struct PendingRequest {
        uint64_t request_id;  // Unique ID for matching responses
        HttpRequest request;
        enum class Type { Navigation, State, Fragment, Logs } type;
        std::chrono::steady_clock::time_point timestamp;
    };

    bool running_ = false;
    std::string static_dir_;  // Path to static files directory
    
    // Endpoint registry
    std::unordered_map<std::string, HttpHandler> endpoints_;
    
    // Track dynamically registered apps and their endpoints for cleanup
    // Maps app_name -> {endpoints_list, http_listener_address}
    std::unordered_map<std::string, AppInfo> registered_apps_info_;
    
    // Legacy map for backward compatibility
    std::unordered_map<std::string, std::vector<std::string>> registered_apps_;
    
    // Pending requests waiting for service responses (keyed by request_id)
    std::map<uint64_t, PendingRequest> pending_requests_;
    std::vector<HttpResponse> pending_responses_;
    uint64_t next_request_id_ = 1;
    
    static constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(5);
};

} // namespace protoflow::mainapp
