#pragma once

#include <sstream>

/// Logging macros for protoflow services
/// These macros allow for formatted logging with stream-style syntax
/// Example: PROTOFLOW_LOG_INFO(service, "Counter value: " << counter);

#define PROTOFLOW_LOG_TRACE(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_trace(oss.str()); \
    } while(0)

#define PROTOFLOW_LOG_DEBUG(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_debug(oss.str()); \
    } while(0)

#define PROTOFLOW_LOG_INFO(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_info(oss.str()); \
    } while(0)

#define PROTOFLOW_LOG_WARN(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_warn(oss.str()); \
    } while(0)

#define PROTOFLOW_LOG_ERROR(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_error(oss.str()); \
    } while(0)

#define PROTOFLOW_LOG_FATAL(service, msg) \
    do { \
        std::ostringstream oss; \
        oss << msg; \
        (service).log_fatal(oss.str()); \
    } while(0)
