#include "services/handlers/home_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <protoflow/html.hpp>

namespace protoflow::mainapp::handlers {

HttpResponse handle_home(const HTTPService& service, const HttpRequest& /*request*/) {
    using namespace html;

    // ── Build navigation items HTML ──
    std::string nav_items_html;
    const auto& apps = service.get_registered_apps();
    if (apps.empty()) {
        nav_items_html = to_html(
            li(attrs<class_<"nav-item">>{},
               em(text("No apps registered")))
        );
    } else {
        for (const auto& [name, endpoints] : apps) {
            nav_items_html +=
                "<li class=\"nav-item\">"
                "<a class=\"nav-link\" href=\"#\" data-app=\"" + name + "\">" +
                name + "</a></li>";
        }
    }

    // ── Build header using html-fragment ──
    auto header = div(attrs<class_<"header">>{},
        h1(text("Protoflow Dashboard")),
        badge_info(text("v0.1"))
    );

    // ── Build welcome content ──
    auto welcome = container(
        section_with_title(
            h2(text("System Overview")),
            card(
                h3(text("Welcome to Protoflow")),
                p(text("Select an application from the sidebar to view "
                       "its status fragment.")),
                data_row(
                    text("Registered apps:"),
                    badge_info(text(std::to_string(apps.size())))
                ),
                data_row(
                    text("Server status:"),
                    status_ok(text("Running"))
                )
            )
        )
    );

    // ── Assemble HTML document ──
    std::string page =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <title>Protoflow Dashboard</title>\n"
        "  <link rel=\"stylesheet\" href=\"/static/styles.css\">\n"
        "</head>\n"
        "<body>\n"
        + to_html(header) +
        "  <div class=\"layout\">\n"
        "    <nav class=\"sidebar\">\n"
        "      <h2>Applications</h2>\n"
        "      <ul class=\"nav-list\">\n"
        + nav_items_html +
        "      </ul>\n"
        "    </nav>\n"
        "    <main class=\"main-content\" id=\"main-content\">\n"
        + to_html(welcome) +
        "    </main>\n"
        "  </div>\n"
        "  <script>\n"
        R"js(
async function loadFragment(appName, fragmentId) {
    var main = document.getElementById('main-content');
    main.innerHTML = '<div class="loading">Loading fragment…</div>';

    document.querySelectorAll('.nav-link').forEach(function(el) {
        el.classList.remove('active');
    });
    var active = document.querySelector('[data-app="' + appName + '"]');
    if (active) active.classList.add('active');

    try {
        var resp = await fetch('/app/' + appName + '/fragment/' + fragmentId);
        if (resp.ok) {
            main.innerHTML = await resp.text();
        } else {
            main.innerHTML = '<div class="error-box">Failed to load fragment (HTTP ' + resp.status + ')</div>';
        }
    } catch (e) {
        main.innerHTML = '<div class="error-box">Network error: ' + e.message + '</div>';
    }
}

// Set up click handlers for nav links
document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('.nav-link').forEach(function(link) {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            var appName = this.getAttribute('data-app');
            if (appName) {
                loadFragment(appName, 'status');
            }
        });
    });
});
)js"
        "  </script>\n"
        "</body>\n"
        "</html>";

    HttpResponse response;
    response.set_html(page);
    return response;
}

} // namespace protoflow::mainapp::handlers
