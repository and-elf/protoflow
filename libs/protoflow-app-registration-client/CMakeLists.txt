add_library(protoflow-app-registration-client
    src/app_registration_client.cpp
)

add_library(protoflow::app-registration-client ALIAS protoflow-app-registration-client)

target_include_directories(protoflow-app-registration-client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(protoflow-app-registration-client
    PUBLIC
        protoflow::app-registration-protocol
        protoflow::service  
        protoflow::fsm
        protoflow::rpc
        protoflow::rpc-service
)

target_compile_features(protoflow-app-registration-client PUBLIC cxx_std_23)

# Installation
install(TARGETS protoflow-app-registration-client
    EXPORT protoflow-app-registration-clientTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT protoflow-app-registration-clientTargets
    FILE protoflow-app-registration-clientTargets.cmake
    NAMESPACE protoflow::
    DESTINATION lib/cmake/protoflow-app-registration-client
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/templates/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-app-registration-clientConfig.cmake
    INSTALL_DESTINATION lib/cmake/protoflow-app-registration-client
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-app-registration-clientConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-app-registration-clientConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-app-registration-clientConfigVersion.cmake
    DESTINATION lib/cmake/protoflow-app-registration-client
)
