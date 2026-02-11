# protoflow-rpc-server library
# RPC server implementation

protoflow_add_library(
    NAME rpc-server
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC server implementation"
    
    PUBLIC_HEADERS
        include/protoflow/rpc/rpc_server.hpp
        
    SOURCES
        src/rpc_server.cpp
        
    PUBLIC_INCLUDE_DIRS
        include
        
    PUBLIC_LINK_LIBRARIES
        protoflow::rpc-protocol
)
