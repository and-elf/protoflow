// Copyright (c) 2026 Andreas
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

namespace protoflow::http {

enum class method {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

constexpr auto to_string(method m) -> std::string_view {
    switch (m) {
        case method::GET:     return "GET";
        case method::POST:    return "POST";
        case method::PUT:     return "PUT";
        case method::DELETE:  return "DELETE";
        case method::PATCH:   return "PATCH";
        case method::HEAD:    return "HEAD";
        case method::OPTIONS: return "OPTIONS";
    }
    return "UNKNOWN";
}

constexpr auto from_string(std::string_view str) -> std::optional<method> {
    if (str == "GET")     return method::GET;
    if (str == "POST")    return method::POST;
    if (str == "PUT")     return method::PUT;
    if (str == "DELETE")  return method::DELETE;
    if (str == "PATCH")   return method::PATCH;
    if (str == "HEAD")    return method::HEAD;
    if (str == "OPTIONS") return method::OPTIONS;
    return std::nullopt;
}

enum class content_type {
    html,
    json,
    text,
    unknown
};

class request {
public:
    request() = default;
    
    request(method m, std::string path)
        : method_(m), path_(std::move(path)) {}
    
    auto get_method() const -> method { return method_; }
    auto get_path() const -> std::string_view { return path_; }
    
    auto header(std::string_view name) const -> std::optional<std::string_view> {
        auto it = headers_.find(std::string(name));
        if (it != headers_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    auto set_header(std::string name, std::string value) -> request& {
        headers_[std::move(name)] = std::move(value);
        return *this;
    }
    
    auto query(std::string_view name) const -> std::optional<std::string_view> {
        auto it = query_params_.find(std::string(name));
        if (it != query_params_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    auto set_query(std::string name, std::string value) -> request& {
        query_params_[std::move(name)] = std::move(value);
        return *this;
    }
    
    auto path_param(std::string_view name) const -> std::optional<std::string_view> {
        auto it = path_params_.find(std::string(name));
        if (it != path_params_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    auto set_path_param(std::string name, std::string value) -> request& {
        path_params_[std::move(name)] = std::move(value);
        return *this;
    }
    
    auto body() const -> std::string_view { return body_; }
    auto set_body(std::string body) -> request& {
        body_ = std::move(body);
        return *this;
    }
    
    // Content negotiation
    auto accepts_html() const -> bool {
        auto accept = header("Accept");
        if (!accept) return false;
        
        auto accept_str = *accept;
        return accept_str.find("text/html") != std::string_view::npos ||
               accept_str.find("*/*") != std::string_view::npos ||
               accept_str.find("application/xhtml+xml") != std::string_view::npos;
    }
    
    auto accepts_json() const -> bool {
        auto accept = header("Accept");
        if (!accept) return false;
        
        return accept->find("application/json") != std::string_view::npos;
    }
    
    auto preferred_content_type() const -> content_type {
        auto accept = header("Accept");
        if (!accept) return content_type::html;
        
        auto accept_str = *accept;
        
        // Check in priority order
        if (accept_str.find("application/json") != std::string_view::npos) {
            return content_type::json;
        }
        if (accept_str.find("text/html") != std::string_view::npos ||
            accept_str.find("application/xhtml+xml") != std::string_view::npos) {
            return content_type::html;
        }
        if (accept_str.find("text/plain") != std::string_view::npos) {
            return content_type::text;
        }
        if (accept_str.find("*/*") != std::string_view::npos) {
            return content_type::html;
        }
        
        return content_type::unknown;
    }

private:
    method method_ = method::GET;
    std::string path_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> query_params_;
    std::unordered_map<std::string, std::string> path_params_;
    std::string body_;
};

} // namespace protoflow::http
