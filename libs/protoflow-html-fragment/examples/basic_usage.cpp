#include <protoflow/html.hpp>
#include <iostream>

using namespace protoflow::html;

// Example: Create a simple status display
auto create_greeting() {
    return div(text("Hello World"));
}

// Example: Create a status card
auto create_card() {
    return div(
        attrs<class_<"card">>{},
        h1(text("Dashboard")),
        p(text("System Status: Online"))
    );
}

// Example: Create a menu widget
auto create_menu() {
    return div(
        attrs<class_<"menu">>{},
        ul(
            li(a(attrs<href_<"/">>{}, text("Home"))),
            li(a(attrs<href_<"/about">>{}, text("About"))),
            li(a(attrs<href_<"/contact">>{}, text("Contact")))
        )
    );
}

// Example: Create a data table
auto create_table() {
    return table(
        attrs<class_<"data-table">>{},
        thead(
            tr(
                th(text("Name")),
                th(text("Status")),
                th(text("Value"))
            )
        ),
        tbody(
            tr(
                td(text("Sensor 1")),
                td(span(attrs<class_<"badge-ok">>{}, text("OK"))),
                td(text("23.5°C"))
            ),
            tr(
                td(text("Sensor 2")),
                td(span(attrs<class_<"badge-ok">>{}, text("OK"))),
                td(text("45.2%"))
            )
        )
    );
}

// Example: Create a process status dashboard
auto create_dashboard() {
    return div(
        attrs<class_<"dashboard">>{},
        
        // Title
        div(
            attrs<class_<"header">>{},
            h1(text("Process Monitor")),
            create_menu()
        ),
        
        // Main content area
        div(
            attrs<class_<"content">>{},
            
            // Status card
            div(
                attrs<class_<"section">>{},
                h2(text("System Status")),
                create_card()
            ),
            
            // Data table
            div(
                attrs<class_<"section">>{},
                h2(text("Process Data")),
                create_table()
            )
        ),
        
        // Status bar
        div(
            attrs<class_<"footer">>{},
            p(text("© 2026 Protoflow Application"))
        )
    );
}

int main() {
    // Simple example
    std::cout << "=== Simple Greeting ===" << std::endl;
    std::cout << to_html(create_greeting()) << std::endl << std::endl;
    
    // Card example
    std::cout << "=== Status Card ===" << std::endl;
    std::cout << to_html(create_card()) << std::endl << std::endl;
    
    // Menu example
    std::cout << "=== Menu Widget ===" << std::endl;
    std::cout << to_html(create_menu()) << std::endl << std::endl;
    
    // Table example
    std::cout << "=== Data Table ===" << std::endl;
    std::cout << to_html(create_table()) << std::endl << std::endl;
    
    // Complete dashboard
    std::cout << "=== Process Dashboard ===" << std::endl;
    std::cout << to_html(create_dashboard()) << std::endl << std::endl;
    
    // Fragment example
    std::cout << "=== Fragment Example ===" << std::endl;
    auto frag = make_fragment(div(
        attrs<id_<"dynamic">>{},
        text("This is a fragment")
    ));
    std::cout << frag->render() << std::endl;
    
    return 0;
}
