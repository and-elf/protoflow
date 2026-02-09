# Main application
# Runs as root, provides hardware arbitration and app registration

add_executable(protoflow-main-app)

target_sources(protoflow-main-app
    PRIVATE
        # To be added
        # src/main.cpp
        # src/app_registration_service.cpp
        # src/hardware_arbitration_service.cpp
        # src/http_server.cpp
)

target_link_libraries(protoflow-main-app
    PRIVATE
        protoflow::runtime
        protoflow::messaging
        protoflow::logging
        protoflow::fsm
        protoflow::rpc
        protoflow::transport-tcp
        protoflow::html-fragment
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
