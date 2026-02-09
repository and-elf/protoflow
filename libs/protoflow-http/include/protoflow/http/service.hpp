// Copyright (c) 2026 Andreas
// SPDX-License-Identifier: MIT
#pragma once

#include "router.hpp"
#include <memory>
#include <string>

namespace protoflow::http {

// HTTP service for runtime endpoint registration
class http_service {
public:
    http_service() = default;
    virtual ~http_service() = default;
    
    // Register route with handler that takes request
    template<typename Func>
    auto route(std::string method_str, std::string path, Func&& handler) -> void {
        auto m = from_string(method_str);
        if (!m) return;
        
        router_.add_route(*m, std::move(path), make_handler(std::forward<Func>(handler)));
    }
    
    // Register route with handler that takes no arguments
    template<typename Func>
    auto route_simple(std::string method_str, std::string path, Func&& handler) -> void {
        auto m = from_string(method_str);
        if (!m) return;
        
        router_.add_route(*m, std::move(path), make_handler_no_args(std::forward<Func>(handler)));
    }
    
    // Register GET route
    template<typename Func>
    auto get(std::string path, Func&& handler) -> void {
        route("GET", std::move(path), std::forward<Func>(handler));
    }
    
    // Register POST route
    template<typename Func>
    auto post(std::string path, Func&& handler) -> void {
        route("POST", std::move(path), std::forward<Func>(handler));
    }
    
    // Register PUT route
    template<typename Func>
    auto put(std::string path, Func&& handler) -> void {
        route("PUT", std::move(path), std::forward<Func>(handler));
    }
    
    // Register DELETE route
    template<typename Func>
    auto del(std::string path, Func&& handler) -> void {
        route("DELETE", std::move(path), std::forward<Func>(handler));
    }
    
    // Handle incoming request
    auto handle_request(request& req) -> response {
        return router_.handle(req);
    }
    
    // Start HTTP server (to be implemented)
    virtual auto start(int port) -> void {
        port_ = port;
        // Actual server implementation would go here
        // For now, just store the port
    }
    
    // Stop HTTP server
    virtual auto stop() -> void {
        // Server stop implementation
    }
    
    auto get_port() const -> int { return port_; }

private:
    router router_;
    int port_ = 8080;
};

} // namespace protoflow::http
