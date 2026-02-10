#pragma once

/// Test stub for logging macros
/// These no-op macros allow testing without the actual logging service
/// which has compilation issues

#define PROTOFLOW_LOG_TRACE(service, msg) do { (void)(service); } while(0)
#define PROTOFLOW_LOG_DEBUG(service, msg) do { (void)(service); } while(0)
#define PROTOFLOW_LOG_INFO(service, msg) do { (void)(service); } while(0)
#define PROTOFLOW_LOG_WARN(service, msg) do { (void)(service); } while(0)
#define PROTOFLOW_LOG_ERROR(service, msg) do { (void)(service); } while(0)
