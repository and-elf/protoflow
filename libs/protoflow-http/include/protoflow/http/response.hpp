// Copyright (c) 2026 Andreas
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace protoflow::http {

class response {
public:
    response() = default;
    
    explicit response(int status_code)
        : status_code_(status_code) {}
    
    response(int status_code, std::string body)
        : status_code_(status_code), body_(std::move(body)) {}
    
    auto status(int code) -> response& {
        status_code_ = code;
        return *this;
    }
    
    auto get_status() const -> int { return status_code_; }
    
    auto header(std::string name, std::string value) -> response& {
        headers_[std::move(name)] = std::move(value);
        return *this;
    }
    
    auto get_header(std::string_view name) const -> std::string_view {
        auto it = headers_.find(std::string(name));
        if (it != headers_.end()) {
            return it->second;
        }
        return "";
    }
    
    auto content_type(std::string_view type) -> response& {
        headers_["Content-Type"] = std::string(type);
        return *this;
    }
    
    auto body(std::string content) -> response& {
        body_ = std::move(content);
        return *this;
    }
    
    auto get_body() const -> std::string_view { return body_; }
    
    auto get_headers() const -> const std::unordered_map<std::string, std::string>& {
        return headers_;
    }

private:
    int status_code_ = 200;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

// Helper functions for common responses
inline auto ok() -> response {
    return response(200);
}

inline auto ok(std::string body) -> response {
    return response(200, std::move(body));
}

inline auto created() -> response {
    return response(201);
}

inline auto created(std::string body) -> response {
    return response(201, std::move(body));
}

inline auto no_content() -> response {
    return response(204);
}

inline auto bad_request(std::string message = "Bad Request") -> response {
    return response(400, std::move(message));
}

inline auto not_found(std::string message = "Not Found") -> response {
    return response(404, std::move(message));
}

inline auto internal_error(std::string message = "Internal Server Error") -> response {
    return response(500, std::move(message));
}

} // namespace protoflow::http
