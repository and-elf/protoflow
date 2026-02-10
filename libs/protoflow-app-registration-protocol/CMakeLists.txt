cmake_minimum_required(VERSION 3.25)

# protoflow-app-registration-protocol library
# Shared FSM states and events for app registration

protoflow_add_library(
    NAME app-registration-protocol
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Shared protocol definitions for application registration"
    TYPE INTERFACE
    
    PUBLIC_INCLUDE_DIRS
        include
)
