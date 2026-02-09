#include "services/http_service.hpp"
#include "messages.hpp"
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
    
    // Check for timed out requests
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> timed_out_ids;
    
    for (const auto& [req_id, pending] : pending_requests_) {
        auto elapsed = now - pending.timestamp;
        
        if (elapsed >= REQUEST_TIMEOUT) {
            timed_out_ids.push_back(req_id);
        }
    }
    
    // Remove timed out requests and send error responses
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
    
    // In a full implementation:
    // 1. Accept incoming connections
    // 2. Parse HTTP requests
    // 3. Route to appropriate handlers
    // 4. Send HTTP responses from pending_responses_
    
    // For now, this is a placeholder that would integrate with an actual HTTP server
    
    service::Service::poll();
}

void HTTPService::register_endpoint(const std::string& method, 
                                    const std::string& path, 
                                    HttpHandler handler) {
    std::string key = method + " " + path;
    endpoints_[key] = std::move(handler);
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
    
    // TODO: Send NavigationRequest message to AppRegistrationService
    // For now, serve a basic page
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
    
    // TODO: Send FragmentRequest message to AppRegistrationService
    // and wait for FragmentResponse
    
    HttpResponse response;
    response.status_code = 404;
    response.set_html("<html><body><h1>404 Not Found</h1><p>App or endpoint not found</p></body></html>");
    return response;
}

HttpResponse HTTPService::serve_state_api(const HttpRequest& request) {
    HttpResponse response;
    
    // TODO: Send StateRequest message to AppRegistrationService
    // and wait for StateResponse
    
    if (accepts_json(request) || !accepts_html(request)) {
        // Return JSON
        response.set_json(R"({"error": "Not yet implemented - requires message-based communication"})");
    } else {
        // Return HTML representation
        using namespace html;
        auto page = html_doc(
            head(title(text("System State"))),
            body(
                h1(text("System State")),
                pre(text("Not yet implemented - requires message-based communication"))
            )
        );
        
        response.set_html(render(page));
    }
    
    return response;
}

void HTTPService::handle(service::Message&& msg) {
    // Handle navigation responses
    if (auto* nav_response = std::get_if<NavigationResponse>(&msg.payload)) {
        PROTOFLOW_LOG_INFO(*this, "Received NavigationResponse for request #" << nav_response->request_id);
        
        // Remove from pending requests
        pending_requests_.erase(nav_response->request_id);
        
        // Build HTTP response with navigation HTML
        HttpResponse response;
        response.set_html(nav_response->html_fragment);
        pending_responses_.push_back(std::move(response));
    }
    // Handle state responses
    else if (auto* state_response = std::get_if<StateResponse>(&msg.payload)) {
        PROTOFLOW_LOG_INFO(*this, "Received StateResponse for request #" << state_response->request_id);
        
        // Remove from pending requests
        pending_requests_.erase(state_response->request_id);
        
        // Build HTTP response with state data
        HttpResponse response;
        // Content is already formatted as JSON or HTML
        if (state_response->content.starts_with("{") || state_response->content.starts_with("[")) {
            response.set_json(state_response->content);
        } else {
            response.set_html(state_response->content);
        }
        pending_responses_.push_back(std::move(response));
    }
    // Handle fragment responses
    else if (auto* frag_response = std::get_if<FragmentResponse>(&msg.payload)) {
        PROTOFLOW_LOG_INFO(*this, "Received FragmentResponse for request #" << frag_response->request_id
                          << " (" << frag_response->app_name << frag_response->endpoint << ")");
        
        // Remove from pending requests
        pending_requests_.erase(frag_response->request_id);
        
        HttpResponse response;
        response.status_code = frag_response->status_code;
        
        if (frag_response->html_fragment) {
            // Fragment is already complete HTML, use it directly
            response.set_html(*frag_response->html_fragment);
        } else {
            response.set_html("<html><body><h1>404 Not Found</h1><p>App or endpoint not found</p></body></html>");
        }
        
        pending_responses_.push_back(std::move(response));
    }
    // Handle log responses (from LoggingService)
    else if (auto* log_response = std::get_if<logging::LogResponse>(&msg.payload)) {
        PROTOFLOW_LOG_INFO(*this, "Received LogResponse for request #" << log_response->request_id
                          << " with " << log_response->logs.size() << " logs (total matches: " 
                          << log_response->total_matches << ")");
        
        // Remove from pending requests
        pending_requests_.erase(log_response->request_id);
        
        HttpResponse response;
        
        // Build JSON response with log data
        std::ostringstream json;
        json << "{\n";
        json << "  \"total\": " << log_response->total_matches << ",\n";
        json << "  \"count\": " << log_response->logs.size() << ",\n";
        json << "  \"logs\": [\n";
        
        for (size_t i = 0; i < log_response->logs.size(); ++i) {
            const auto& log = log_response->logs[i];
            
            // Convert timestamp to ISO 8601 string
            auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
            std::tm tm_buf{};
            gmtime_r(&time_t, &tm_buf);
            char timestamp_buf[64];
            std::strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
            
            json << "    {\n";
            json << "      \"timestamp\": \"" << timestamp_buf << "\",\n";
            json << "      \"level\": \"" << to_string(log.level) << "\",\n";
            json << "      \"source\": " << log.source << ",\n";
            json << "      \"message\": \"" << log.message << "\"\n";
            json << "    }";
            if (i < log_response->logs.size() - 1) json << ",";
            json << "\n";
        }
        
        json << "  ]\n";
        json << "}\n";
        
        response.set_json(json.str());
        pending_responses_.push_back(std::move(response));
    }
}

std::vector<service::Message> HTTPService::generate_outbound() {
    std::vector<service::Message> messages;
    
    // In a full implementation, this would generate request messages
    // based on incoming HTTP requests that need data from other services
    
    return messages;
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
