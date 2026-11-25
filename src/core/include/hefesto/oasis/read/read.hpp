// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS read entry point (capability oasis-read;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D12): `oasis read`, the
// CLI, the shell and the generic open path (D15 format recognition) all
// call oasis::read. Like gds::read it runs on the command thread under the
// exclusive database lock, with progress and cancel through `progress`.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASR fills
// in the implementation in src/core/src/oasis/read/ (design D2).

#pragma once

#include <cstdint>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/mask/stream_read.hpp"
#include "hefesto/oasis/command/settings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::oasis {

/// The options of one read: the session's shared read settings, `oasis`
/// settings and read warnings at the time of the call.
struct OasisReadOptions {
  /// MaskSession::stream_read: the read settings shared with `gds`.
  stream::StreamReadSettings stream_read;
  /// MaskSession::oasis.
  OasisSettings settings;
  /// CIFRescaleAllow (MaskSession::rescale_allowed).
  bool rescale_allowed = true;
  mask::WarningSettings warnings;
  std::string typed_name;
  /// The file system the file is opened on; null means the local one.
  platform::FileSystem* files = nullptr;
  /// The OASIS_FILE property of read-only cells.
  std::string oasis_file;
  /// The size of the file in bytes, for progress reports; 0 = unknown.
  std::uint64_t source_bytes = 0;
};

/// Reads `path` (gzip-wrapped or not) into the editor's cell library. The
/// result's value is the top cell to load into the window, if any
/// (oasis-read "Top cell selection"); `files` may be an in-memory file
/// system.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const OasisReadOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::oasis
