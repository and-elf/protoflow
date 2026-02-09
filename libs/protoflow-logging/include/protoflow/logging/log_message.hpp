#pragma once

#include <protoflow/messaging/log_message.hpp>

namespace protoflow::logging {

// Re-export LogMessage and LogLevel from messaging namespace
// This maintains the logging:: namespace for semantic clarity
using LogMessage = protoflow::messaging::LogMessage;
using Level = protoflow::messaging::LogLevel;
using LogLevel = protoflow::messaging::LogLevel;

// Re-export to_string function
using protoflow::messaging::to_string;

} // namespace protoflow::logging
