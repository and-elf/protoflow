# Skeleton client application
# Template/reference for implementing protoflow client apps

# Fetch CLI11 (may already be available from main-app)
include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(CLI11)

protoflow_add_app(
    NAME skeleton-app
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Protoflow skeleton client application - template for new apps"
    USER protoflow
    MAINTAINER "Protoflow Project <protoflow@example.com>"

    SOURCES
        src/main.cpp
        src/app.cpp

    LINK_LIBRARIES
        protoflow::service
        protoflow::messaging
        protoflow::logging
        protoflow::config
        protoflow::fsm
        protoflow::rpc
        protoflow::rpc-client
        protoflow::rpc-service
        protoflow::transport-tcp
        protoflow::app-registration-protocol
        protoflow::app-registration-client
        protoflow::hw-client
        protoflow::hw-protocol
        protoflow::html-fragment
        protoflow::http
        CLI11::CLI11
)

target_include_directories(protoflow-skeleton-app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
