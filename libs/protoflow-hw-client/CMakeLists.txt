# protoflow-hw-client library
# Hardware client interface (header-only)

protoflow_add_library(
    NAME hw-client
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Hardware client interface for RPC-based hardware access"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::rpc
        protoflow::fsm
)

