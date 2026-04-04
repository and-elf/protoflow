#pragma once

#include "../http_service.hpp"

namespace protoflow::mainapp::handlers {

/// Handler for GET /api/state endpoint
HttpResponse handle_state_api(const HTTPService& service, const HttpRequest& request);

} // namespace protoflow::mainapp::handlers
