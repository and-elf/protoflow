#include "services/handlers/app_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <protoflow/html.hpp>

namespace protoflow::mainapp::handlers {

HttpResponse handle_app_endpoint(const HTTPService& service, const HttpRequest& request) {
    using namespace html;

    // Parse path: /app/{name}/fragment/{id} or /app/{name}/...
    std::string_view path = request.path;
    auto rest = path.substr(5);  // skip "/app/"

    auto slash = rest.find('/');
    std::string app_name;
    std::string_view remainder;

    if (slash == std::string_view::npos) {
        app_name = std::string(rest);
    } else {
        app_name = std::string(rest.substr(0, slash));
        remainder = rest.substr(slash + 1);
    }

    if (app_name.empty()) {
        HttpResponse response;
        response.status_code = 302;
        response.headers["Location"] = "/";
        return response;
    }

    // Fragment request: /app/{name}/fragment/{id}
    if (remainder.starts_with("fragment/") && remainder.size() > 9) {
        std::string fragment_id(remainder.substr(9));
        return handle_fragment(service, app_name, fragment_id);
    }

    // App overview
    const auto& apps = service.get_registered_apps();
    auto it = apps.find(app_name);
    if (it == apps.end()) {
        HttpResponse response;
        response.status_code = 404;
        response.set_html(to_html(
            card(
                h3(text("Not Found")),
                status_error(text("Application '" + app_name + "' is not registered"))
            )
        ));
        return response;
    }

    HttpResponse response;
    response.set_html(to_html(
        container(
            card(
                h3(text(app_name)),
                data_row(text("Endpoints:"),
                         text(std::to_string(it->second.size()))),
                data_row(text("Status:"),
                         badge_success(text("Registered")))
            )
        )
    ));
    return response;
}

HttpResponse handle_fragment(const HTTPService& service,
                            const std::string& app_name,
                            const std::string& fragment_id) {
    using namespace html;

    const auto& apps = service.get_registered_apps();
    auto it = apps.find(app_name);
    if (it == apps.end()) {
        HttpResponse response;
        response.status_code = 404;
        response.set_html(to_html(
            card(
                h3(text("Not Found")),
                status_error(text("Application '" + app_name + "' is not registered"))
            )
        ));
        return response;
    }

    // Build endpoint list items using html-fragment
    std::string ep_items;
    for (const auto& ep : it->second) {
        ep_items += to_html(li(code(text(ep))));
    }

    // Fragment info card (html-fragment semantic components)
    std::string info_html = to_html(
        card(
            h3(text(app_name + " \u2014 " + fragment_id)),
            data_row(text("Application:"), badge_success(text(app_name))),
            data_row(text("Fragment ID:"),  badge_info(text(fragment_id))),
            data_row(text("Endpoints:"),    text(std::to_string(it->second.size())))
        )
    );

    // Note about RPC forwarding
    std::string note_html = to_html(
        card_simple(
            status_pending(
                text("This is a placeholder fragment rendered by the main app's "
                     "HTTPService using the html-fragment library. In production, "
                     "fragment requests are forwarded to '" + app_name + "' via "
                     "RPC (cmd::render_fragment) and the app returns its own "
                     "html-fragment output.")
            )
        )
    );

    // Endpoint list card
    std::string ep_header = to_html(h3(text("Registered Endpoints")));
    std::string ep_html =
        "<div class=\"card\"><div class=\"card-header\">" +
        ep_header +
        "</div><div class=\"card-body\"><ul>" +
        ep_items +
        "</ul></div></div>";

    HttpResponse response;
    response.set_html(
        "<div class=\"container\">" +
        info_html + note_html + ep_html +
        "</div>"
    );
    return response;
}

} // namespace protoflow::mainapp::handlers
