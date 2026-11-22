// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scratch session of the LEF and DEF reader fuzz targets
// (fuzz_lef_reader, fuzz_def_reader; m6-lef-def-oasis design D21): an
// editor session on an in-memory file system with the fixed technology
// tests/fuzz/data/lefdef_fuzz.tech loaded as `tech load` loads it (LEF
// layer table, DRC rules, output style), in one magic-compat mode, and the
// LEF and DEF reads of an input held in that file system, plain or as one
// gzip member, and the `lef write` and `def write` of the cells they made.
//
// The technology is loaded once per mode, into a prototype session, and
// each input's session takes a copy of what the load installed (the
// technology, grid, locked types, DRC rules, LEF layer table, output and
// input styles and display styles), so an input costs no tech load.
//
// Resource caps of the writes: at most kMaxWriteCells cells are written;
// a VIAS cut array is capped at 1,000,000 cuts by the writer itself
// (Q-LEFDEF-150); the rest of a write is bounded by what the read built,
// which the input size bounds. No `.ext` file exists, so `def write`
// discovers no net geometry.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#pragma once

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/def/read/read.hpp"
#include "hefesto/def/write/write.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/read/read.hpp"
#include "hefesto/lef/write/write.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/commands.hpp"

namespace hefesto::fuzz {

/// `text` as one gzip member (deflate level 1); empty when it cannot be
/// compressed.
inline std::string gzip_text(std::string_view text) {
  std::vector<std::byte> plain(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    plain[i] = static_cast<std::byte>(text[i]);
  }
  auto memory = std::make_unique<gds::MemorySink>();
  gds::MemorySink* inner = memory.get();
  auto sink = gds::make_gzip_sink(std::move(memory), 1);
  if (!sink.ok() || !sink.value()->write(std::span<const std::byte>{plain}).ok() || !sink.value()->finish().ok()) {
    return {};
  }
  const std::vector<std::byte>& packed = inner->bytes();
  std::string out(packed.size(), '\0');
  for (std::size_t i = 0; i < packed.size(); ++i) {
    out[i] = static_cast<char>(packed[i]);
  }
  return out;
}

/// The most cells one input's writes cover (each kind of write).
inline constexpr std::size_t kMaxWriteCells = 2;

/// An editor session on an in-memory file system, before any technology.
struct LefDefFuzzBase {
  compat::CompatSettings compat;
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  platform::PlatformServices view = services.view();
  editor::EditorState editor;
  diag::NullSink sink;
  bool loaded = false;

  explicit LefDefFuzzBase(bool magic_compat) {
    compat.magic_compat = magic_compat;
    editor.drc.compat = &compat;
    editor.mask.time_zone = "UTC0";
  }
};

/// The session lefdef_fuzz.tech is loaded into once per mode, as `tech
/// load` loads it.
struct LefDefTechPrototype : LefDefFuzzBase {
  explicit LefDefTechPrototype(bool magic_compat) : LefDefFuzzBase(magic_compat) {
    const std::string tech_text = source_file("tests/fuzz/data/lefdef_fuzz.tech");
    if (!view.files.write_atomic("/t/lefdef_fuzz.tech", tech_text).ok()) {
      return;
    }
    tech::SessionLoadOptions options;
    options.magic_compat = magic_compat;
    options.precheck = false;
    loaded = tech::load_session_technology(editor, view, "/t/lefdef_fuzz.tech", options, sink).ok();
  }
};

inline const LefDefTechPrototype& lefdef_tech_prototype(bool magic_compat) {
  static const LefDefTechPrototype on{true};
  static const LefDefTechPrototype off{false};
  return magic_compat ? on : off;
}

/// An editor session with lefdef_fuzz.tech loaded, on an in-memory file
/// system.
struct LefDefFuzzSession : LefDefFuzzBase {
  explicit LefDefFuzzSession(bool magic_compat) : LefDefFuzzBase(magic_compat) {
    const LefDefTechPrototype& prototype = lefdef_tech_prototype(magic_compat);
    loaded = prototype.loaded;
    if (!loaded) {
      return;
    }
    // What tech::install_technology leaves in the session (a fresh session
    // has no cells, so no plane count changes).
    const editor::EditorState& from = prototype.editor;
    editor.grid = from.grid;
    editor.technology = from.technology;
    editor.locked_types = from.locked_types;
    editor.drc.cif = from.drc.cif;
    editor.drc.styles = from.drc.styles;
    editor.drc.style = from.drc.style;
    editor.drc.rules = from.drc.rules;
    editor.lefdef.layers = from.lefdef.layers;
    editor.mask.output_styles = from.mask.output_styles;
    editor.mask.input_styles = from.mask.input_styles;
    editor.styles = from.styles;
  }

  /// Stores `text` at `path`, as one gzip member when `gzip` is set.
  bool store(const std::string& path, std::string_view text, bool gzip) {
    return view.files.write_atomic(path, gzip ? gzip_text(text) : std::string{text}).ok();
  }

  /// `lef read` of `path` (a read must never crash).
  void read_lef(const std::string& path, bool annotate) {
    lef::LefReadOptions options;
    options.annotate = annotate;
    options.typed_name = path;
    options.files = &view.files;
    (void)lef::read(editor, path, options);
  }

  /// `def read` of `path` with the given options' flags.
  void read_def(const std::string& path, def::DefReadOptions options) {
    options.typed_name = path;
    options.files = &view.files;
    (void)def::read(editor, path, options);
  }

  /// The cells no other cell uses, by name, at most kMaxWriteCells.
  [[nodiscard]] std::vector<db::CellDef*> roots() const {
    std::vector<db::CellDef*> out;
    editor.cells.for_each([&](db::CellDef& def) {
      if (def.instances().empty() && !def.name().empty() && def.name() != "(UNNAMED)") {
        out.push_back(&def);
      }
    });
    std::sort(out.begin(), out.end(), [](const db::CellDef* a, const db::CellDef* b) { return a->name() < b->name(); });
    out.resize(std::min(out.size(), kMaxWriteCells));
    return out;
  }

  /// `lef write` of each root into memory, the first with -tech when
  /// `tech` is set (a read-then-write must never crash).
  void write_lef(bool tech) {
    lef::LefWriteOptions options;
    options.files = &view.files;
    options.typed_name = "/w/out";
    for (db::CellDef* cell : roots()) {
      options.tech = tech;
      tech = false;
      (void)lef::write(editor, *cell, "/w/out", options);
    }
  }

  /// `def write` of each root into memory, with -anal and -allspecial when
  /// `all` is set.
  void write_def(bool all) {
    def::DefWriteOptions options;
    options.files = &view.files;
    options.typed_name = "/w/out";
    options.anal = all;
    options.all_special = all;
    for (db::CellDef* cell : roots()) {
      (void)def::write(editor, *cell, "/w/out", options);
    }
  }

  /// The fixed LEF library of the DEF target (macros, vias, via rules and
  /// a non-default rule) read into the session.
  void read_fixed_lef() {
    static const std::string lef_text = source_file("tests/fuzz/data/lefdef_fuzz.lef");
    if (store("/t/lefdef_fuzz.lef", lef_text, false)) {
      read_lef("/t/lefdef_fuzz.lef", false);
    }
  }
};

}  // namespace hefesto::fuzz
