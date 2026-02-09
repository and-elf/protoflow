# Protoflow CMake macros for building apps and libraries
# See docs/cmake-macros.md for detailed documentation

include(CMakeParseArguments)
include(GNUInstallDirs)

# ============================================================================
# protoflow_add_app - Build a protoflow application with Debian packaging
# ============================================================================
function(protoflow_add_app)
    set(options "")
    set(oneValueArgs 
        NAME VERSION DESCRIPTION PKI_PATH USER MAINTAINER HOMEPAGE LOGGING_CONFIG)
    set(multiValueArgs 
        SOURCES HW_REQUIREMENTS LINK_LIBRARIES 
        DEPENDS_APPS DEBIAN_DEPENDS)
    
    cmake_parse_arguments(
        APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Validate required arguments
    if(NOT APP_NAME)
        message(FATAL_ERROR "protoflow_add_app: NAME is required")
    endif()
    if(NOT APP_VERSION)
        message(FATAL_ERROR "protoflow_add_app: VERSION is required")
    endif()
    if(NOT APP_SOURCES)
        message(FATAL_ERROR "protoflow_add_app: SOURCES is required")
    endif()
    
    # Set defaults
    if(NOT APP_DESCRIPTION)
        set(APP_DESCRIPTION "Protoflow application: ${APP_NAME}")
    endif()
    if(NOT APP_USER)
        set(APP_USER "protoflow-${APP_NAME}")
    endif()
    if(NOT APP_PKI_PATH)
        set(APP_PKI_PATH "/etc/protoflow/pki/${APP_NAME}")
    endif()
    if(NOT APP_MAINTAINER)
        set(APP_MAINTAINER "Protoflow Project <protoflow@example.com>")
    endif()
    
    # Create executable
    set(TARGET_NAME "protoflow-${APP_NAME}")
    add_executable(${TARGET_NAME} ${APP_SOURCES})
    
    # Link libraries
    if(APP_LINK_LIBRARIES)
        target_link_libraries(${TARGET_NAME} 
            PRIVATE ${APP_LINK_LIBRARIES})
    endif()
    
    # Set properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        VERSION ${APP_VERSION}
        OUTPUT_NAME ${TARGET_NAME}
    )
    
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
    
    # Generate systemd service file
    set(APP_BINARY_NAME ${TARGET_NAME})
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/templates/app.service.in
        ${CMAKE_BINARY_DIR}/systemd/${TARGET_NAME}.service
        @ONLY
    )
    
    # Install executable
    install(TARGETS ${TARGET_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Install systemd service
    install(FILES 
        ${CMAKE_BINARY_DIR}/systemd/${TARGET_NAME}.service
        DESTINATION lib/systemd/system
    )
    
    # Install logging configuration if provided
    if(APP_LOGGING_CONFIG)
        install(FILES ${APP_LOGGING_CONFIG}
            DESTINATION /etc/protoflow/${APP_NAME}/
            RENAME logging.ini
        )
    endif()
    
    message(STATUS "Configured protoflow app: ${APP_NAME} v${APP_VERSION}")
endfunction()

# ============================================================================
# protoflow_add_library - Build a protoflow library with proper CMake targets
# ============================================================================
function(protoflow_add_library)
    set(options "")
    set(oneValueArgs 
        NAME VERSION DESCRIPTION TYPE MAINTAINER HOMEPAGE)
    set(multiValueArgs 
        SOURCES PUBLIC_HEADERS PUBLIC_INCLUDE_DIRS PRIVATE_INCLUDE_DIRS
        LINK_LIBRARIES PUBLIC_LINK_LIBRARIES DEBIAN_DEPENDS)
    
    cmake_parse_arguments(
        LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Validate required arguments
    if(NOT LIB_NAME)
        message(FATAL_ERROR "protoflow_add_library: NAME is required")
    endif()
    if(NOT LIB_VERSION)
        message(FATAL_ERROR "protoflow_add_library: VERSION is required")
    endif()
    
    # Set defaults
    if(NOT LIB_TYPE)
        set(LIB_TYPE STATIC)
    endif()
    if(NOT LIB_DESCRIPTION)
        set(LIB_DESCRIPTION "Protoflow library: ${LIB_NAME}")
    endif()
    
    set(TARGET_NAME "protoflow-${LIB_NAME}")
    set(ALIAS_NAME "protoflow::${LIB_NAME}")
    
    # Create library
    if(LIB_TYPE STREQUAL "INTERFACE")
        add_library(${TARGET_NAME} INTERFACE)
    else()
        add_library(${TARGET_NAME} ${LIB_TYPE} ${LIB_SOURCES})
    endif()
    
    add_library(${ALIAS_NAME} ALIAS ${TARGET_NAME})
    
    # Parse version
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${LIB_VERSION})
    set(LIB_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(LIB_VERSION_MINOR ${CMAKE_MATCH_2})
    set(LIB_VERSION_PATCH ${CMAKE_MATCH_3})
    
    # Include directories
    if(LIB_TYPE STREQUAL "INTERFACE")
        if(LIB_PUBLIC_INCLUDE_DIRS)
            target_include_directories(${TARGET_NAME}
                INTERFACE
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PUBLIC_INCLUDE_DIRS}>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            )
        endif()
    else()
        if(LIB_PUBLIC_INCLUDE_DIRS)
            target_include_directories(${TARGET_NAME}
                PUBLIC
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PUBLIC_INCLUDE_DIRS}>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            )
        endif()
        if(LIB_PRIVATE_INCLUDE_DIRS)
            target_include_directories(${TARGET_NAME}
                PRIVATE ${LIB_PRIVATE_INCLUDE_DIRS}
            )
        endif()
    endif()
    
    # Link libraries
    if(LIB_TYPE STREQUAL "INTERFACE")
        if(LIB_PUBLIC_LINK_LIBRARIES)
            target_link_libraries(${TARGET_NAME}
                INTERFACE ${LIB_PUBLIC_LINK_LIBRARIES}
            )
        endif()
    else()
        if(LIB_PUBLIC_LINK_LIBRARIES)
            target_link_libraries(${TARGET_NAME}
                PUBLIC ${LIB_PUBLIC_LINK_LIBRARIES}
            )
        endif()
        if(LIB_LINK_LIBRARIES)
            target_link_libraries(${TARGET_NAME}
                PRIVATE ${LIB_LINK_LIBRARIES}
            )
        endif()
    endif()
    
    # Set properties
    if(NOT LIB_TYPE STREQUAL "INTERFACE")
        set_target_properties(${TARGET_NAME} PROPERTIES
            VERSION ${LIB_VERSION}
            SOVERSION ${LIB_VERSION_MAJOR}
            OUTPUT_NAME ${TARGET_NAME}
        )
        
        if(LIB_PUBLIC_HEADERS)
            set_target_properties(${TARGET_NAME} PROPERTIES
                PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}"
            )
        endif()
    endif()
    
    # C++23 requirement
    if(LIB_TYPE STREQUAL "INTERFACE")
        target_compile_features(${TARGET_NAME} INTERFACE cxx_std_23)
    else()
        target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)
    endif()
    
    # Install targets
    install(TARGETS ${TARGET_NAME}
        EXPORT ${TARGET_NAME}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/protoflow/${LIB_NAME}
    )
    
    # Install headers (for INTERFACE or when not using PUBLIC_HEADER)
    if(LIB_PUBLIC_INCLUDE_DIRS)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PUBLIC_INCLUDE_DIRS}/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
        )
    endif()
    
    # Install CMake config files
    install(EXPORT ${TARGET_NAME}Targets
        FILE ${TARGET_NAME}Targets.cmake
        NAMESPACE protoflow::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
    
    # Generate config files
    include(CMakePackageConfigHelpers)
    
    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/templates/Config.cmake.in
        ${CMAKE_BINARY_DIR}/${TARGET_NAME}Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
    
    write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake
        VERSION ${LIB_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    install(FILES
        ${CMAKE_BINARY_DIR}/${TARGET_NAME}Config.cmake
        ${CMAKE_BINARY_DIR}/${TARGET_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
    )
    
    message(STATUS "Configured protoflow library: ${LIB_NAME} v${LIB_VERSION} (${LIB_TYPE})")
endfunction()

message(STATUS "Protoflow macros loaded")
