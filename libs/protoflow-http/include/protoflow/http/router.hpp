// Copyright (c) 2026 Andreas
// SPDX-License-Identifier: MIT
#pragma once

#include "request.hpp"
#include "response.hpp"
#include "json.hpp"
#include <protoflow/html.hpp>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <regex>

// Forward-declare render context from html-fragment in global scope
namespace protoflow::html { class render_ctx; }

namespace protoflow::http {

// Handler function type
using handler_func = std::function<response(const request&)>;

// Route pattern matching
class route_pattern {
public:
    explicit route_pattern(std::string pattern)
        : pattern_(std::move(pattern)) {
        parse();
    }
    
    auto matches(std::string_view path) const -> bool {
        if (param_positions_.empty()) {
            // Simple exact match
            return path == pattern_;
        }
        
        // Regex match for parameterized routes
        return std::regex_match(std::string(path), regex_);
    }
    
    auto extract_params(std::string_view path, request& req) const -> void {
        if (param_positions_.empty()) {
            return;
        }
        
        std::smatch matches;
        std::string path_str(path);
        if (std::regex_match(path_str, matches, regex_)) {
            for (size_t i = 0; i < param_names_.size(); ++i) {
                req.set_path_param(param_names_[i], matches[i + 1].str());
            }
        }
    }

private:
    void parse() {
        // Find :param patterns
        std::string regex_pattern;
        size_t pos = 0;
        
        for (size_t i = 0; i < pattern_.size(); ++i) {
            if (pattern_[i] == ':') {
                // Found parameter
                size_t start = i + 1;
                size_t end = start;
                while (end < pattern_.size() && 
                       (std::isalnum(pattern_[end]) || pattern_[end] == '_')) {
                    ++end;
                }
                
                std::string param_name = pattern_.substr(start, end - start);
                param_names_.push_back(param_name);
                param_positions_.push_back(start - 1);
                
                // Add regex pattern
                regex_pattern += pattern_.substr(pos, i - pos);
                regex_pattern += "([^/]+)";
                
                pos = end;
                i = end - 1;
            }
        }
        
        if (!param_positions_.empty()) {
            regex_pattern += pattern_.substr(pos);
            regex_ = std::regex(regex_pattern);
        }
    }

    std::string pattern_;
    std::vector<std::string> param_names_;
    std::vector<size_t> param_positions_;
    std::regex regex_;
};

// Route entry
struct route {
    method method_type;
    route_pattern pattern;
    handler_func handler;
    
    route(method m, std::string path, handler_func h)
        : method_type(m), pattern(std::move(path)), handler(std::move(h)) {}
};

// HTTP Router
class router {
public:
    router() = default;
    
    // Add route with handler
    auto add_route(method m, std::string path, handler_func handler) -> void {
        routes_.emplace_back(m, std::move(path), std::move(handler));
    }
    
    // Find matching route
    auto find_route(const request& req) -> handler_func* {
        for (auto& route : routes_) {
            if (route.method_type == req.get_method() && 
                route.pattern.matches(req.get_path())) {
                return &route.handler;
            }
        }
        return nullptr;
    }
    
    // Handle request
    auto handle(request& req) -> response {
        // Find matching route
        for (auto& route : routes_) {
            if (route.method_type == req.get_method() && 
                route.pattern.matches(req.get_path())) {
                // Extract path parameters
                route.pattern.extract_params(req.get_path(), req);
                return route.handler(req);
            }
        }
        
        return not_found("Route not found");
    }

private:
    std::vector<route> routes_;
};

// Content negotiation helpers
namespace detail {
    // Forward-declare render_ctx to avoid include-order issues
    namespace protoflow_html_forward {
        class render_ctx;
    }
    // Check if type has render method (is an HTML tag/node)
    template<typename T>
    concept has_render = requires(const T& t, protoflow::html::render_ctx& ctx) {
        { t.render(ctx) } -> std::same_as<void>;
    };
    
    // Check if type is response
    template<typename T>
    concept is_response = std::is_same_v<std::remove_cvref_t<T>, response>;
    
    // Check if type is string-like
    template<typename T>
    concept is_string = std::is_convertible_v<T, std::string> ||
                       std::is_convertible_v<T, std::string_view> ||
                       std::is_same_v<std::remove_cvref_t<T>, const char*>;
    
    // Render HTML tag to string
    template<has_render T>
    auto render_html(const T& tag) -> std::string {
        protoflow::html::render_ctx ctx;
        tag.render(ctx);
        return ctx.take_result();
    }
    
    // Render data to HTML (default simple renderer)
    template<typename T>
    auto render_data_as_html(const T& data) -> std::string {
        
        // Create a simple data display using JSON in a pre-formatted div
        return "<div class=\"data\">" + json::to_json(data) + "</div>";
    }
    
    // Render data to JSON
    template<typename T>
    auto render_data_as_json(const T& data) -> std::string {
        return json::to_json(data);
    }
}

// Handler wrapper that supports content negotiation
template<typename Func>
auto make_handler(Func&& func) -> handler_func {
    return [func = std::forward<Func>(func)](const request& req) -> response {
        auto result = func(req);
        
        using R = decltype(result);
        
        if constexpr (detail::is_response<R>) {
            // Handler returns response directly
            return result;
        } else if constexpr (detail::has_render<R>) {
            // Handler returns HTML tag
            return ok(detail::render_html(result))
                .content_type("text/html; charset=utf-8");
        } else if constexpr (detail::is_string<R>) {
            // Handler returns string
            return ok(std::string(result))
                .content_type("text/plain; charset=utf-8");
        } else {
            // Handler returns data - use content negotiation
            auto content_type = req.preferred_content_type();
            
            if (content_type == content_type::json) {
                return ok(detail::render_data_as_json(result))
                    .content_type("application/json; charset=utf-8");
            } else {
                // Default to HTML
                return ok(detail::render_data_as_html(result))
                    .content_type("text/html; charset=utf-8");
            }
        }
    };
}

// Handler wrapper for no-argument functions
template<typename Func>
auto make_handler_no_args(Func&& func) -> handler_func {
    return [func = std::forward<Func>(func)](const request& req) -> response {
        auto result = func();
        
        using R = decltype(result);
        
        if constexpr (detail::is_response<R>) {
            return result;
        } else if constexpr (detail::has_render<R>) {
            return ok(detail::render_html(result))
                .content_type("text/html; charset=utf-8");
        } else if constexpr (detail::is_string<R>) {
            return ok(std::string(result))
                .content_type("text/plain; charset=utf-8");
        } else {
            auto content_type = req.preferred_content_type();
            
            if (content_type == content_type::json) {
                return ok(detail::render_data_as_json(result))
                    .content_type("application/json; charset=utf-8");
            } else {
                return ok(detail::render_data_as_html(result))
                    .content_type("text/html; charset=utf-8");
            }
        }
    };
}

} // namespace protoflow::http
