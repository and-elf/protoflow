// Example: HTTP service with runtime endpoint registration
#include <protoflow/http.hpp>
#include <protoflow/html.hpp>
#include <iostream>

using namespace protoflow::http;
using namespace protoflow::html;

// Example data structures (must be at namespace scope for JSON macro)
struct SystemStatus {
    std::string state;
    int uptime_seconds;
    double cpu_usage;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SystemStatus, state, uptime_seconds, cpu_usage)

struct ProcessInfo {
    int pid;
    std::string name;
    double cpu;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ProcessInfo, pid, name, cpu)

// Example: Handler that returns data (auto content-negotiation)
auto get_system_status(const request& req) -> SystemStatus {
    return {"running", 3600, 23.5};
}

// Example: Handler that returns HTML fragment
auto get_dashboard() {
    auto status = get_system_status(request{});
    
    return container(
        section_with_title(text("System Dashboard"),
            data_row(text("Status"), badge_success(text(status.state))),
            data_row(text("Uptime"), text(std::to_string(status.uptime_seconds) + " seconds")),
            data_row(text("CPU Usage"), text(std::to_string(status.cpu_usage) + "%"))
        )
    );
}

// Example: Handler that returns JSON directly
auto get_metrics_json(const request& req) -> response {
    SystemStatus status = {"running", 3600, 23.5};
    
    return ok(json::to_json(status))
        .content_type("application/json; charset=utf-8");
}

// Example: Handler with path parameters
auto get_process_info(const request& req) -> response {
    auto pid = req.path_param("pid");
    
    if (!pid) {
        return bad_request("Missing process ID");
    }
    
    // Simulate process data
    if (req.accepts_html()) {
        auto fragment = container(
            h2(text("Process " + std::string(*pid))),
            data_row(text("PID"), text(*pid)),
            data_row(text("Status"), badge_success(text("Running")))
        );
        
        protoflow::html::render_ctx ctx;
        fragment.render(ctx);
        return ok(ctx.take_result()).content_type("text/html; charset=utf-8");
    } else {
        auto json_data = "{\"pid\":" + std::string(*pid) + ",\"status\":\"running\"}";
        return ok(json_data).content_type("application/json; charset=utf-8");
    }
}

// Example: Table data handler
auto get_process_list(const request& req) -> std::vector<ProcessInfo> {
    return {
        {1234, "app-server", 12.3},
        {5678, "database", 45.7},
        {9012, "cache", 3.2}
    };
}

int main() {
    http_service server;
    
    // Register routes
    
    // Data endpoint with content negotiation
    server.get("/api/status", [](const request& r) { return get_system_status(r); });
    
    // HTML fragment endpoint
    server.get("/dashboard", [](const request&) { return get_dashboard(); });
    
    // Explicit JSON endpoint
    server.get("/metrics.json", get_metrics_json);
    
    // Parameterized route
    server.route("GET", "/process/:pid", get_process_info);
    
    // List data with auto content-negotiation
    server.get("/api/processes", [](const request& r) { return get_process_list(r); });
    
    // Lambda handler
    server.get("/health", [](const request&) -> response {
        return ok("healthy").content_type("text/plain");
    });
    
    // Simulate requests
    std::cout << "=== Testing HTTP Service ===\n\n";
    
    // Test 1: Status with JSON accept
    {
        request req(method::GET, "/api/status");
        req.set_header("Accept", "application/json");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /api/status (JSON)\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Content-Type: " << resp.get_header("Content-Type") << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    // Test 2: Status with HTML accept
    {
        request req(method::GET, "/api/status");
        req.set_header("Accept", "text/html");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /api/status (HTML)\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Content-Type: " << resp.get_header("Content-Type") << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    // Test 3: Dashboard HTML
    {
        request req(method::GET, "/dashboard");
        req.set_header("Accept", "text/html");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /dashboard\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Body:\n" << resp.get_body() << "\n\n";
    }
    
    // Test 4: Path parameters
    {
        request req(method::GET, "/process/1234");
        req.set_header("Accept", "application/json");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /process/1234 (JSON)\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    // Test 5: Process list
    {
        request req(method::GET, "/api/processes");
        req.set_header("Accept", "application/json");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /api/processes (JSON)\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    // Test 6: Health check
    {
        request req(method::GET, "/health");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /health\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    // Test 7: 404
    {
        request req(method::GET, "/nonexistent");
        
        auto resp = server.handle_request(req);
        std::cout << "GET /nonexistent\n";
        std::cout << "Status: " << resp.get_status() << "\n";
        std::cout << "Body: " << resp.get_body() << "\n\n";
    }
    
    return 0;
}
