#include "services/http_service.hpp"
#include "services/app_registration_service.hpp"
#include <protoflow/logging/macros.hpp>
#include <sstream>

namespace protoflow::mainapp {

// Placeholder HTTP server implementation
struct HTTPService::HttpServerImpl {
    // In a full implementation, this would use an actual HTTP server library
    // like cpp-httplib, Boost.Beast, or a custom TCP-based implementation
};

HTTPService::HTTPService(const std::string& listen_address, uint16_t port)
    : listen_address_(listen_address)
    , port_(port)
{
}

HTTPService::~HTTPService() = default;

void HTTPService::start() {
    PROTOFLOW_LOG_INFO(*this, "Starting HTTP server on " 
                       << listen_address_ << ":" << port_);
    
    running_ = true;
    
    // Register built-in endpoints
    register_endpoint("GET", "/", [this](const HttpRequest& req) {
        return serve_home(req);
    });
    
    register_endpoint("GET", "/api/state", [this](const HttpRequest& req) {
        return serve_state_api(req);
    });
    
    // Register wildcard app endpoint handler
    register_endpoint("GET", "/app/*", [this](const HttpRequest& req) {
        return serve_app_endpoint(req);
    });
    
    PROTOFLOW_LOG_INFO(*this, "Registered " << endpoints_.size() << " endpoints");
    PROTOFLOW_LOG_INFO(*this, "HTTP server started");
}

void HTTPService::stop() {
    PROTOFLOW_LOG_INFO(*this, "Stopping HTTP server");
    running_ = false;
}

void HTTPService::poll() {
    if (!running_) return;
    
    // In a full implementation:
    // 1. Accept incoming connections
    // 2. Parse HTTP requests
    // 3. Route to appropriate handlers
    // 4. Send HTTP responses
    
    // For now, this is a placeholder that would integrate with an actual HTTP server
    
    service::Service::poll();
}

void HTTPService::register_endpoint(const std::string& method, 
                                    const std::string& path, 
                                    HttpHandler handler) {
    std::string key = method + " " + path;
    endpoints_[key] = std::move(handler);
}

void HTTPService::set_app_registration_service(AppRegistrationService* service) {
    app_registration_service_ = service;
}

HttpResponse HTTPService::handle_request(const HttpRequest& request) {
    std::string key = request.method + " " + request.path;
    
    // Try exact match first
    auto it = endpoints_.find(key);
    if (it != endpoints_.end()) {
        return it->second(request);
    }
    
    // Try wildcard match
    for (const auto& [pattern, handler] : endpoints_) {
        if (pattern.find('*') != std::string::npos) {
            // Simple wildcard matching
            std::string prefix = pattern.substr(0, pattern.find('*'));
            std::string method_prefix = request.method + " " + prefix;
            
            if (key.starts_with(method_prefix)) {
                return handler(request);
            }
        }
    }
    
    // Not found
    HttpResponse response;
    response.status_code = 404;
    response.set_html("<html><body><h1>404 Not Found</h1></body></html>");
    return response;
}

HttpResponse HTTPService::serve_home(const HttpRequest& request) {
    using namespace html;
    
    // Build home page with navigation
    auto nav = app_registration_service_ 
        ? app_registration_service_->render_navigation()
        : div(text("No navigation available"));
    
    auto page = html_doc(
        head(
            title(text("Protoflow Main App")),
            style(text(R"(
                body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }
                .navbar { background: #333; padding: 10px; }
                .navbar ul { list-style: none; margin: 0; padding: 0; }
                .navbar li { display: inline; margin-right: 20px; }
                .navbar a { color: white; text-decoration: none; }
                .navbar a:hover { text-decoration: underline; }
                .content { margin-top: 20px; }
            )"))
        ),
        body(
            std::move(nav),
            div(
                attr("class", "content"),
                h1(text("Protoflow Main Application")),
                p(text("Welcome to the Protoflow system.")),
                h2(text("Registered Applications")),
                div(
                    attr("id", "apps"),
                    text("Loading applications...")
                )
            )
        )
    );
    
    HttpResponse response;
    response.set_html(render(page));
    return response;
}

HttpResponse HTTPService::serve_app_endpoint(const HttpRequest& request) {
    // Parse app name and endpoint from path
    // Format: /app/{app_name}/{endpoint}
    
    std::string path = request.path;
    if (path.starts_with("/app/")) {
        path = path.substr(5); // Remove "/app/"
        
        auto slash_pos = path.find('/');
        if (slash_pos != std::string::npos) {
            std::string app_name = path.substr(0, slash_pos);
            std::string endpoint = path.substr(slash_pos);
            
            if (app_registration_service_) {
                auto fragment = app_registration_service_->proxy_fragment_request(
                    app_name, endpoint
                );
                
                if (fragment) {
                    using namespace html;
                    auto nav = app_registration_service_->render_navigation();
                    
                    auto page = html_doc(
                        head(title(text(app_name + " - " + endpoint))),
                        body(
                            std::move(nav),
                            div(
                                attr("class", "content"),
                                std::move(*fragment)
                            )
                        )
                    );
                    
                    HttpResponse response;
                    response.set_html(render(page));
                    return response;
                }
            }
        }
    }
    
    HttpResponse response;
    response.status_code = 404;
    response.set_html("<html><body><h1>404 Not Found</h1><p>App or endpoint not found</p></body></html>");
    return response;
}

HttpResponse HTTPService::serve_state_api(const HttpRequest& request) {
    HttpResponse response;
    
    if (accepts_json(request) || !accepts_html(request)) {
        // Return JSON
        std::string state_json = app_registration_service_
            ? app_registration_service_->aggregate_state_json()
            : R"({"error": "App registration service not available"})";
        
        response.set_json(state_json);
    } else {
        // Return HTML representation
        using namespace html;
        auto page = html_doc(
            head(title(text("System State"))),
            body(
                h1(text("System State")),
                pre(text(app_registration_service_
                    ? app_registration_service_->aggregate_state_json()
                    : "Error: Service not available"))
            )
        );
        
        response.set_html(render(page));
    }
    
    return response;
}

void HTTPService::handle(messaging::Message&& msg) {
    // Handle incoming HTTP-related messages
}

std::vector<messaging::Message> HTTPService::generate_outbound() {
    // Generate HTTP responses as messages
    return {};
}

bool HTTPService::accepts_html(const HttpRequest& request) const {
    auto it = request.headers.find("Accept");
    if (it != request.headers.end()) {
        return it->second.find("text/html") != std::string::npos;
    }
    return false;
}

bool HTTPService::accepts_json(const HttpRequest& request) const {
    auto it = request.headers.find("Accept");
    if (it != request.headers.end()) {
        return it->second.find("application/json") != std::string::npos;
    }
    return false;
}

} // namespace protoflow::mainapp
