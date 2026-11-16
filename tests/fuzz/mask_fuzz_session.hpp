// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scratch session of the M5 reader fuzz targets (fuzz_gds_reader,
// fuzz_cif_reader; m5-mask-gds-cif design D21): the fixed technology
// tests/fuzz/data/mask_fuzz.tech with its cifinput and cifoutput styles
// installed, read once per magic-compat mode, and the GDS write of the top
// cells after a read into an in-memory file system. The write's output
// style generates no cut arrays, so its cost is bounded by the input tiles.
//
// Resource cap: hierarchical generation visits every interaction square
// (at least 50 internal units on a side) a use reaches, as Magic's
// CIFGenSubcells does, so a few bytes of AREF make a write over the whole
// plane run for days (an accepted limit, docs/contributing/security.md
// M5-S4). The writes are skipped when a cell spans more than
// kMaxWriteSquares such squares, so a fuzz time-out is a real hang.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/cif/write/write.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::fuzz {

/// The most 50 x 50 interaction squares a written cell may span.
inline constexpr double kMaxWriteSquares = 2e4;
inline constexpr double kInteractionStep = 50;

/// The technology and both style sets for one magic-compat mode.
struct MaskFuzzStyles {
  std::shared_ptr<const tech::Technology> technology;
  mask::OutputStyleSet output;
  mask::InputStyleSet input;
};

inline MaskFuzzStyles load_mask_fuzz_styles(bool magic_compat) {
  MaskFuzzStyles out;
  platform::MemoryFileSystem fs;
  diag::NullSink sink;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  auto loaded = tech::load_technology_text(source_file("tests/fuzz/data/mask_fuzz.tech"), "/fuzz/mask_fuzz.tech", fs,
                                           options, sink);
  out.technology = loaded.ok() ? loaded.value() : tech::builtin_technology();
  compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  out.output = mask::read_output_styles(*out.technology, sink, compat);
  out.input = mask::read_input_styles(*out.technology, compat, sink);
  return out;
}

inline const MaskFuzzStyles& mask_fuzz_styles(bool magic_compat) {
  static const MaskFuzzStyles on = load_mask_fuzz_styles(true);
  static const MaskFuzzStyles off = load_mask_fuzz_styles(false);
  return magic_compat ? on : off;
}

/// An editor session with the fuzz technology and its styles installed.
struct MaskFuzzSession {
  compat::CompatSettings compat;
  editor::EditorState editor;
  platform::MemoryFileSystem files;

  explicit MaskFuzzSession(bool magic_compat) {
    compat.magic_compat = magic_compat;
    const MaskFuzzStyles& styles = mask_fuzz_styles(magic_compat);
    editor.technology = styles.technology;
    editor.drc.compat = &compat;
    editor.mask.output_styles = styles.output;
    editor.mask.input_styles = styles.input;
    editor.mask.time_zone = "UTC0";
  }

  /// The cells no other cell uses, by name, at most `limit` of them.
  [[nodiscard]] std::vector<db::CellDef*> top_cells(std::size_t limit) const {
    std::vector<db::CellDef*> tops;
    editor.cells.for_each([&](db::CellDef& def) {
      if (def.instances().empty()) {
        tops.push_back(&def);
      }
    });
    std::sort(tops.begin(), tops.end(),
              [](const db::CellDef* a, const db::CellDef* b) { return a->name() < b->name(); });
    tops.resize(std::min(tops.size(), limit));
    return tops;
  }

  /// True when no cell spans more than kMaxWriteSquares interaction squares.
  [[nodiscard]] bool write_bounded() const {
    bool bounded = true;
    editor.cells.for_each([&](db::CellDef& def) {
      const db::Rect& box = def.bbox();
      const double width = static_cast<double>(box.x1) - static_cast<double>(box.x0);
      const double height = static_cast<double>(box.y1) - static_cast<double>(box.y0);
      bounded = bounded && (width / kInteractionStep + 1) * (height / kInteractionStep + 1) <= kMaxWriteSquares;
    });
    return bounded;
  }

  /// Writes each top cell (at most `limit`) as GDS into `files`, with
  /// `merge` and `compress` (0 or a gzip level) as the `gds` settings; a
  /// read-then-write must never crash.
  void write_tops(std::size_t limit, bool merge, int compress) {
    gds::GdsWriteOptions options;
    options.settings = editor.mask.gds;
    options.settings.merge = merge;
    options.settings.compress = compress;
    options.settings.datestamp = 0;
    options.files = &files;
    options.time_zone = "UTC0";
    options.now = 0;
    if (!write_bounded()) {
      return;
    }
    for (db::CellDef* top : top_cells(limit)) {
      (void)gds::write(editor, *top, "/out.gds", options);
    }
  }

  /// Writes each top cell (at most `limit`) as CIF into `files`,
  /// hierarchical or `flat`.
  void write_cif_tops(std::size_t limit, bool flat) {
    cif::CifWriteOptions options;
    options.settings = editor.mask.cif;
    options.flat = flat;
    options.typed_name = "out";
    options.files = &files;
    options.time_zone = "UTC0";
    options.now = 0;
    if (!write_bounded()) {
      return;
    }
    for (db::CellDef* top : top_cells(limit)) {
      (void)cif::write(editor, *top, "/out.cif", options);
    }
  }
};

}  // namespace hefesto::fuzz
