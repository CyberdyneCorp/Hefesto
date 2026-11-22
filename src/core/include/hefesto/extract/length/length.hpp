// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Path length extraction (capability extract-commands "Path length
// extraction"; openspec/changes/m7-extraction/design.md D12): the flood of
// ExtLength.c, ported exactly (Q-EXTRACT-12), writing `distance` records.
//
// Scaffold state (tasks.md 3.3): path_lengths() reports CORE-NOTIMPL-008
// until EXTCMD lands it.
//
// Owner: INTEGRATION (this header); EXTCMD (the implementation).

#pragma once

#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"

namespace hefesto::extract {

class ExtWriter;

/// Writes the driver-receiver distances of `lists` for the context's
/// current root cell.
[[nodiscard]] diag::Status path_lengths(ExtractContext& ctx, const LengthLists& lists, ExtWriter& writer);

}  // namespace hefesto::extract
