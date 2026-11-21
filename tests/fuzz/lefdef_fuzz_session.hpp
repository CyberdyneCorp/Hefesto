// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scratch session of the LEF and DEF reader fuzz targets
// (fuzz_lef_reader, fuzz_def_reader; m6-lef-def-oasis design D21): an
// editor session on an in-memory file system with the fixed technology
// tests/fuzz/data/lefdef_fuzz.tech loaded as `tech load` loads it (LEF
// layer table, DRC rules, output style), in one magic-compat mode, and the
// LEF and DEF reads of an input held in that file system, plain or as one
// gzip member.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/def/read/read.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/gzip.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/read/read.hpp"
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

/// An editor session with lefdef_fuzz.tech loaded, on an in-memory file
/// system.
struct LefDefFuzzSession {
  compat::CompatSettings compat;
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  platform::PlatformServices view = services.view();
  editor::EditorState editor;
  diag::NullSink sink;
  bool loaded = false;

  explicit LefDefFuzzSession(bool magic_compat) {
    compat.magic_compat = magic_compat;
    editor.drc.compat = &compat;
    editor.mask.time_zone = "UTC0";
    static const std::string tech_text = source_file("tests/fuzz/data/lefdef_fuzz.tech");
    if (!view.files.write_atomic("/t/lefdef_fuzz.tech", tech_text).ok()) {
      return;
    }
    tech::SessionLoadOptions options;
    options.magic_compat = magic_compat;
    options.precheck = false;
    loaded = tech::load_session_technology(editor, view, "/t/lefdef_fuzz.tech", options, sink).ok();
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
