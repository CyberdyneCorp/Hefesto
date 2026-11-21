// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs of the schem area (openspec/changes/m8-schematic tasks.md
// 5.2): a function declared by the M8 scaffold before its workstream
// implements it reports CORE-NOTIMPL-007 ("not implemented yet: <what>")
// where it can carry a diagnostic, and a documented neutral value
// otherwise. Each workstream replaces its stubs; the header and the code
// are retired at M8 exit.
//
// Owner: SCAFFOLD.

#pragma once

#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

/// The CORE-NOTIMPL-007 diagnostic for `what` (e.g. "schem::read_document").
[[nodiscard]] diag::Diagnostic not_implemented(std::string_view what);

}  // namespace hefesto::schem
