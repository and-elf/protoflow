#pragma once

// App Registration Client Library
// Service for registering apps with main server via RPC

#include "app_registration_client/config.hpp"
#include "app_registration_client/app_registration_client.hpp"
#include <protoflow/app_registration_protocol.hpp>

namespace protoflow {
    // Re-export for convenience
    namespace arc = ::protoflow::app_registration_client;
}

