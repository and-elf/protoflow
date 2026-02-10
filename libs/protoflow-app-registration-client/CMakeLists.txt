# protoflow-app-registration-client library
# Client library for app registration protocol

protoflow_add_library(
    NAME app-registration-client
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Client library for app registration protocol"
    TYPE STATIC
    
    SOURCES
        src/app_registration_client.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::app-registration-protocol
        protoflow::service  
        protoflow::fsm
        protoflow::rpc
)
