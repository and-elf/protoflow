# protoflow-http library
# HTTP service with runtime endpoint registration

# Fetch nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

protoflow_add_library(
    NAME http
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "HTTP service library"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::service
        protoflow::html-fragment
)

# Add nlohmann/json include directory manually to avoid export issues
target_include_directories(protoflow-http INTERFACE
    $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)
