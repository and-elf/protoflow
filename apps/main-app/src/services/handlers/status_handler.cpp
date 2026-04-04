#include "services/handlers/status_handler.hpp"
#include <protoflow/logging/macros.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

using json = nlohmann::json;

namespace protoflow::mainapp::handlers {

// Helper function to read system file content
static std::string read_system_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper function to get CPU load average
static json get_cpu_load() {
    json cpu_load = json::object();
    std::string loadavg_content = read_system_file("/proc/loadavg");
    if (!loadavg_content.empty()) {
        std::istringstream iss(loadavg_content);
        double load1, load5, load15;
        if (iss >> load1 >> load5 >> load15) {
            cpu_load["1min"] = load1;
            cpu_load["5min"] = load5;
            cpu_load["15min"] = load15;
        }
    }
    return cpu_load;
}

// Helper function to get RAM usage
static json get_ram_usage() {
    json ram = json::object();
    std::string meminfo_content = read_system_file("/proc/meminfo");
    if (!meminfo_content.empty()) {
        std::istringstream iss(meminfo_content);
        std::string line;
        unsigned long mem_total = 0, mem_available = 0, mem_free = 0;
        
        while (std::getline(iss, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream line_iss(line);
                std::string label;
                line_iss >> label >> mem_total;
            } else if (line.find("MemAvailable:") == 0) {
                std::istringstream line_iss(line);
                std::string label;
                line_iss >> label >> mem_available;
            } else if (line.find("MemFree:") == 0) {
                std::istringstream line_iss(line);
                std::string label;
                line_iss >> label >> mem_free;
            }
        }
        
        if (mem_total > 0) {
            ram["total_kb"] = mem_total;
            ram["available_kb"] = mem_available;
            ram["free_kb"] = mem_free;
            ram["used_kb"] = mem_total - mem_available;
            ram["used_percent"] = (static_cast<double>(mem_total - mem_available) * 100.0 / static_cast<double>(mem_total));
        }
    }
    return ram;
}

// Helper function to get IP configuration
static json get_ip_configuration() {
    json interfaces = json::array();
    
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            
            json iface = json::object();
            iface["name"] = std::string(ifa->ifa_name);
            
            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET) {
                char ip_str[INET_ADDRSTRLEN];
                struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
                iface["ipv4"] = std::string(ip_str);
                
                if (ifa->ifa_netmask != nullptr) {
                    struct sockaddr_in* netmask = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask);
                    inet_ntop(AF_INET, &netmask->sin_addr, ip_str, sizeof(ip_str));
                    iface["netmask"] = std::string(ip_str);
                }
            } else if (family == AF_INET6) {
                char ip_str[INET6_ADDRSTRLEN];
                struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
                inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, sizeof(ip_str));
                iface["ipv6"] = std::string(ip_str);
            }
            
            if (!iface.contains("ipv4") && !iface.contains("ipv6")) {
                continue;
            }
            
            interfaces.push_back(iface);
        }
        freeifaddrs(ifaddr);
    }
    
    json ip_config = json::object();
    ip_config["interfaces"] = interfaces;
    return ip_config;
}

// Helper function to get mounted devices
static json get_mounted_devices() {
    json devices = json::array();
    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string device, mount_point, fstype;
            if (iss >> device >> mount_point >> fstype) {
                json dev = json::object();
                dev["device"] = device;
                dev["mount_point"] = mount_point;
                dev["filesystem"] = fstype;
                devices.push_back(dev);
            }
        }
    }
    json mounted = json::object();
    mounted["mounts"] = devices;
    return mounted;
}

// Helper function to get hardware configuration
static json get_hardware_configuration() {
    json hw = json::object();
    
    // CPU info
    std::string cpuinfo = read_system_file("/proc/cpuinfo");
    if (!cpuinfo.empty()) {
        json cpu = json::object();
        std::istringstream iss(cpuinfo);
        std::string line;
        int core_count = 0;
        std::string model_name;
        
        while (std::getline(iss, line)) {
            if (line.find("processor") == 0) {
                core_count++;
            } else if (line.find("model name") == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    model_name = line.substr(colon_pos + 2);
                    // Remove trailing whitespace
                    model_name.erase(model_name.find_last_not_of(" \n\r\t") + 1);
                }
            }
        }
        
        cpu["cores"] = core_count;
        if (!model_name.empty()) {
            cpu["model"] = model_name;
        }
        hw["cpu"] = cpu;
    }
    
    // System hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hw["hostname"] = std::string(hostname);
    }
    
    return hw;
}

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
    
    // Add system information
    j["system"] = json::object();
    j["system"]["hardware"] = get_hardware_configuration();
    j["system"]["cpu_load"] = get_cpu_load();
    j["system"]["ram"] = get_ram_usage();
    j["system"]["ip_configuration"] = get_ip_configuration();
    j["system"]["mounted_devices"] = get_mounted_devices();
    
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
    
    // Add system information
    j["system"] = json::object();
    j["system"]["hardware"] = get_hardware_configuration();
    j["system"]["cpu_load"] = get_cpu_load();
    j["system"]["ram"] = get_ram_usage();
    j["system"]["ip_configuration"] = get_ip_configuration();
    j["system"]["mounted_devices"] = get_mounted_devices();
    
    j["timestamp"] = "";

    response.set_json(j.dump());
    return response;
}

} // namespace protoflow::mainapp::handlers
