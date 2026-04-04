#include <protoflow/config/config_paths.hpp>

#include <cstdlib>

namespace protoflow::config {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Platform helpers
// ---------------------------------------------------------------------------

namespace {

/// Get an environment variable, returning nullopt if unset or empty.
std::optional<std::string> get_env(const char* name) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe) – env access is inherently racy;
    // callers are expected to read config early in main().
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string{value};
}

/// Home directory of the current user.
std::optional<fs::path> home_directory() {
#ifdef _WIN32
    if (auto p = get_env("USERPROFILE")) return fs::path{*p};
    // Fallback: HOMEDRIVE + HOMEPATH
    auto drive = get_env("HOMEDRIVE");
    auto hpath = get_env("HOMEPATH");
    if (drive && hpath) return fs::path{*drive + *hpath};
    return std::nullopt;
#else
    if (auto p = get_env("HOME")) return fs::path{*p};
    return std::nullopt;
#endif
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<fs::path>
config_directory_candidates(const std::string& app_name) {
    std::vector<fs::path> dirs;

#ifdef _WIN32
    // Windows: %APPDATA%\protoflow\<app>
    if (auto appdata = get_env("APPDATA")) {
        dirs.emplace_back(fs::path{*appdata} / "protoflow" / app_name);
    }
    // Fallback: %LOCALAPPDATA%\protoflow\<app>
    if (auto local = get_env("LOCALAPPDATA")) {
        dirs.emplace_back(fs::path{*local} / "protoflow" / app_name);
    }
#else
    // 1. User-level: $XDG_CONFIG_HOME/protoflow/<app>
    if (auto xdg = get_env("XDG_CONFIG_HOME")) {
        dirs.emplace_back(fs::path{*xdg} / "protoflow" / app_name);
    } else if (auto home = home_directory()) {
        dirs.emplace_back(*home / ".config" / "protoflow" / app_name);
    }

    // 2. System-level: /etc/protoflow/<app>
    dirs.emplace_back(fs::path{"/etc/protoflow"} / app_name);
#endif

    return dirs;
}

std::vector<fs::path>
config_directories(const std::string& app_name) {
    auto candidates = config_directory_candidates(app_name);
    std::vector<fs::path> result;
    result.reserve(candidates.size());
    for (auto& dir : candidates) {
        std::error_code ec;
        if (fs::is_directory(dir, ec)) {
            result.push_back(std::move(dir));
        }
    }
    return result;
}

std::optional<fs::path>
find_config_file(const std::string& app_name, const std::string& filename) {
    for (auto& dir : config_directory_candidates(app_name)) {
        auto path = dir / filename;
        std::error_code ec;
        if (fs::is_regular_file(path, ec)) {
            return path;
        }
    }
    return std::nullopt;
}

std::vector<fs::path>
find_all_config_files(const std::string& app_name, const std::string& filename) {
    std::vector<fs::path> result;
    for (auto& dir : config_directory_candidates(app_name)) {
        auto path = dir / filename;
        std::error_code ec;
        if (fs::is_regular_file(path, ec)) {
            result.push_back(std::move(path));
        }
    }
    return result;
}

std::optional<fs::path>
user_config_directory(const std::string& app_name, bool create) {
    fs::path dir;

#ifdef _WIN32
    if (auto appdata = get_env("APPDATA")) {
        dir = fs::path{*appdata} / "protoflow" / app_name;
    } else {
        return std::nullopt;
    }
#else
    if (auto xdg = get_env("XDG_CONFIG_HOME")) {
        dir = fs::path{*xdg} / "protoflow" / app_name;
    } else if (auto home = home_directory()) {
        dir = *home / ".config" / "protoflow" / app_name;
    } else {
        return std::nullopt;
    }
#endif

    if (create) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            return std::nullopt;
        }
    }

    return dir;
}

fs::path
system_config_directory(const std::string& app_name) {
#ifdef _WIN32
    // On Windows there is no separate system-level path; reuse user-level.
    if (auto appdata = get_env("APPDATA")) {
        return fs::path{*appdata} / "protoflow" / app_name;
    }
    return fs::path{"C:/ProgramData/protoflow"} / app_name;
#else
    return fs::path{"/etc/protoflow"} / app_name;
#endif
}

} // namespace protoflow::config
