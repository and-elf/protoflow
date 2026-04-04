#pragma once

#include "../http_service.hpp"

namespace protoflow::mainapp::handlers {

/// Handler for GET /status endpoint
HttpResponse handle_status(const HTTPService& service, const HttpRequest& request);

/// Handler for GET /api/status endpoint  
HttpResponse handle_status_async(const HTTPService& service, const HttpRequest& request);

} // namespace protoflow::mainapp::handlers
