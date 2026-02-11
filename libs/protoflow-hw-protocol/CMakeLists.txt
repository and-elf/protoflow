cmake_minimum_required(VERSION 3.25)

# protoflow-hw-protocol library
# Shared protocol definitions for hardware arbitration

protoflow_add_library(
    NAME hw-protocol
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Shared protocol definitions for hardware arbitration"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
)
