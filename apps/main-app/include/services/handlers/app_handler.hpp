#pragma once

#include "../http_service.hpp"

namespace protoflow::mainapp::handlers {

/// Handler for GET /app/* endpoints (app overview and fragments)
HttpResponse handle_app_endpoint(const HTTPService& service, const HttpRequest& request);

/// Handler for app fragments
HttpResponse handle_fragment(const HTTPService& service, 
                            const std::string& app_name,
                            const std::string& fragment_id);

} // namespace protoflow::mainapp::handlers
