add_library(protoflow-hw-client INTERFACE)
add_library(protoflow::hw-client ALIAS protoflow-hw-client)

target_include_directories(protoflow-hw-client
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(protoflow-hw-client
    INTERFACE
        protoflow::rpc
)

target_compile_features(protoflow-hw-client INTERFACE cxx_std_23)

# Installation
install(TARGETS protoflow-hw-client
    EXPORT protoflow-hw-clientTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT protoflow-hw-clientTargets
    FILE protoflow-hw-clientTargets.cmake
    NAMESPACE protoflow::
    DESTINATION lib/cmake/protoflow-hw-client
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/templates/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-hw-clientConfig.cmake
    INSTALL_DESTINATION lib/cmake/protoflow-hw-client
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-hw-clientConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-hw-clientConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/protoflow-hw-clientConfigVersion.cmake
    DESTINATION lib/cmake/protoflow-hw-client
)
