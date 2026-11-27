// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of the route area (openspec/changes/m10-plot-route
// tasks.md 3.1): a command or function declared by the M10 plot/route
// scaffold before its workstream implements it reports CORE-NOTIMPL-010
// ("not implemented yet: <what>"). Each workstream replaces its stubs;
// INTEGRATION retires the header and the code at M10 exit (design D22).
//
// Owner: SCAFFOLD.

#pragma once

#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::route {

/// The CORE-NOTIMPL-010 diagnostic for `what` (e.g. "route::...").
[[nodiscard]] diag::Diagnostic not_implemented(std::string_view what);

}  // namespace hefesto::route
