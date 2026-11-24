// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of the sim area (openspec/changes/m9-lvs-simulation
// tasks.md 4.2): a function declared by the M9 scaffold before its
// workstream implements it reports CORE-NOTIMPL-009 ("not implemented yet:
// <what>"). Each workstream replaces its stubs; INTEGRATION retires the
// header and the code at M9 exit (design D24).
//
// Owner: SCAFFOLD.

#pragma once

#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::sim {

/// The CORE-NOTIMPL-009 diagnostic for `what` (e.g. "sim::prepare").
[[nodiscard]] diag::Diagnostic not_implemented(std::string_view what);

}  // namespace hefesto::sim
