# Unit tests

# Add library test subdirectories
if(BUILD_APP_REGISTRATION_CLIENT)
    add_subdirectory(libs/protoflow-app-registration-client)
endif()

add_subdirectory(libs/protoflow-fsm)
add_subdirectory(libs/protoflow-html-fragment)
add_subdirectory(libs/protoflow-http)

if(BUILD_HW_CLIENT)
    add_subdirectory(libs/protoflow-hw-client)
endif()

add_subdirectory(libs/protoflow-messaging)
add_subdirectory(libs/protoflow-logging)
add_subdirectory(libs/protoflow-rpc)
add_subdirectory(libs/protoflow-transport-tcp)
add_subdirectory(libs/protoflow-transport-unix)

