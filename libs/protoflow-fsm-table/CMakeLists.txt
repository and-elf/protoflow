cmake_minimum_required(VERSION 3.20)
project(protoflow-fsm-table VERSION 1.0.0 LANGUAGES CXX)

# Define the library
add_library(protoflow-fsm-table INTERFACE)
add_library(protoflow::fsm-table ALIAS protoflow-fsm-table)

# Set include directories
target_include_directories(protoflow-fsm-table
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Require C++20
target_compile_features(protoflow-fsm-table INTERFACE cxx_std_20)

# Installation
install(TARGETS protoflow-fsm-table
    EXPORT protoflow-fsm-tableTargets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT protoflow-fsm-tableTargets
    FILE protoflow-fsm-tableConfig.cmake
    NAMESPACE protoflow::
    DESTINATION lib/cmake/protoflow-fsm-table
)

# Add version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/protoflow-fsm-tableConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/protoflow-fsm-tableConfigVersion.cmake"
    DESTINATION lib/cmake/protoflow-fsm-table
)

# Build examples if this is the main project or if explicitly requested
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR PROTOFLOW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
