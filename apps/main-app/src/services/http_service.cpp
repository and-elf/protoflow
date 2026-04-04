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

    // ── Sidebar navigation items (runtime app data → string concat) ──
    std::string nav_items_html;
    if (registered_apps_.empty()) {
        nav_items_html = to_html(
            li(attrs<class_<"nav-item">>{},
               em(text("No apps registered")))
        );
    } else {
        for (const auto& [name, endpoints] : registered_apps_) {
            nav_items_html +=
                "<li class=\"nav-item\">"  // runtime attrs for dynamic href
                "<a class=\"nav-link\" href=\"#\" data-app=\"" + name + "\" "
                "onclick=\"loadFragment('" + name + "','status'); return false;\">" +
                name + "</a></li>";
        }
    }

    // ── Header (html-fragment) ──
    std::string header_html = to_html(
        div(attrs<class_<"header">>{},
            h1(text("Protoflow Dashboard")),
            badge_info(text("v0.1"))
        )
    );

    // ── Welcome content (html-fragment semantic components) ──
    std::string welcome_html = to_html(
        container(
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
        )
    );

    // ── Assemble full HTML document ──
    std::string page =
        "<!DOCTYPE html>\n<html>\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<title>Protoflow Dashboard</title>\n"
        "<style>\n"
        R"css(
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
     background:#f0f2f5;color:#333}
.header{background:#1a1a2e;color:#fff;padding:14px 24px;
        display:flex;align-items:center;justify-content:space-between}
.header h1{font-size:1.3rem;font-weight:600}
.layout{display:flex;height:calc(100vh - 52px)}
.sidebar{width:240px;background:#16213e;color:#ccc;padding:20px 0;
         overflow-y:auto;flex-shrink:0}
.sidebar h2{font-size:.8rem;text-transform:uppercase;letter-spacing:1.5px;
            color:#7a7a9a;padding:0 20px 12px;border-bottom:1px solid #2a2a4a;
            margin-bottom:8px}
.nav-list{list-style:none}
.nav-item{margin:1px 0}
.nav-link{display:block;padding:10px 20px;color:#b0b0c8;text-decoration:none;
          font-size:.9rem;transition:all .15s}
.nav-link:hover{background:#1a1a3e;color:#fff}
.nav-link.active{background:#0d2137;color:#4fc3f7;border-left:3px solid #4fc3f7;
                 padding-left:17px}
.main-content{flex:1;padding:28px;overflow-y:auto}
.container{max-width:840px}
.card{background:#fff;border-radius:8px;box-shadow:0 1px 3px rgba(0,0,0,.08);
      margin-bottom:16px;overflow:hidden}
.card-header{padding:14px 20px;border-bottom:1px solid #eee;font-weight:600}
.card-body{padding:20px}
.section{margin-bottom:24px}
.section-title{margin-bottom:12px}
.data-row{display:flex;justify-content:space-between;align-items:center;
          padding:10px 0;border-bottom:1px solid #f5f5f5}
.data-row:last-child{border-bottom:none}
.data-label{color:#666;font-size:.9rem}
.data-value{font-weight:500}
.badge{display:inline-block;padding:3px 10px;border-radius:12px;
       font-size:.78rem;font-weight:600}
.badge-success{background:#e8f5e9;color:#2e7d32}
.badge-error{background:#ffebee;color:#c62828}
.badge-warning{background:#fff3e0;color:#e65100}
.badge-info{background:#e3f2fd;color:#1565c0}
.status{padding:8px 14px;border-radius:6px;font-size:.9rem}
.status-ok{background:#e8f5e9;border-left:3px solid #4caf50}
.status-error{background:#ffebee;border-left:3px solid #f44336}
.status-warning{background:#fff3e0;border-left:3px solid #ff9800}
.status-pending{background:#f3e5f5;border-left:3px solid #9c27b0}
.metric{display:flex;align-items:baseline;gap:8px;padding:6px 0}
.metric-label{color:#666;font-size:.9rem}
.metric-value{font-size:1.2rem;font-weight:600}
.metric-unit{color:#999;font-size:.8rem}
.loading{text-align:center;padding:48px;color:#999}
.error-box{background:#ffebee;color:#c62828;padding:16px 20px;border-radius:8px;
           border-left:3px solid #f44336}
ul{list-style:disc;padding-left:20px}
code{background:#f5f5f5;padding:2px 6px;border-radius:3px;font-size:.85rem}
)css"
        "</style>\n</head>\n<body>\n"
        + header_html +
        "<div class=\"layout\">\n"
        "  <nav class=\"sidebar\">\n"
        "    <h2>Applications</h2>\n"
        "    <ul class=\"nav-list\">" + nav_items_html + "</ul>\n"
        "  </nav>\n"
        "  <main class=\"main-content\" id=\"main-content\">\n"
        + welcome_html +
        "  </main>\n"
        "</div>\n"
        "<script>\n"
        R"js(
async function loadFragment(appName, fragmentId) {
    var main = document.getElementById('main-content');
    main.innerHTML = '<div class="loading">Loading fragment\u2026</div>';

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
)js"
        "</script>\n</body>\n</html>";

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

// ────────────────────────────────────────────────────────────────
//  App registration / unregistration handlers
// ────────────────────────────────────────────────────────────────

void HTTPService::handle_app_registration(const AppRegistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App registered: " << event.app_name
                      << " with " << event.endpoints.size() << " endpoints");

    // Track the app and its endpoints; the wildcard /app/* handler
    // (serve_app_endpoint) routes fragment and overview requests.
    registered_apps_[event.app_name] = event.endpoints;
}

void HTTPService::handle_app_unregistration(const AppUnregistrationEvent& event) {
    PROTOFLOW_LOG_INFO(*this, "App unregistered: " << event.app_name);
    registered_apps_.erase(event.app_name);
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
