#pragma once

// Protoflow FSM Library
// Compile-time validated finite state machines with observable behavior

#include "fsm/types.hpp"
#include "fsm/sink.hpp"
#include "fsm/sinks.hpp"
#include "fsm/fsm.hpp"

namespace protoflow {
    // Re-export main namespace for convenience
    namespace fsm = ::protoflow::fsm;
}
