#include "services/handlers/state_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <protoflow/html.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace protoflow::mainapp::handlers {

HttpResponse handle_state_api(const HTTPService& service, const HttpRequest& request) {
    HttpResponse response;

    bool accepts_json = false;
    bool accepts_html = false;
    
    auto it = request.headers.find("Accept");
    if (it != request.headers.end()) {
        accepts_json = it->second.find("application/json") != std::string::npos;
        accepts_html = it->second.find("text/html") != std::string::npos;
    }

    if (accepts_json || !accepts_html) {
        // Return JSON response using nlohmann::json
        json j = json::object();
        json apps = json::array();
        
        for (const auto& [name, eps] : service.get_registered_apps()) {
            json app = json::object();
            app["name"] = name;
            app["endpoints"] = eps.size();
            apps.push_back(app);
        }
        
        j["registered_apps"] = apps;
        response.set_json(j.dump());
    } else {
        // Return HTML response using html-fragment
        using namespace html;
        std::string body_html = to_html(
            container(
                card(
                    h3(text("System State")),
                    data_row(
                        text("Registered applications:"),
                        badge_info(text(std::to_string(service.get_registered_apps().size())))
                    ),
                    data_row(
                        text("Status:"),
                        status_ok(text("Running"))
                    )
                )
            )
        );
        response.set_html(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<title>System State</title></head><body>"
            + body_html +
            "</body></html>"
        );
    }

    return response;
}

} // namespace protoflow::mainapp::handlers
