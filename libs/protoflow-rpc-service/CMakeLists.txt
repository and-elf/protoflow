cmake_minimum_required(VERSION 3.25)

protoflow_add_library(
    NAME rpc-service
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC service for managing network connections via messaging"
    TYPE INTERFACE
    
    # SOURCES
    #     src/rpc_service.cpp  # Disabled - has compilation errors
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::service
        protoflow::transport-tcp
        protoflow::logging
)
