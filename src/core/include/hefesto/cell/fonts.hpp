// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's default outline fonts, loaded the way Magic's system startup file
// does (`setlabel font FreeSans.pt3 0.58`, `FreeSerif.pt3 0.58`,
// `FreeMono.pt3 0.6`), so that `flabel` records naming them (every
// sky130_fd_sc_hd cell uses FreeSans) read and write back unchanged
// (cell-file-format "Label records", font-labels).
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cell {

/// Loads each default font not yet in `fonts` from the first directory of
/// `directories` holding its `.pt3` file (Magic installs them in
/// `$CAD_ROOT/magic/sys`), in Magic's order. A missing file is skipped
/// silently; an unreadable one is reported to `sink`. Returns the number of
/// fonts added.
std::size_t load_default_fonts(platform::FileSystem& fs, const std::vector<platform::Path>& directories,
                               display::FontRegistry& fonts, diag::DiagnosticSink& sink);

}  // namespace hefesto::cell
