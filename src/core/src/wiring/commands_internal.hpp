// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the parts of `wire` and the `trackwire` command kept in their own
// files (segment.cpp, tool.cpp). Owner: WIRE.

#pragma once

#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "wiring/session.hpp"

namespace hefesto::wiring {

/// `wire segment ...` (`args` starts with the option word).
[[nodiscard]] cmd::CommandResult segment_command(const WireSession& s, const std::vector<std::string>& args);

/// Adds `trackwire`.
bool register_trackwire(cmd::CommandRegistry& registry);

}  // namespace hefesto::wiring
