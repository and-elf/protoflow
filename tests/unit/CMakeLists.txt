# Unit tests

# Add library test subdirectories
if(PROTOFLOW_BUILD_APP_REGISTRATION_CLIENT)
    add_subdirectory(libs/protoflow-app-registration-client)
endif()

add_subdirectory(libs/protoflow-fsm)

if(PROTOFLOW_BUILD_FSM_TABLE)
    add_subdirectory(libs/protoflow-fsm-table)
endif()

add_subdirectory(libs/protoflow-config)
add_subdirectory(libs/protoflow-html-fragment)
add_subdirectory(libs/protoflow-http)

if(PROTOFLOW_BUILD_HW_CLIENT)
    add_subdirectory(libs/protoflow-hw-client)
endif()

add_subdirectory(libs/protoflow-messaging)
add_subdirectory(libs/protoflow-logging)
add_subdirectory(libs/protoflow-rpc)
add_subdirectory(libs/protoflow-transport-tcp)
add_subdirectory(libs/protoflow-transport-unix)

# Add application test subdirectories
if(PROTOFLOW_BUILD_MAIN_APP)
    add_subdirectory(apps/main-app)
endif()
