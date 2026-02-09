#pragma once

#include <protoflow/messaging/types.hpp>
#include <protoflow/html/element.hpp>
#include <string>
#include <optional>
#include <vector>

namespace protoflow::mainapp {

/// Message: Request navigation data for building UI menus
struct NavigationRequest {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // For matching responses
};

/// Message: Response with navigation data
struct NavigationResponse {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // Matches the request
    std::string html_fragment;  // Rendered navigation HTML
};

/// Message: Request system state aggregation
struct StateRequest {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // For matching responses
    enum class Format { JSON, HTML } format;
};

/// Message: Response with system state
struct StateResponse {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // Matches the request
    std::string content;  // JSON or HTML based on request
};

/// Message: Request HTML fragment from a registered app
struct FragmentRequest {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // For matching responses
    std::string app_name;
    std::string endpoint;
};

/// Message: Response with HTML fragment
struct FragmentResponse {
    messaging::ServiceId requester_id;
    uint64_t request_id;  // Matches the request
    std::string app_name;
    std::string endpoint;
    std::optional<std::string> html_fragment;  // nullopt if app/endpoint not found
    int status_code = 200;  // HTTP status code
};

} // namespace protoflow::mainapp
