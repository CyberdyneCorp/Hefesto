// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS read (capability gds-read; openspec/changes/m5-mask-gds-cif/design.md
// D4 "Commands and progress", D11): `gds read`, the CLI and the shell call
// gds::read. It looks the file up as PaZOpen does (<name>, <name>.gz, the
// search path), parses the stream through the record dispatch table and
// drives the shared read engine (mask::ReadEngine) with the current input
// style. The read holds the exclusive database lock and runs on the command
// thread; Progress reports and cancels.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSR fills
// in the implementation in src/core/src/gds/read/ (design D2).

#pragma once

#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::gds {

/// The options of one read: the session's `gds` settings and read warnings
/// at the time of the call, and the name the user typed (for messages and
/// GDS_FILE).
struct GdsReadOptions {
  GdsSettings settings;
  mask::WarningSettings warnings;
  std::string typed_name;
};

/// Reads `path` into the editor's cell library.
[[nodiscard]] cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path,
                                      const GdsReadOptions& options, const mask::Progress* progress = nullptr);

}  // namespace hefesto::gds
