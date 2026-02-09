#include <protoflow/config/ini_parser.hpp>
#include <fstream>
#include <sstream>

namespace protoflow::config {

namespace {
    std::string trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
}

std::expected<IniDocument, IniError>
parse_ini_string(const std::string& content) {
    IniDocument document;
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Remove comments
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        line = trim(line);
        if (line.empty()) continue;

        // Section header: [section]
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            if (!document.contains(current_section)) {
                document[current_section] = IniSection{};
            }
            continue;
        }

        // Key-value pair: key = value
        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));
            
            if (current_section.empty()) {
                // Global properties (no section)
                current_section = "";
                if (!document.contains("")) {
                    document[""] = IniSection{};
                }
            }
            
            document[current_section][key] = value;
        }
    }

    return document;
}

std::expected<IniDocument, IniError>
parse_ini_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected(IniError::file_not_found);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parse_ini_string(buffer.str());
}

} // namespace protoflow::config