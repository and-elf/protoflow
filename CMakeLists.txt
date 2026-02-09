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
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
option(ENABLE_SANITIZER_UB "Enable undefined behavior sanitizer" OFF)
option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
option(ENABLE_TESTING "Enable test builds" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(BUILD_DEBIAN_PACKAGES "Build Debian packages" ON)

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
    include(GoogleTest)
    
    # Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Libraries
add_subdirectory(libs/protoflow-runtime)
add_subdirectory(libs/protoflow-messaging)
add_subdirectory(libs/protoflow-logging)
add_subdirectory(libs/protoflow-fsm)
add_subdirectory(libs/protoflow-rpc)
add_subdirectory(libs/protoflow-transport-tcp)
add_subdirectory(libs/protoflow-transport-mqtt)
add_subdirectory(libs/protoflow-html-fragment)

# Applications
add_subdirectory(apps/main-app)

# Tests
if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# CPack configuration for Debian packages
if(BUILD_DEBIAN_PACKAGES)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Protoflow Project")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andreas <andreas@example.com>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/yourorg/protoflow")
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
