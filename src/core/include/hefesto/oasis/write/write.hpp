// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS write entry point (capability oasis-write;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D13): `oasis write`, the
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

#include <string>
#include <string_view>

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
/// arrays, contacts, labels, library, lower, merge, undefined) taken from
/// the `oasis` values, magscale from `oasis`, and no GDS compression (OASIS
/// compresses with CBLOCKs). `datestamp` is not mapped: for OASIS it is a
/// read option (OASIS has no dates), so the GDS datestamp stays nullopt. `stream_read` is part of
/// the D4 signature; prepare_write reads no read setting today.
[[nodiscard]] gds::GdsWriteOptions to_gds_write_options(const OasisSettings& settings,
                                                        const stream::StreamReadSettings& stream_read);

/// "Write command and output file naming": the file written for the name
/// `name` (the typed file, or the bare command's root cell name): `name`,
/// with `.oas` appended when it contains no `.` anywhere.
[[nodiscard]] std::string output_file_name(std::string_view name);

/// The name a bare `oasis` writes: the root cell's name after its last `/`
/// (output_file_name then appends `.oas`).
[[nodiscard]] std::string default_output_name(const db::CellDef& root);

/// Writes `root` and its subtree to `path` (`options.prepare` is handed to
/// gds::prepare_write unchanged; `progress`'s cancel flag also stops its
/// subtree loading). A failed prepare phase creates no file; a cancelled
/// emit leaves no file; any other failure still leaves a decodable file
/// with the cells written ("Write result and error propagation").
[[nodiscard]] cmd::CommandResult write(editor::EditorState& editor, db::CellDef& root, const platform::Path& path,
                                       const OasisWriteOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::oasis
