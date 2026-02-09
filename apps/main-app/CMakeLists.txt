# Main application
# Runs as root, provides hardware arbitration and app registration

add_executable(protoflow-main-app)

target_sources(protoflow-main-app
    PRIVATE
        src/main.cpp
        src/runtime.cpp
        src/services/app_registration_service.cpp
        src/services/hardware_arbitration_service.cpp
        src/services/http_service.cpp
)

target_include_directories(protoflow-main-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(protoflow-main-app
    PRIVATE
        protoflow::runtime
        protoflow::messaging
        protoflow::logging
        protoflow::service
        protoflow::fsm
        protoflow::rpc
        protoflow::hw-client
        protoflow::transport-tcp
        protoflow::html-fragment
        protoflow::http
        nlohmann_json::nlohmann_json
)

target_compile_features(protoflow-main-app PRIVATE cxx_std_23)

# Install
install(TARGETS protoflow-main-app
    RUNTIME DESTINATION bin
)

# Install config files
install(FILES
    config/hardware.conf
    DESTINATION /etc/protoflow
)
