#include "services/http_service.hpp"
#include <protoflow/messages.hpp>
#include <protoflow/logging/macros.hpp>
#include <sstream>



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

    // Register built-in endpoints
    register_endpoint("GET", "/", [this](const HttpRequest& req) {
        return serve_home(req);
    });

    register_endpoint("GET", "/api/state", [this](const HttpRequest& req) {
        return serve_state_api(req);
    });

    register_endpoint("GET", "/app/*", [this](const HttpRequest& req) {
        return serve_app_endpoint(req);
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

    HttpResponse response;
    response.status_code = 404;
    response.set_html("<html><body><h1>404 Not Found</h1></body></html>");
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
//  Built-in page handlers
// ────────────────────────────────────────────────────────────────

HttpResponse HTTPService::serve_home(const HttpRequest& request) {
    (void)request;

    std::string html = R"(<!doctype html>
<html>
    <head>
        <meta charset="utf-8" />
        <title>Protoflow Main App</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }
            .content { margin-top: 20px; }
        </style>
    </head>
    <body>
        <div class="content">
            <h1>Protoflow Main Application</h1>
            <p>Welcome to the Protoflow system.</p>
            <h2>Registered Applications</h2>
            <div id="apps">Loading applications...</div>
        </div>
    </body>
</html>)";

    HttpResponse response;
    response.set_html(html);
    return response;
}

HttpResponse HTTPService::serve_app_endpoint(const HttpRequest& request) {
    (void)request;

    HttpResponse response;
    response.status_code = 404;
    response.set_html("<html><body><h1>404 Not Found</h1><p>App or endpoint not found</p></body></html>");
    return response;
}

HttpResponse HTTPService::serve_state_api(const HttpRequest& request) {
    HttpResponse response;

    if (accepts_json(request) || !accepts_html(request)) {
        response.set_json(R"({"error": "Not yet implemented - requires message-based communication"})");
    } else {
        std::string html = R"(<!doctype html>
<html>
  <head><meta charset="utf-8"/><title>System State</title></head>
  <body>
    <h1>System State</h1>
    <pre>Not yet implemented - requires message-based communication</pre>
  </body>
</html>)";
        response.set_html(html);
    }

    return response;
}

// ────────────────────────────────────────────────────────────────
//  App registration / unregistration handlers
// ────────────────────────────────────────────────────────────────

void HTTPService::handle_app_registration(const AppRegistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App registered: " << event.app_name
                      << " with " << event.endpoints.size() << " endpoints");

    for (const auto& endpoint : event.endpoints) {
        std::string path = "/app/" + event.app_name + "/" + endpoint;

        PROTOFLOW_LOG_INFO(*this, "  Registering endpoint: GET " << path);

        register_endpoint("GET", path, [this, app_name = event.app_name, endpoint](const HttpRequest& req) {
            (void)req;
            HttpResponse response;
            response.status_code = 503;
            response.set_html("<html><body><h1>Service Unavailable</h1>"
                            "<p>Endpoint forwarding not yet implemented</p></body></html>");
            return response;
        });
    }

    registered_apps_[event.app_name] = event.endpoints;
}

void HTTPService::handle_app_unregistration(const AppUnregistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App unregistered: " << event.app_name);

    auto it = registered_apps_.find(event.app_name);
    if (it != registered_apps_.end()) {
        for (const auto& endpoint : it->second) {
            std::string path = "/app/" + event.app_name + "/" + endpoint;
            std::string key = "GET " + path;
            auto eit = endpoints_.find(key);
            if (eit != endpoints_.end()) {
                PROTOFLOW_LOG_INFO(*this, "  Unregistering endpoint: " << key);
                endpoints_.erase(eit);
            }
        }
        registered_apps_.erase(it);
    }
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
