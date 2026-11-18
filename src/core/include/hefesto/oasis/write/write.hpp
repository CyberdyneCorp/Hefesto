// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS write entry point (capability oasis-write;
// openspec/changes/m6-lef-def-oasis/design.md D4, D13): `oasis write`, the
// CLI and the shell all call oasis::write. It reuses the M5 three-phase
// write unchanged: gds::prepare_write (exclusive lock), oasis::emit
// (oasis/write/emit.hpp, shared lock) and gds::finish_write. The prepare
// options come from to_gds_write_options; a GDS-only check in
// prepare_write that must differ for OASIS is raised as an open item and
// INTEGRATION adds a format parameter, so the prepare code is not forked.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold, and
// to_gds_write_options); OASW fills in write in src/core/src/oasis/write/
// (design D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/stream_read.hpp"
#include "hefesto/oasis/command/settings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::oasis {

/// The options of one write: the session's `oasis` settings at the time of
/// the call, and the prepare-phase options shared with `gds write`
/// (typed name, subtree loader, file system, time zone, clock).
struct OasisWriteOptions {
  OasisSettings settings;
  gds::GdsWriteOptions prepare;
};

/// The GdsSettings that gds::prepare_write reads for an OASIS write: the
/// write options `oasis` shares with `gds` by syntax (abstract, addendum,
/// arrays, contacts, labels, library, lower, merge, undefined, datestamp)
/// taken from the `oasis` values, magscale from `oasis`, and no GDS
/// compression (OASIS compresses with CBLOCKs). `stream_read` is part of
/// the D4 signature; prepare_write reads no read setting today.
[[nodiscard]] gds::GdsWriteOptions to_gds_write_options(const OasisSettings& settings,
                                                        const stream::StreamReadSettings& stream_read);

/// Writes `root` and its subtree to `path`. SCAFFOLD: fails with
/// CORE-NOTIMPL-006 ("not implemented yet: oasis write") until OASW lands it.
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const OasisWriteOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::oasis
