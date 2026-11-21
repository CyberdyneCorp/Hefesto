// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `lef` section (capability
// lef-tech-section: the table, the defaults, the scaling;
// hefesto-testing-strategy "Parser fuzzing"; m6-lef-def-oasis design D21).
// The first byte is a mode: bits 0-2 pick the base technology
// (mask_section_fuzz.hpp; base 2 is tests/fuzz/data/lefdef_fuzz.tech, which
// has DRC widths and spacings for the defaults), bit 3 magic-compat OFF,
// bits 4-5 the grid rescale n (1-4) and bits 6-7 its d (1-4). The rest is
// the body of a `lef` section that replaces the base's own one.
//
// The technology is loaded as `tech load` loads it (the section is read
// after the drc section and installed with the post-load scaling and
// defaults), the layout is rescaled by n/d as `scalegrid` does (the LEF
// table and the via areas scale, magic-compat OFF refills the defaults),
// and every record is enumerated and every magic type looked up.
//
// Resource caps: none needed; the section's work is linear in its lines.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/tech/lifecycle.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/commands.hpp"
#include "mask_section_fuzz.hpp"

namespace {

namespace lef = hefesto::lef;

const hefesto::fuzz::SectionBases& bases() {
  static const hefesto::fuzz::SectionBases instance{"lef", "tests/fuzz/data/lefdef_fuzz.tech"};
  return instance;
}

/// Every record of the table, and the LEF record of every magic type.
void query_table(const hefesto::editor::EditorState& editor) {
  const lef::LayerTable& table = editor.lefdef.layers;
  table.enumerate([&](std::string_view name, const lef::LayerRecord&) { (void)table.find(name); });
  for (std::size_t t = 0; t < editor.technology->type_count(); ++t) {
    (void)table.magic_to_lef(static_cast<hefesto::tech::TileType>(t));
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, body] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & 8U) == 0;
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::platform::PlatformServices view = services.view();
  if (!view.files.write_atomic("/t/section.tech", bases().text(mode & 7U, body)).ok()) {
    return 0;
  }
  hefesto::editor::EditorState editor;
  editor.drc.compat = &compat;
  hefesto::diag::NullSink sink;
  hefesto::tech::SessionLoadOptions options;
  options.magic_compat = compat.magic_compat;
  options.precheck = false;
  if (!hefesto::tech::load_session_technology(editor, view, "/t/section.tech", options, sink).ok()) {
    return 0;
  }
  query_table(editor);
  const std::int64_t n = 1 + ((mode >> 4U) & 3U);
  const std::int64_t d = 1 + ((mode >> 6U) & 3U);
  (void)hefesto::tech::scale_layout(editor, n, d);
  lef::refill_layer_table(editor, compat);
  query_table(editor);
  return 0;
}
