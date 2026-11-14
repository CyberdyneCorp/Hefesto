// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS read (capability gds-read; openspec/changes/m5-mask-gds-cif/design.md
// D4 "Commands and progress", D11): `gds read`, the CLI and the shell call
// gds::read. find_gds_file looks the file up as `gds read` does (the
// extensions .gds, .gds.gz, .gds2, .strm and none, along the cell search
// path); gds::read parses the stream (plain or gzip) through the record
// handlers and drives the shared read engine (mask::ReadEngine) with the
// current input style. The read holds the exclusive database lock, records
// no undo and runs on the command thread; Progress cancels it.
//
// Round 3 contract updates (GDSR): GdsReadOptions gained `files` (the file
// system; null for the local one), `gds_file` (the GDS_FILE value of a
// read-only read, chosen per Q-MASK-129 and substituted per Q-MASK-128 by
// the caller; empty: the path as given, substituted) and `time_zone` (the
// Q-MASK-132 zone of BGNSTR dates; empty: the host zone). gds::read also
// reads from a ByteSource, and find_gds_file was added. The CommandResult
// value is the library cell name when a cell of that name was read
// ("Library grammar and header processing" step 7: the caller loads it into
// the layout window, which needs the window); diagnostics carry Magic's
// messages in order.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/read/ (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds {

class ByteSource;

/// The options of one read: the session's `gds` settings and read warnings
/// at the time of the call, and the name the user typed (for messages and
/// GDS_FILE).
struct GdsReadOptions {
  GdsSettings settings;
  mask::WarningSettings warnings;
  std::string typed_name;
  /// The file system the file (and the redirect file) is opened on; null
  /// means the local one.
  platform::FileSystem* files = nullptr;
  /// The GDS_FILE property of read-only cells and annotated abstract views.
  std::string gds_file;
  /// The zone BGNSTR dates are converted in (a POSIX TZ string, Q-MASK-132);
  /// empty: the host zone.
  std::string time_zone;
};

/// "Read command and file lookup": `name` with `.gds`, `.gds.gz`, `.gds2`,
/// `.strm` and nothing appended, in that order, along the cell search path
/// (a compressed counterpart `<candidate>.gz` is taken when the candidate
/// is missing); nullopt when none opens.
[[nodiscard]] std::optional<cell::FoundFile> find_gds_file(cell::LookupContext& ctx, std::string_view name);

/// Reads `path` into the editor's cell library.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const GdsReadOptions& options, const mask::Progress* progress = nullptr);

/// Reads the stream of `source` (plain bytes; open_source inflates gzip).
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, ByteSource& source, const GdsReadOptions& options,
                                      const mask::Progress* progress = nullptr);

}  // namespace hefesto::gds
