cmake_minimum_required(VERSION 3.25)

# protoflow-config library
add_library(protoflow-config STATIC)

target_sources(protoflow-config
    PRIVATE
        src/ini_parser.cpp
        src/hardware_config.cpp
        src/logging_config.cpp
)

target_include_directories(protoflow-config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(protoflow-config
    PUBLIC
        protoflow::messaging  # For LogLevel enum
        protoflow::logging    # For log_message.hpp
)

target_compile_features(protoflow-config PUBLIC cxx_std_23)

# Alias for consistent naming
add_library(protoflow::config ALIAS protoflow-config)

# Install
install(TARGETS protoflow-config
    EXPORT protoflow-configTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT protoflow-configTargets
    FILE protoflow-configTargets.cmake
    NAMESPACE protoflow::
    DESTINATION lib/cmake/protoflow-config
)

# Generate config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/protoflow-configConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-configConfig.cmake
    INSTALL_DESTINATION lib/cmake/protoflow-config
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-configConfig.cmake
    DESTINATION lib/cmake/protoflow-config
)
