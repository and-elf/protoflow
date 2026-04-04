#pragma once

#include "../http_service.hpp"

namespace protoflow::mainapp::handlers {

/// Handler for GET / endpoint (home page)
HttpResponse handle_home(const HTTPService& service, const HttpRequest& request);

} // namespace protoflow::mainapp::handlers
