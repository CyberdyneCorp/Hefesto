// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The emit phase of OASIS write (capability oasis-write;
// openspec/changes/m6-lef-def-oasis/design.md D4 "Commands and progress",
// D13). OASIS write reuses M5's three-phase write unchanged:
// gds::prepare_write (exclusive lock), then this emit (shared lock:
// generation through mask::generate_hierarchical, then OASIS encoding
// through oasis::Encoder), then gds::finish_write. Like gds::emit_write it
// reads its settings from the PreparedWrite and `settings` only and mutates
// nothing in the session or the layout (Q-MASK-208). A cancelled emit
// leaves no output file: it writes a temporary file in the target directory
// and renames it on success.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASW fills
// in the implementation in src/core/src/oasis/write/ (design D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/oasis/command/settings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::oasis {

/// Generates and writes the OASIS file `path` for `prepared`. SCAFFOLD:
/// fails with CORE-NOTIMPL-006 ("not implemented yet: oasis write") until
/// OASW lands it.
[[nodiscard]] cmd::CommandResult emit(const gds::PreparedWrite& prepared, const OasisSettings& settings,
                                      const platform::Path& path, const mask::Progress* progress = nullptr);

}  // namespace hefesto::oasis
