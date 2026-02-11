# protoflow-rpc-protocol library
# RPC wire protocol definitions

protoflow_add_library(
    NAME rpc-protocol
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC wire protocol definitions"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
)
