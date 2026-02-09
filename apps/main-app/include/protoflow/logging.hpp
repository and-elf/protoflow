#pragma once

// Convenience header for protoflow logging library

#include <protoflow/logging/logging.hpp>
#include <protoflow/logging/logging_service.hpp>
#include <protoflow/logging/log_message.hpp>

namespace protoflow::logging {
    // Alias for logging service as "Logger" for compatibility
    using Logger = LoggingService;
}
