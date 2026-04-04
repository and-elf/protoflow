#pragma once

#include "../http_service.hpp"
#include <optional>
#include <vector>

namespace protoflow::mainapp::handlers {

/// Handler for GET /static/* endpoints
HttpResponse handle_static(const HTTPService& service, const HttpRequest& request);

/// Read a file from the static directory
std::optional<std::vector<std::byte>> read_static_file(const std::string& static_dir,
                                                       const std::string& filename);

} // namespace protoflow::mainapp::handlers
