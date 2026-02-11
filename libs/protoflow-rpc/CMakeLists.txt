# protoflow-rpc library
# RPC framework for inter-app communication (facade library)

protoflow_add_library(
    NAME rpc
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC framework for inter-app communication"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
        
    PUBLIC_LINK_LIBRARIES
        protoflow::rpc-protocol
        protoflow::rpc-client
        protoflow::rpc-server
)
