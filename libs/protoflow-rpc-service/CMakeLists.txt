cmake_minimum_required(VERSION 3.25)

protoflow_add_library(
    NAME rpc-service
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC client service for managing outbound network connections via messaging"
    
    SOURCES
        src/messages.cpp
        src/rpc_client_service.cpp
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::service
        protoflow::rpc
        protoflow::transport-tcp
        protoflow::logging
)
