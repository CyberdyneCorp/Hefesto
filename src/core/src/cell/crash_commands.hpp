// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CELL commands: registration of `crash` and `archive`
// (crash_commands.cpp), called by register_cell_commands.

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::cell::detail {

[[nodiscard]] bool register_crash_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::cell::detail
