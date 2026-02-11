# protoflow-rpc-client library
# RPC client implementation

protoflow_add_library(
    NAME rpc-client
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC client implementation"
    
    PUBLIC_HEADERS
        include/protoflow/rpc/rpc_client.hpp
        
    SOURCES
        src/rpc_client.cpp
        
    PUBLIC_INCLUDE_DIRS
        include
        
    PUBLIC_LINK_LIBRARIES
        protoflow::rpc-protocol
)
