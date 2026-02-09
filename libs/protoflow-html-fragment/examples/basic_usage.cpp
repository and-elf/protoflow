#include <protoflow/html.hpp>
#include <iostream>

using namespace protoflow::html;

// Example: Using semantic components
auto create_status_display() {
    return container(
        section_with_title(
            h2(text("System Status")),
            status_ok(text("All systems operational")),
            metric(text("CPU Usage"), text("23%"), text(""))
        )
    );
}

// Example: Create a simple status display
auto create_greeting() {
    return div(text("Hello World"));
}

// Example: Create a status card using components
auto create_card() {
    return card(
        h3(text("Process Monitor")),
        data_row(text("Status:"), badge_success(text("Running"))),
        data_row(text("Uptime:"), text("48h 32m"))
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

// Example: Create a data table with semantic components
auto create_table() {
    return table(
        attrs<class_<"data-table">>{},
        thead(
            table_row(
                table_header_cell(text("Process")),
                table_header_cell(text("Status")),
                table_header_cell(text("CPU"))
            )
        ),
        tbody(
            table_row(
                table_data_cell(text("nginx")),
                table_data_cell(badge_success(text("OK"))),
                table_data_cell(text("2.3%"))
            ),
            table_row(
                table_data_cell(text("postgres")),
                table_data_cell(badge_success(text("OK"))),
                table_data_cell(text("5.7%"))
            ),
            table_row(
                table_data_cell(text("redis")),
                table_data_cell(badge_warning(text("HIGH MEM"))),
                table_data_cell(text("1.2%"))
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
    // Component example
    std::cout << "=== Status Display (Components) ===" << std::endl;
    std::cout << to_html(create_status_display()) << std::endl << std::endl;
    
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
