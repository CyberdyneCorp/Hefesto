// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M0 built-in commands (`help` moved to the ui area in M2):
//   echo [<word> ...]                return the words joined by single spaces
//   version                          return the Hefesto version
//   quit [<status>] [-noprompt]      stop the running script / session
//   setting [get <name> | set <name> <on|off>]
//                                    list, read or change a project setting
//   hefesto::strict [on|off]         query or set script strict mode for the
//                                    calling interpreter (hefesto-diagnostics)
//   hefesto::debug::hashorder size ?+key|-key ...?
//                                    M0-only: Magic string hash table order
//                                    (golden case CORE-HASH-01)

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::cmd {

/// Adds the built-ins to `registry`. Returns false if a name was taken.
bool register_builtins(CommandRegistry& registry);

}  // namespace hefesto::cmd
