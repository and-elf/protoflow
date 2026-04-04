#include "services/http_service.hpp"
#include "services/handlers/status_handler.hpp"
#include "services/handlers/state_handler.hpp"
#include "services/handlers/home_handler.hpp"
#include "services/handlers/app_handler.hpp"
#include "services/handlers/static_handler.hpp"
#include <protoflow/messages.hpp>
#include <protoflow/logging/macros.hpp>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>



namespace protoflow::mainapp {

// ────────────────────────────────────────────────────────────────
//  Construction
// ────────────────────────────────────────────────────────────────

HTTPService::HTTPService()
    : Service({HttpMessageTypes::HttpRequest})   // subscribe to request events
{}

// ────────────────────────────────────────────────────────────────
//  Lifecycle
// ────────────────────────────────────────────────────────────────

void HTTPService::start() {
    PROTOFLOW_LOG_INFO(*this, "Starting HTTPService (event-driven)");

    running_ = true;

    // Set up static files directory
    // Try multiple possible locations for the static directory
    std::vector<std::string> static_paths = {
        "./apps/main-app/static",
        "../apps/main-app/static",
        "../../apps/main-app/static",
        "../../../apps/main-app/static",
        "/home/andreas/work/protoflow/apps/main-app/static"
    };
    
    for (const auto& path : static_paths) {
        try {
            if (std::filesystem::exists(path)) {
                static_dir_ = std::filesystem::absolute(path).string();
                PROTOFLOW_LOG_INFO(*this, "Found static directory: " << static_dir_);
                break;
            }
        } catch (const std::exception& e) {
            PROTOFLOW_LOG_DEBUG(*this, "Error checking path " << path << ": " << e.what());
        }
    }
    
    if (static_dir_.empty()) {
        PROTOFLOW_LOG_WARN(*this, "Static directory not found, static files will not be served");
    }

    // Register built-in endpoints
    using namespace handlers;

    register_endpoint("GET", "/", [this](const HttpRequest& req) {
        return handle_home(*this, req);
    });

    register_endpoint("GET", "/api/state", [this](const HttpRequest& req) {
        return handle_state_api(*this, req);
    });

    register_endpoint("GET", "/status", [this](const HttpRequest& req) {
        return handle_status(*this, req);
    });

    register_endpoint("GET", "/api/status", [this](const HttpRequest& req) {
        return handle_status_async(*this, req);
    });

    register_endpoint("GET", "/static/*", [this](const HttpRequest& req) {
        return handle_static(*this, req);
    });

    register_endpoint("GET", "/app/*", [this](const HttpRequest& req) {
        return handle_app_endpoint(*this, req);
    });

    PROTOFLOW_LOG_INFO(*this, "Registered " << endpoints_.size() << " endpoints");
    PROTOFLOW_LOG_INFO(*this, "HTTPService started");
}

void HTTPService::stop() {
    PROTOFLOW_LOG_INFO(*this, "Stopping HTTPService");
    running_ = false;
}

void HTTPService::poll() {
    if (!running_) return;

    // Check for timed-out pending requests
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> timed_out_ids;

    for (const auto& [req_id, pending] : pending_requests_) {
        if (now - pending.timestamp >= REQUEST_TIMEOUT)
            timed_out_ids.push_back(req_id);
    }

    for (uint64_t req_id : timed_out_ids) {
        auto it = pending_requests_.find(req_id);
        if (it != pending_requests_.end()) {
            auto elapsed = now - it->second.timestamp;
            PROTOFLOW_LOG_WARN(*this, "Request #" << req_id
                              << " timed out after "
                              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                              << "ms");

            HttpResponse timeout_response;
            timeout_response.status_code = 504;
            timeout_response.set_json(R"({"error": "Request timed out waiting for service response"})");
            pending_responses_.push_back(std::move(timeout_response));

            pending_requests_.erase(it);
        }
    }

    // Let the base class drain inbound queue → handle()
    service::Service::poll();
}

// ────────────────────────────────────────────────────────────────
//  Endpoint registry
// ────────────────────────────────────────────────────────────────

void HTTPService::register_endpoint(const std::string& method,
                                    const std::string& path,
                                    HttpHandler handler) {
    std::string key = method + " " + path;
    endpoints_[key] = std::move(handler);
}

HttpResponse HTTPService::handle_request(const HttpRequest& request) {
    std::string key = request.method + " " + request.path;

    // Exact match
    auto it = endpoints_.find(key);
    if (it != endpoints_.end()) return it->second(request);

    // Wildcard match
    for (const auto& [pattern, handler] : endpoints_) {
        if (pattern.find('*') != std::string::npos) {
            std::string prefix = pattern.substr(0, pattern.find('*'));
            std::string method_prefix = request.method + " " + prefix;
            if (key.starts_with(method_prefix)) return handler(request);
        }
    }

    using namespace html;
    HttpResponse response;
    response.status_code = 404;
    response.set_html(to_html(
        card(
            h3(text("404 \u2014 Not Found")),
            p(text("The requested resource could not be found."))
        )
    ));
    return response;
}

// ────────────────────────────────────────────────────────────────
//  Event handling (message bus)
// ────────────────────────────────────────────────────────────────

void HTTPService::handle(service::Message&& msg) {
    if (msg.type() != HttpMessageTypes::HttpRequest) {
        PROTOFLOW_LOG_DEBUG(*this, "Ignoring unexpected message type " << msg.type());
        return;
    }

    auto ev = HttpRequestEvent::deserialize(msg.bytes());
    if (!ev) {
        PROTOFLOW_LOG_WARN(*this, "Failed to deserialize HttpRequestEvent");
        return;
    }

    // Convert event → internal HttpRequest
    HttpRequest hreq;
    hreq.method  = std::move(ev->method);
    hreq.path    = std::move(ev->path);
    hreq.headers = std::move(ev->headers);
    hreq.body    = std::move(ev->body);

    // Route through endpoint handlers
    HttpResponse hres = handle_request(hreq);

    // Convert internal HttpResponse → HttpResponseEvent → Message
    HttpResponseEvent resp_ev;
    resp_ev.connection_id = ev->connection_id;
    resp_ev.status_code   = hres.status_code;
    resp_ev.headers       = std::move(hres.headers);
    resp_ev.body          = std::move(hres.body);

    auto payload = resp_ev.serialize();
    auto resp_msg = messaging::MessageBuilder{}
        .from(service_id_)
        .type(HttpMessageTypes::HttpResponse)
        .payload(std::move(payload))
        .build();

    write(std::move(resp_msg));
}

std::vector<service::Message> HTTPService::generate_outbound() {
    // Responses are queued via write() in handle(), nothing extra needed.
    return {};
}

// ────────────────────────────────────────────────────────────────
//  Request handlers
// ────────────────────────────────────────────────────────────────
//
// All HTTP endpoint handlers have been moved to separate files in the
// handlers/ directory for better organization and maintainability.
// They now use nlohmann::json instead of raw string concatenation.
//
// Moved handlers:
//  - handle_home() → services/handlers/home_handler.cpp
//  - handle_app_endpoint(), handle_fragment() → services/handlers/app_handler.cpp
//  - handle_state_api() → services/handlers/state_handler.cpp
//  - handle_status(), handle_status_async() → services/handlers/status_handler.cpp
//  - handle_static(), read_static_file() → services/handlers/static_handler.cpp

// ────────────────────────────────────────────────────────────────
//  App registration / unregistration handlers
// ────────────────────────────────────────────────────────────────

void HTTPService::handle_app_registration(const AppRegistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App registered: " << event.app_name
                      << " with " << event.endpoints.size() << " endpoints");

    // Track the app and its endpoints; the wildcard /app/* handler
    // (serve_app_endpoint) routes fragment and overview requests.
    registered_apps_[event.app_name] = event.endpoints;
    
    // Try to extract HTTP listener URL from the first endpoint if available
    // Endpoints format can be: "http://localhost:8080", "/api/data", etc.
    std::string http_listener;
    for (const auto& ep : event.endpoints) {
        if (ep.find("http://") == 0 || ep.find("https://") == 0) {
            // Extract base URL (scheme + host + port)
            size_t slash_pos = ep.find('/', 8);  // Skip "https://"
            if (slash_pos != std::string::npos) {
                http_listener = ep.substr(0, slash_pos);
            } else {
                http_listener = ep;
            }
            break;
        }
    }
    
    // If no HTTP endpoint found but we have endpoints, assume localhost
    // This is a heuristic - in production, apps would register their HTTP listener explicitly
    if (http_listener.empty() && !event.endpoints.empty()) {
        // Default assumption: apps likely listen on localhost with sequential ports
        // starting from 8081 (8080 is main app)
        PROTOFLOW_LOG_WARN(*this, "App " << event.app_name 
                          << " did not register HTTP listener URL");
        http_listener = "http://localhost:8081";  // Placeholder
    }
    
    registered_apps_info_[event.app_name] = {event.endpoints, http_listener};
}

void HTTPService::handle_app_unregistration(const AppUnregistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App unregistered: " << event.app_name);
    registered_apps_.erase(event.app_name);
    registered_apps_info_.erase(event.app_name);
}

// ────────────────────────────────────────────────────────────────
//  Content negotiation helpers
// ────────────────────────────────────────────────────────────────

bool HTTPService::accepts_html(const HttpRequest& request) const {
    auto it = request.headers.find("Accept");
    if (it != request.headers.end())
        return it->second.find("text/html") != std::string::npos;
    return false;
}

bool HTTPService::accepts_json(const HttpRequest& request) const {
    auto it = request.headers.find("Accept");
    if (it != request.headers.end())
        return it->second.find("application/json") != std::string::npos;
    return false;
}


} // namespace protoflow::mainapp
