#include "services/handlers/status_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace protoflow::mainapp::handlers {

HttpResponse handle_status(const HTTPService& service, const HttpRequest& /*request*/) {
    HttpResponse response;

    // Build JSON response using nlohmann::json
    json j = json::object();
    j["status"] = "running";
    
    json apps = json::array();
    for (const auto& [name, endpoints] : service.get_registered_apps()) {
        json app = json::object();
        app["name"] = name;
        app["endpoints"] = endpoints.size();
        apps.push_back(app);
    }
    
    j["registered_apps"] = apps;
    j["timestamp"] = "";

    response.set_json(j.dump());
    return response;
}
HttpResponse handle_status_async(const HTTPService& service, const HttpRequest& /*request*/) {
    HttpResponse response;

    // Build JSON response with extended app info using nlohmann::json
    json j = json::object();
    j["status"] = "running";
    
    json apps = json::array();
    for (const auto& [name, info] : service.get_registered_apps_info()) {
        json app = json::object();
        app["name"] = name;
        app["endpoints"] = info.endpoints.size();
        app["http_listener"] = info.http_listener;
        apps.push_back(app);
    }
    
    j["registered_apps"] = apps;
    j["timestamp"] = "";

    response.set_json(j.dump());
    return response;
}

} // namespace protoflow::mainapp::handlers
