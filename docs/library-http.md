# protoflow-http

HTTP service library with runtime endpoint registration and automatic HTML/JSON content negotiation.

## Overview

The HTTP library provides:
- HTTP server with runtime endpoint registration
- Automatic content negotiation (HTML vs JSON based on Accept header)
- Integration with application registration service
- Built-in support for HTML fragments and JSON responses

## Key Features

- **Runtime Endpoint Registration**: Add HTTP endpoints dynamically via service registration
- **Content Negotiation**: Same endpoint responds with HTML or JSON based on Accept header
- **Type-Safe Handlers**: Compile-time verified handler signatures
- **HTML Fragment Support**: Direct integration with protoflow-html-fragment
- **JSON Rendering**: Automatic JSON serialization for data types

## Architecture

```
HTTP Request → Router → Handler → Content Negotiator → Response
                  ↓
        Registration Service
```

## Basic Usage

```cpp
#include <protoflow/http.hpp>

using namespace protoflow::http;

// Define a handler that returns data
struct StatusData {
    std::string status;
    int uptime_seconds;
};

auto get_status() -> StatusData {
    return {"running", 3600};
}

// Register endpoint
http_service srv;
srv.route("GET", "/status", get_status);

// Responds with:
// - HTML if Accept: text/html
// - JSON if Accept: application/json
```

## HTML Fragment Integration

```cpp
#include <protoflow/http.hpp>
#include <protoflow/html.hpp>

auto get_dashboard() {
    using namespace protoflow::html;
    
    return container(
        card(h1(text("Dashboard")), 
             p(text("System running")))
    );
}

srv.route("GET", "/dashboard", get_dashboard);
// Returns rendered HTML fragment
```

## Content Negotiation

The library automatically handles content negotiation:

```cpp
// Handler returns data
auto get_metrics() -> MetricsData { /* ... */ }

srv.route("GET", "/metrics", get_metrics);

// Browser request (Accept: text/html):
// → Renders as HTML using default formatter or custom template

// API request (Accept: application/json):
// → Returns JSON: {"cpu": 23.5, "memory": 4096}
```

## Handler Types

### HTML Fragment Handlers

```cpp
auto handler() -> html::node_ptr {
    return html::div(text("Hello"));
}
```

### Data Handlers (dual HTML/JSON)

```cpp
struct Response {
    std::string message;
    int code;
};

auto handler() -> Response {
    return {"success", 200};
}
```

### Raw String Handlers

```cpp
auto handler() -> std::string {
    return "Plain text response";
}
```

## Runtime Registration

Endpoints are registered through the application service:

```cpp
// Service registers its endpoints
class MyService : public protoflow::service {
public:
    void on_start() override {
        auto http = get_service<http_service>();
        http->route("GET", "/api/data", &MyService::get_data, this);
    }
    
    auto get_data() -> DataResponse { /* ... */ }
};
```

## HTTP Methods

Supported HTTP methods:
- GET
- POST
- PUT
- DELETE
- PATCH

## Path Parameters

```cpp
srv.route("GET", "/users/:id", [](const std::string& id) {
    return get_user(id);
});
```

## Request Context

```cpp
auto handler(const http::request& req) {
    auto user_agent = req.header("User-Agent");
    auto query_param = req.query("filter");
    
    return response_data;
}
```

## Response Control

```cpp
auto handler() -> http::response {
    return http::response()
        .status(200)
        .header("Cache-Control", "no-cache")
        .body(my_data);
}
```

## Implementation Details

### Content Type Detection

```cpp
// Checks Accept header:
// text/html, */*, or application/xhtml+xml → HTML response
// application/json → JSON response
```

### JSON Serialization

Uses compile-time reflection for automatic JSON conversion:

```cpp
struct User {
    std::string name;
    int age;
};

// Automatically serializes to: {"name":"John","age":30}
```

### HTML Rendering

For data types without custom HTML renderer:

```cpp
// Default table renderer for structs
struct Data { int x; std::string y; };

// Renders as:
// <div class="data">
//   <div class="data-row"><span>x</span><span>42</span></div>
//   <div class="data-row"><span>y</span><span>value</span></div>
// </div>
```

## See Also

- [Application Service](library-service.md) - Service registration
- [HTML Fragment](library-html-fragment.md) - HTML generation
- [RPC](library-rpc.md) - Inter-service communication
