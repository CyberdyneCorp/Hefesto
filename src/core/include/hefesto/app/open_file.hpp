// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The generic open path (capability oasis-command "Format recognition on
// open"; openspec/changes/m6-lef-def-oasis/design.md D15, D16): a layout
// file opened by any means other than the `gds` or `oasis` commands (the
// desktop File > Open picker, drag and drop, the iPadOS Files "Open in"
// request) is recognised by its content, after gzip decompression,
// whatever its extension (oasis::sniff_format), and becomes the command
// that reads it: `oasis read` for OASIS (with the current `oasis`
// settings), `gds read` for GDS, `load` for a Magic cell. A file that is
// none of these is reported as an unknown format (OASIS-CMD-005). The
// shells run the command line like any typed command, so it is echoed,
// logged and reaches the M5 progress and cancel hosts.
//
// Owner: OASCMD (design D2: the format recognition in `app/` open paths).

#pragma once

#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/oasis/command/format.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::app {

/// What opening one file runs.
struct OpenCommand {
  oasis::FileFormat format = oasis::FileFormat::unknown;
  /// The command words: {"oasis", "read", path}, {"gds", "read", path} or
  /// {"load", path}.
  std::vector<std::string> words;
  /// `words` as one Tcl command line (each word quoted as a list element).
  [[nodiscard]] std::string line() const;
};

/// The command that opens `path` on `fs`, from the file's content; an
/// OASIS-CMD-005 error for an unknown format, or the file system's error
/// when the file cannot be read.
[[nodiscard]] diag::Outcome<OpenCommand> open_command(platform::FileSystem& fs, const platform::Path& path);

}  // namespace hefesto::app
