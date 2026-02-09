# protoflow-rpc library
# RPC framework for inter-app communication

protoflow_add_library(
    NAME rpc
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "RPC framework for inter-app communication"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
)
