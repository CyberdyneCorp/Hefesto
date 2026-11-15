// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `cifinput` section reader
// (capability mask-input-styles; hefesto-testing-strategy "Parser
// fuzzing"; m5-mask-gds-cif design D21). The first byte is a mode: bits
// 0-2 pick the base technology (mask_section_fuzz.hpp), bit 3
// magic-compat OFF, bits 4-5 the grid rescale n (1-4) and bits 6-7 its d
// (1-4). The rest is the body of a `cifinput` section that replaces the
// base's own one.
//
// The technology is loaded, every style is read and selected, the current
// one rescaled by n/d and asked for its scale, grow sizes and calma layer
// map, and then a small CIF file (a box on each of the first CIF layers
// of the style, in a symbol called twice) is read through it, so the
// fuzzed operators run in the read engine.
//
// Resource caps: at most kMaxLayers CIF layers get a box; the read
// engine's operators work on a handful of input tiles.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZ workstream.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/cif/read/read.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "mask_section_fuzz.hpp"

namespace {

namespace mask = hefesto::mask;
namespace tech = hefesto::tech;

constexpr std::size_t kMaxLayers = 16;

const hefesto::fuzz::SectionBases& bases() {
  static const hefesto::fuzz::SectionBases instance{"cifinput", "tests/data/mask/input/input.tech"};
  return instance;
}

/// A CIF layer name the CIF grammar can spell (`L` takes up to four
/// letters and digits).
bool cif_spellable(std::string_view name) {
  if (name.empty() || name.size() > 4) {
    return false;
  }
  for (const char c : name) {
    if (std::isupper(static_cast<unsigned char>(c)) == 0 && std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
  }
  return true;
}

/// The sample CIF: symbol 1 with a box on each spellable CIF layer of
/// `style`, called twice, and the same boxes at the top level.
std::string sample_cif(const mask::InputStyle& style) {
  std::string boxes;
  std::size_t count = 0;
  for (const std::string& name : style.cif_layers) {
    if (count == kMaxLayers) {
      break;
    }
    if (cif_spellable(name)) {
      const std::string x = std::to_string(400 * count);
      boxes += "L " + name + "; B 200 200 " + x + " 100; 94 n" + std::to_string(count) + " " + x + " 100;\n";
      ++count;
    }
  }
  return "DS 1 1 1;\n9 child;\n" + boxes + "DF;\nC 1 T 0 1000;\nC 1 MX R 0 1 T 3000 0;\n" + boxes + "E\n";
}

void query_current(const mask::InputStyleSet& styles, const tech::Technology& technology) {
  const mask::InputStyle* current = styles.current_style();
  if (current == nullptr) {
    return;
  }
  (void)current->microns_per_unit();
  for (tech::TileType t = 0; t < technology.type_count(); ++t) {
    (void)current->grow_size(t);
  }
  for (const auto& [key, layer] : current->calma) {
    (void)current->calma_layer(key.layer, key.datatype);
    (void)current->label_of(layer);
  }
  (void)mask::input_scale(styles);
}

void read_sample(const std::shared_ptr<const tech::Technology>& technology, const mask::InputStyleSet& styles,
                 const hefesto::compat::CompatSettings& compat) {
  const mask::InputStyle* current = styles.current_style();
  if (current == nullptr) {
    return;
  }
  hefesto::editor::EditorState editor;
  hefesto::compat::CompatSettings session_compat = compat;
  editor.technology = technology;
  editor.drc.compat = &session_compat;
  editor.mask.input_styles = styles;
  auto top = editor.cells.create("top", technology->plane_count());
  if (!top.ok()) {
    return;
  }
  top.value()->flags().available = true;
  editor.edit_cell = top.value();
  hefesto::platform::MemoryFileSystem files;
  hefesto::cif::CifReadOptions options;
  options.files = &files;
  options.typed_name = "in";
  (void)hefesto::cif::read_text(editor, sample_cif(*current), options);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, body] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = (mode & 8U) == 0;
  const auto technology = hefesto::fuzz::load_fuzz_technology(bases().text(mode & 7U, body), compat.magic_compat);
  if (technology == nullptr) {
    return 0;
  }
  hefesto::diag::NullSink sink;
  mask::InputStyleSet styles = mask::read_input_styles(*technology, compat, sink);
  const std::int64_t n = 1 + ((mode >> 4U) & 3U);
  const std::int64_t d = 1 + ((mode >> 6U) & 3U);
  for (const std::string& name : styles.names()) {
    (void)mask::select_input_style(styles, name, n, d, compat, sink);
  }
  if (!styles.names().empty()) {
    (void)mask::select_input_style(styles, styles.names().front(), compat);
  }
  (void)mask::rescale_input_styles(styles, n, d, compat);
  query_current(styles, *technology);
  read_sample(technology, styles, compat);
  return 0;
}
