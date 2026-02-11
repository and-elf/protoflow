cmake_minimum_required(VERSION 3.25)

project(protoflow
    VERSION 0.1.0
    DESCRIPTION "Message-driven service-oriented framework"
    LANGUAGES CXX
)

# C++23 required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd/IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Options
option(ENABLE_COVERAGE "Enable code coverage" ON)
option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
option(ENABLE_SANITIZER_UB "Enable undefined behavior sanitizer" OFF)
option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
option(ENABLE_TESTING "Enable test builds" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(BUILD_DEBIAN_PACKAGES "Build Debian packages" ON)

# Library build options
option(BUILD_RUNTIME "Build protoflow-runtime library" ON)
option(BUILD_MESSAGING "Build protoflow-messaging library" ON)
option(BUILD_LOGGING "Build protoflow-logging library" ON)
option(BUILD_CONFIG "Build protoflow-config library" ON)
option(BUILD_SERVICE "Build protoflow-service library" ON)
option(BUILD_FSM "Build protoflow-fsm library" ON)
option(BUILD_FSM_TABLE "Build protoflow-fsm-table library" ON)
option(BUILD_RPC "Build protoflow-rpc library" ON)
option(BUILD_RPC_SERVICE "Build protoflow-rpc-service library" ON)
option(BUILD_TRANSPORT_TCP "Build protoflow-transport-tcp library" ON)
option(BUILD_TRANSPORT_UNIX "Build protoflow-transport-unix library" ON)
option(BUILD_TRANSPORT_MQTT "Build protoflow-transport-mqtt library" OFF)
option(BUILD_HTML_FRAGMENT "Build protoflow-html-fragment library" ON)
option(BUILD_HTTP "Build protoflow-http library" ON)
option(BUILD_HW_PROTOCOL "Build protoflow-hw-protocol library" ON)
option(BUILD_HW_CLIENT "Build protoflow-hw-client library" OFF)
option(BUILD_APP_REGISTRATION_PROTOCOL "Build protoflow-app-registration-protocol library" ON)
option(BUILD_APP_REGISTRATION_CLIENT "Build protoflow-app-registration-client library" ON)
option(BUILD_MAIN_APP "Build main application" ON)

# Include CMake modules
include(CompilerWarnings)
include(Sanitizers)
include(CodeCoverage)
include(StaticAnalyzers)
include(ProtoflowMacros)

# Apply compiler warnings to all targets
set_project_warnings()

# Apply sanitizers if enabled
enable_sanitizers()

# Enable testing
if(ENABLE_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)
endif()

# Libraries
if(BUILD_RUNTIME)
    add_subdirectory(libs/protoflow-runtime)
endif()

if(BUILD_MESSAGING)
    add_subdirectory(libs/protoflow-messaging)
endif()

if(BUILD_LOGGING)
    add_subdirectory(libs/protoflow-logging)
endif()

if(BUILD_CONFIG)
    add_subdirectory(libs/protoflow-config)
endif()

if(BUILD_SERVICE)
    add_subdirectory(libs/protoflow-service)
endif()

if(BUILD_FSM)
    add_subdirectory(libs/protoflow-fsm)
endif()

if(BUILD_FSM_TABLE)
    add_subdirectory(libs/protoflow-fsm-table)
endif()

if(BUILD_RPC)
    add_subdirectory(libs/protoflow-rpc)
endif()

if(BUILD_RPC_SERVICE)
    add_subdirectory(libs/protoflow-rpc-service)
endif()

if(BUILD_TRANSPORT_TCP)
    add_subdirectory(libs/protoflow-transport-tcp)
endif()

if(BUILD_TRANSPORT_UNIX)
    add_subdirectory(libs/protoflow-transport-unix)
endif()

if(BUILD_TRANSPORT_MQTT)
    add_subdirectory(libs/protoflow-transport-mqtt)
endif()

if(BUILD_HTML_FRAGMENT)
    add_subdirectory(libs/protoflow-html-fragment)
endif()

if(BUILD_HTTP)
    add_subdirectory(libs/protoflow-http)
endif()

if(BUILD_HW_PROTOCOL)
    add_subdirectory(libs/protoflow-hw-protocol)
endif()

if(BUILD_HW_CLIENT)
    add_subdirectory(libs/protoflow-hw-client)
endif()

if(BUILD_APP_REGISTRATION_PROTOCOL)
    add_subdirectory(libs/protoflow-app-registration-protocol)
endif()

if(BUILD_APP_REGISTRATION_CLIENT)
    add_subdirectory(libs/protoflow-app-registration-client)
endif()

# Applications
if(BUILD_MAIN_APP)
    add_subdirectory(apps/main-app)
endif()

# Tests
if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# CPack configuration for Debian packages
if(BUILD_DEBIAN_PACKAGES)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Protoflow Project")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andreas <>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/and-elf/protoflow")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    
    include(CPack)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Protoflow Configuration:")
message(STATUS "  Version:               ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:          C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:              ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Testing:               ${ENABLE_TESTING}")
message(STATUS "  Coverage:              ${ENABLE_COVERAGE}")
message(STATUS "  Sanitizer (Address):   ${ENABLE_SANITIZER_ADDRESS}")
message(STATUS "  Sanitizer (UB):        ${ENABLE_SANITIZER_UB}")
message(STATUS "  Sanitizer (Thread):    ${ENABLE_SANITIZER_THREAD}")
message(STATUS "  Warnings as Errors:    ${ENABLE_WARNINGS_AS_ERRORS}")
message(STATUS "  Debian Packages:       ${BUILD_DEBIAN_PACKAGES}")
message(STATUS "")
message(STATUS "Libraries:")
message(STATUS "  runtime:               ${BUILD_RUNTIME}")
message(STATUS "  messaging:             ${BUILD_MESSAGING}")
message(STATUS "  logging:               ${BUILD_LOGGING}")
message(STATUS "  config:                ${BUILD_CONFIG}")
message(STATUS "  service:               ${BUILD_SERVICE}")
message(STATUS "  fsm:                   ${BUILD_FSM}")
message(STATUS "  fsm-table:             ${BUILD_FSM_TABLE}")
message(STATUS "  rpc:                   ${BUILD_RPC}")
message(STATUS "  rpc-service:           ${BUILD_RPC_SERVICE}")
message(STATUS "  http:                  ${BUILD_HTTP}")
message(STATUS "  transport-tcp:         ${BUILD_TRANSPORT_TCP}")
message(STATUS "  transport-mqtt:        ${BUILD_TRANSPORT_MQTT}")
message(STATUS "  transport-unix:        ${BUILD_TRANSPORT_UNIX}")
message(STATUS "  html-fragment:         ${BUILD_HTML_FRAGMENT}")
message(STATUS "  hw-protocol:           ${BUILD_HW_PROTOCOL}")
message(STATUS "  hw-client:             ${BUILD_HW_CLIENT}")
message(STATUS "  app-registration-protocol: ${BUILD_APP_REGISTRATION_PROTOCOL}")
message(STATUS "  app-registration-client: ${BUILD_APP_REGISTRATION_CLIENT}")
message(STATUS "")
message(STATUS "Applications:")
message(STATUS "  main-app:              ${BUILD_MAIN_APP}")
message(STATUS "")
