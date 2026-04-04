#include "services/handlers/static_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace protoflow::mainapp::handlers {

std::optional<std::vector<std::byte>> read_static_file(const std::string& static_dir,
                                                       const std::string& filename) {
    if (static_dir.empty()) {
        return std::nullopt;
    }

    // Prevent path traversal attacks
    if (filename.find("..") != std::string::npos || filename.find("//") != std::string::npos) {
        return std::nullopt;
    }

    std::string filepath = static_dir + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    // Read file into vector
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::nullopt;
    }

    return buffer;
}

HttpResponse handle_static(const HTTPService& service, const HttpRequest& request) {
    HttpResponse response;

    // Extract filename from path: /static/{filename}
    std::string_view path = request.path;
    if (!path.starts_with("/static/")) {
        response.status_code = 400;
        json error = json::object();
        error["error"] = "Invalid static path";
        response.set_json(error.dump());
        return response;
    }

    // Get the filename relative to /static/
    std::string filename(path.substr(8));  // skip "/static/"

    if (filename.empty()) {
        response.status_code = 400;
        json error = json::object();
        error["error"] = "No file specified";
        response.set_json(error.dump());
        return response;
    }

    // Try to read the file
    auto file_data = read_static_file(service.get_static_dir(), filename);
    if (!file_data) {
        response.status_code = 404;
        json error = json::object();
        error["error"] = "Static file not found";
        response.set_json(error.dump());
        return response;
    }

    // Determine content type based on file extension
    std::string content_type = "application/octet-stream";
    if (filename.ends_with(".css")) {
        content_type = "text/css; charset=utf-8";
    } else if (filename.ends_with(".js")) {
        content_type = "application/javascript; charset=utf-8";
    } else if (filename.ends_with(".html")) {
        content_type = "text/html; charset=utf-8";
    } else if (filename.ends_with(".json")) {
        content_type = "application/json; charset=utf-8";
    } else if (filename.ends_with(".png")) {
        content_type = "image/png";
    } else if (filename.ends_with(".jpg") || filename.ends_with(".jpeg")) {
        content_type = "image/jpeg";
    } else if (filename.ends_with(".svg")) {
        content_type = "image/svg+xml";
    } else if (filename.ends_with(".woff")) {
        content_type = "font/woff";
    } else if (filename.ends_with(".woff2")) {
        content_type = "font/woff2";
    }

    response.status_code = 200;
    response.headers["Content-Type"] = content_type;
    response.body = std::move(*file_data);

    return response;
}

} // namespace protoflow::mainapp::handlers
