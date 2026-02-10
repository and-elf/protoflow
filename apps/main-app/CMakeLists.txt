# Main application
# Runs as root, provides hardware arbitration and app registration

protoflow_add_app(
    NAME main-app
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Protoflow main daemon - hardware arbitration and app registration"
    USER root
    MAINTAINER "Protoflow Project <protoflow@example.com>"
    
    SOURCES
        src/main.cpp
        src/runtime.cpp
        src/services/app_registration_service.cpp
        src/services/hardware_arbitration_service.cpp
        src/services/http_service.cpp
        src/services/rpc_server_service.cpp
    
    LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
        protoflow::logging
        protoflow::service
        protoflow::fsm
        protoflow::rpc
        protoflow::rpc-service
        protoflow::hw-client
        protoflow::transport-tcp
        protoflow::html-fragment
        protoflow::http
        protoflow::config
        nlohmann_json::nlohmann_json
)

# Include directory for private headers
target_include_directories(protoflow-main-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Install config files
install(FILES
    config/hardware.conf
    DESTINATION /etc/protoflow
)
