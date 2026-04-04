#include "services/http_service.hpp"
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
    register_endpoint("GET", "/", [this](const HttpRequest& req) {
        return serve_home(req);
    });

    register_endpoint("GET", "/api/state", [this](const HttpRequest& req) {
        return serve_state_api(req);
    });

    register_endpoint("GET", "/status", [this](const HttpRequest& req) {
        return serve_status(req);
    });

    register_endpoint("GET", "/api/status", [this](const HttpRequest& req) {
        return serve_status_async(req);
    });

    register_endpoint("GET", "/static/*", [this](const HttpRequest& req) {
        return serve_static(req);
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
//  Built-in page handlers
// ────────────────────────────────────────────────────────────────

HttpResponse HTTPService::serve_home(const HttpRequest& /*request*/) {
    using namespace html;

    // ── Build navigation items HTML manually for runtime flexibility ──
    std::string nav_items_html;
    if (registered_apps_.empty()) {
        nav_items_html = to_html(
            li(attrs<class_<"nav-item">>{},
               em(text("No apps registered")))
        );
    } else {
        for (const auto& [name, endpoints] : registered_apps_) {
            nav_items_html +=
                "<li class=\"nav-item\">"
                "<a class=\"nav-link\" href=\"#\" data-app=\"" + name + "\">" +
                name + "</a></li>";
        }
    }

    // ── Build page with html-fragment components and external CSS ──
    // Header
    auto header = div(attrs<class_<"header">>{},
        h1(text("Protoflow Dashboard")),
        badge_info(text("v0.1"))
    );

    // Welcome content
    auto welcome = container(
        section_with_title(
            h2(text("System Overview")),
            card(
                h3(text("Welcome to Protoflow")),
                p(text("Select an application from the sidebar to view "
                       "its status fragment.")),
                data_row(
                    text("Registered apps:"),
                    badge_info(text(std::to_string(registered_apps_.size())))
                ),
                data_row(
                    text("Server status:"),
                    status_ok(text("Running"))
                )
            )
        )
    );

    // ── Assemble HTML document ──
    std::string page =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <title>Protoflow Dashboard</title>\n"
        "  <link rel=\"stylesheet\" href=\"/static/styles.css\">\n"
        "</head>\n"
        "<body>\n"
        + to_html(header) +
        "  <div class=\"layout\">\n"
        "    <nav class=\"sidebar\">\n"
        "      <h2>Applications</h2>\n"
        "      <ul class=\"nav-list\">\n"
        + nav_items_html +
        "      </ul>\n"
        "    </nav>\n"
        "    <main class=\"main-content\" id=\"main-content\">\n"
        + to_html(welcome) +
        "    </main>\n"
        "  </div>\n"
        "  <script>\n"
        R"js(
async function loadFragment(appName, fragmentId) {
    var main = document.getElementById('main-content');
    main.innerHTML = '<div class="loading">Loading fragment…</div>';

    document.querySelectorAll('.nav-link').forEach(function(el) {
        el.classList.remove('active');
    });
    var active = document.querySelector('[data-app="' + appName + '"]');
    if (active) active.classList.add('active');

    try {
        var resp = await fetch('/app/' + appName + '/fragment/' + fragmentId);
        if (resp.ok) {
            main.innerHTML = await resp.text();
        } else {
            main.innerHTML = '<div class="error-box">Failed to load fragment (HTTP ' + resp.status + ')</div>';
        }
    } catch (e) {
        main.innerHTML = '<div class="error-box">Network error: ' + e.message + '</div>';
    }
}

// Set up click handlers for nav links
document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('.nav-link').forEach(function(link) {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            var appName = this.getAttribute('data-app');
            if (appName) {
                loadFragment(appName, 'status');
            }
        });
    });
});
)js"
        "  </script>\n"
        "</body>\n"
        "</html>";

    HttpResponse response;
    response.set_html(page);
    return response;
}

HttpResponse HTTPService::serve_app_endpoint(const HttpRequest& request) {
    using namespace html;

    // Parse path: /app/{name}/fragment/{id} or /app/{name}/...
    std::string_view path = request.path;
    auto rest = path.substr(5);  // skip "/app/"

    auto slash = rest.find('/');
    std::string app_name;
    std::string_view remainder;

    if (slash == std::string_view::npos) {
        app_name = std::string(rest);
    } else {
        app_name = std::string(rest.substr(0, slash));
        remainder = rest.substr(slash + 1);
    }

    if (app_name.empty()) {
        HttpResponse response;
        response.status_code = 302;
        response.headers["Location"] = "/";
        return response;
    }

    // Fragment request: /app/{name}/fragment/{id}
    if (remainder.starts_with("fragment/") && remainder.size() > 9) {
        std::string fragment_id(remainder.substr(9));
        return serve_fragment(app_name, fragment_id);
    }

    // App overview
    auto it = registered_apps_.find(app_name);
    if (it == registered_apps_.end()) {
        HttpResponse response;
        response.status_code = 404;
        response.set_html(to_html(
            card(
                h3(text("Not Found")),
                status_error(text("Application '" + app_name + "' is not registered"))
            )
        ));
        return response;
    }

    HttpResponse response;
    response.set_html(to_html(
        container(
            card(
                h3(text(app_name)),
                data_row(text("Endpoints:"),
                         text(std::to_string(it->second.size()))),
                data_row(text("Status:"),
                         badge_success(text("Registered")))
            )
        )
    ));
    return response;
}

HttpResponse HTTPService::serve_fragment(const std::string& app_name,
                                         const std::string& fragment_id) {
    using namespace html;

    auto it = registered_apps_.find(app_name);
    if (it == registered_apps_.end()) {
        HttpResponse response;
        response.status_code = 404;
        response.set_html(to_html(
            card(
                h3(text("Not Found")),
                status_error(text("Application '" + app_name + "' is not registered"))
            )
        ));
        return response;
    }

    // Build endpoint list items using html-fragment
    std::string ep_items;
    for (const auto& ep : it->second) {
        ep_items += to_html(li(code(text(ep))));
    }

    // Fragment info card (html-fragment semantic components)
    std::string info_html = to_html(
        card(
            h3(text(app_name + " \u2014 " + fragment_id)),
            data_row(text("Application:"), badge_success(text(app_name))),
            data_row(text("Fragment ID:"),  badge_info(text(fragment_id))),
            data_row(text("Endpoints:"),    text(std::to_string(it->second.size())))
        )
    );

    // Note about RPC forwarding
    std::string note_html = to_html(
        card_simple(
            status_pending(
                text("This is a placeholder fragment rendered by the main app's "
                     "HTTPService using the html-fragment library. In production, "
                     "fragment requests are forwarded to '" + app_name + "' via "
                     "RPC (cmd::render_fragment) and the app returns its own "
                     "html-fragment output.")
            )
        )
    );

    // Endpoint list card
    std::string ep_header = to_html(h3(text("Registered Endpoints")));
    std::string ep_html =
        "<div class=\"card\"><div class=\"card-header\">" +
        ep_header +
        "</div><div class=\"card-body\"><ul>" +
        ep_items +
        "</ul></div></div>";

    HttpResponse response;
    response.set_html(
        "<div class=\"container\">" +
        info_html + note_html + ep_html +
        "</div>"
    );
    return response;
}

HttpResponse HTTPService::serve_state_api(const HttpRequest& request) {
    using namespace html;
    HttpResponse response;

    if (accepts_json(request) || !accepts_html(request)) {
        std::string json = R"({"registered_apps":[)";
        bool first = true;
        for (const auto& [name, eps] : registered_apps_) {
            if (!first) json += ",";
            json += R"({"name":")"
                    + name + R"(","endpoints":)"
                    + std::to_string(eps.size()) + "}";
            first = false;
        }
        json += "]}";
        response.set_json(json);
    } else {
        std::string body_html = to_html(
            container(
                card(
                    h3(text("System State")),
                    data_row(
                        text("Registered applications:"),
                        badge_info(text(std::to_string(registered_apps_.size())))
                    ),
                    data_row(
                        text("Status:"),
                        status_ok(text("Running"))
                    )
                )
            )
        );
        response.set_html(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>System State</title></head><body>"
            + body_html +
            "</body></html>"
        );
    }

    return response;
}

std::string HTTPService::fetch_app_status(const std::string& app_name,
                                          const std::string& base_url) {
    // For now, return a placeholder that indicates we tried to fetch from the app
    // In a full implementation, this would make an HTTP GET request to base_url/status
    // and parse the response
    PROTOFLOW_LOG_DEBUG(*this, "Would fetch status from " << app_name
                       << " at " << base_url << "/status");
    return "{}";
}

HttpResponse HTTPService::serve_status_async(const HttpRequest& /*request*/) {
    HttpResponse response;

    // Build JSON response with data from registered apps
    std::string json = R"({"status":"running","registered_apps":[)";
    bool first = true;
    for (const auto& [name, info] : registered_apps_info_) {
        if (!first) json += ",";
        
        // Escape app name for JSON
        std::string escaped_name = name;
        size_t pos = 0;
        while ((pos = escaped_name.find('"', pos)) != std::string::npos) {
            escaped_name.replace(pos, 1, "\\\"");
            pos += 2;
        }
        
        // Try to fetch app-specific status
        // For now, just include what we know from registration
        json += R"({"name":")"
                + escaped_name
                + R"(","endpoints":)"
                + std::to_string(info.endpoints.size())
                + R"(,"http_listener":")"
                + info.http_listener
                + "\"}";
        first = false;
    }
    json += R"(],"timestamp":""})";

    response.set_json(json);
    return response;
}

HttpResponse HTTPService::serve_status(const HttpRequest& /*request*/) {
    HttpResponse response;

    // Build JSON response
    std::string json = R"({"status":"running","registered_apps":[)";
    bool first = true;
    for (const auto& [name, endpoints] : registered_apps_) {
        if (!first) json += ",";
        // Escape app name for JSON
        std::string escaped_name = name;
        // Simple escape: replace quotes with escaped quotes
        size_t pos = 0;
        while ((pos = escaped_name.find('"', pos)) != std::string::npos) {
            escaped_name.replace(pos, 1, "\\\"");
            pos += 2;
        }
        json += R"({"name":")"
                + escaped_name + R"(","endpoints":)"  
                + std::to_string(endpoints.size())
                + "}";
        first = false;
    }
    json += R"(],"timestamp":""})";

    response.set_json(json);
    return response;
}

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

// ────────────────────────────────────────────────────────────────
//  Static file serving
// ────────────────────────────────────────────────────────────────

std::optional<std::vector<std::byte>> HTTPService::read_static_file(const std::string& filename) {
    if (static_dir_.empty()) {
        PROTOFLOW_LOG_DEBUG(*this, "Static directory not configured");
        return std::nullopt;
    }

    // Prevent path traversal attacks
    if (filename.find("..") != std::string::npos || filename.find("//") != std::string::npos) {
        PROTOFLOW_LOG_WARN(*this, "Attempted path traversal in static file: " << filename);
        return std::nullopt;
    }

    std::string filepath = static_dir_ + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        PROTOFLOW_LOG_DEBUG(*this, "Static file not found: " << filepath);
        return std::nullopt;
    }

    // Read file into vector
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        PROTOFLOW_LOG_WARN(*this, "Error reading static file: " << filepath);
        return std::nullopt;
    }

    PROTOFLOW_LOG_DEBUG(*this, "Served static file: " << filename << " (" << size << " bytes)");
    return buffer;
}

HttpResponse HTTPService::serve_static(const HttpRequest& request) {
    HttpResponse response;

    // Extract filename from path: /static/{filename}
    std::string_view path = request.path;
    if (!path.starts_with("/static/")) {
        response.status_code = 400;
        response.set_json(R"({"error": "Invalid static path"})");
        return response;
    }

    // Get the filename relative to /static/
    std::string filename(path.substr(8));  // skip "/static/"

    if (filename.empty()) {
        response.status_code = 400;
        response.set_json(R"({"error": "No file specified"})");
        return response;
    }

    // Try to read the file
    auto file_data = read_static_file(filename);
    if (!file_data) {
        response.status_code = 404;
        response.set_json(R"({"error": "Static file not found"})");
        return response;
    }

    // Determine content type based on file extension
    std::string content_type = "application/octet-stream";
    if (filename.ends_with(".css")) {
        content_type = "text/css; charset=utf-8";
    } else if (filename.ends_with(".js")) {
        content_type = "application/javascript; charset=utf-8";
    } else if (filename.ends_with(".html")) {
        content_type = "text/html; charset=utf-8";
    } else if (filename.ends_with(".json")) {
        content_type = "application/json; charset=utf-8";
    } else if (filename.ends_with(".png")) {
        content_type = "image/png";
    } else if (filename.ends_with(".jpg") || filename.ends_with(".jpeg")) {
        content_type = "image/jpeg";
    } else if (filename.ends_with(".svg")) {
        content_type = "image/svg+xml";
    } else if (filename.ends_with(".woff")) {
        content_type = "font/woff";
    } else if (filename.ends_with(".woff2")) {
        content_type = "font/woff2";
    }

    response.status_code = 200;
    response.headers["Content-Type"] = content_type;
    response.body = std::move(*file_data);

    return response;
}

} // namespace protoflow::mainapp
