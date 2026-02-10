# protoflow-logging library
# Logging infrastructure integrated with service messaging

protoflow_add_library(
    NAME logging
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Logging infrastructure integrated with service messaging"
    TYPE STATIC
    
    SOURCES
        src/log_message.cpp
        # src/logging_service.cpp  # Disabled - has compilation errors
    
    PUBLIC_INCLUDE_DIRS
        include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::messaging
        protoflow::service
)
