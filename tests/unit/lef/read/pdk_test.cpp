// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef read on the pinned PDK LEF sets (openspec/changes/archive/2026-11-22-m6-lef-def-oasis
// tasks.md 1.4, 5.7): the tech LEF and the standard-cell LEF of sky130A,
// gf180mcuD and ihp-sg13g2 read without errors into a session with the
// PDK's technology; every MACRO becomes a cell, and one macro's properties
// and port labels match its LEF text. Each test skips when its PDK or LEF
// set is not in .cache/pdks (scripts/fetch_pdk.py).
//
// Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/read/read.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/commands.hpp"
#include "reference_data.hpp"

namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace lef = hefesto::lef;

#define PDK_TAGS "[cap:lef-read][req:MACRO and SITE cell creation][pdk]"

namespace {

/// What the LEF text says of one macro.
struct MacroText {
  std::string class_;
  std::string site;
  std::string symmetry;
  float width = 0.0F;
  float height = 0.0F;
  std::set<std::string> pins;
};

std::vector<std::string> words(const std::string& line) {
  std::istringstream in{line};
  std::vector<std::string> out;
  for (std::string w; in >> w;) {
    out.push_back(w);
  }
  return out;
}

/// The words of a statement, without its `;`.
std::string joined(const std::vector<std::string>& w) {
  std::string out;
  for (std::size_t i = 1; i < w.size() && w[i] != ";"; ++i) {
    out += (out.empty() ? "" : " ") + w[i];
  }
  return out;
}

/// The MACRO names of a LEF file (MACRO at the start of a line), and the
/// statements of `macro`.
std::vector<std::string> scan(const std::filesystem::path& file, const std::string& macro, MacroText& text) {
  std::ifstream in{file};
  std::vector<std::string> macros;
  bool inside = false;
  int depth = 0;  // PIN nesting inside the macro
  for (std::string line; std::getline(in, line);) {
    const auto w = words(line);
    if (w.empty()) {
      continue;
    }
    if (line.starts_with("MACRO ") && w.size() > 1) {  // not a PROPERTYDEFINITIONS line
      macros.push_back(w[1]);
      inside = w[1] == macro;
      continue;
    }
    if (!inside) {
      continue;
    }
    if (w[0] == "END" && w.size() > 1 && w[1] == macro) {
      inside = false;
    } else if (w[0] == "PIN" && depth == 0) {
      text.pins.insert(w[1]);
      depth = 1;
    } else if (w[0] == "END" && w.size() > 1 && text.pins.contains(w[1])) {
      depth = 0;
    } else if (depth == 0 && w[0] == "CLASS") {
      text.class_ = joined(w);
    } else if (depth == 0 && w[0] == "SITE") {
      text.site = w[1];
    } else if (depth == 0 && w[0] == "SYMMETRY") {
      text.symmetry = joined(w);
    } else if (depth == 0 && w[0] == "SIZE" && w.size() >= 4) {
      text.width = std::stof(w[1]);
      text.height = std::stof(w[3]);
    }
  }
  return macros;
}

std::vector<diag::Diagnostic> errors(const hefesto::cmd::CommandResult& result) {
  std::vector<diag::Diagnostic> out;
  for (const auto& d : result.diagnostics) {
    if (d.severity == diag::Severity::error) {
      out.push_back(d);
    }
  }
  return out;
}

std::string string_property(const db::CellDef& cell, std::string_view name) {
  const auto* value = cell.properties().find(name);
  const auto* text = value != nullptr ? std::get_if<std::string>(value) : nullptr;
  return text != nullptr ? *text : std::string{};
}

void read_pdk(const hefesto::test::PinnedLef& set, const std::string& macro) {
  const auto tech = hefesto::test::pinned_tech(*set.pdk);
  const auto library = hefesto::test::pinned_lef_library(set);
  if (!tech || !library) {
    SKIP("the " << set.library << " LEF set or its technology is not in .cache/pdks");
  }
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, false, true}};
  hefesto::editor::EditorState editor;
  diag::NullSink quiet;
  hefesto::platform::PlatformServices view = services.view();
  REQUIRE(hefesto::tech::load_session_technology(editor, view, tech->string(), {}, quiet).ok());

  for (const auto& file : {*library / std::string{set.tech_lef}, *library / std::string{set.cell_lef}}) {
    INFO(file.string());
    const auto result = lef::read(editor, file.string(), lef::LefReadOptions{});
    REQUIRE(result.ok());
    for (const auto& d : errors(result)) {
      UNSCOPED_INFO(d.text);
    }
    CHECK(errors(result).empty());
  }

  MacroText text;
  const auto macros = scan(*library / std::string{set.cell_lef}, macro, text);
  REQUIRE_FALSE(macros.empty());
  for (const auto& name : macros) {
    CHECK(editor.cells.find(name) != nullptr);
  }
  const db::CellDef* cell = editor.cells.find(macro);
  REQUIRE(cell != nullptr);
  CHECK(string_property(*cell, "LEFclass") == text.class_);
  CHECK(string_property(*cell, "LEFsite") == text.site);
  CHECK(string_property(*cell, "LEFsymmetry") == text.symmetry);
  CHECK(string_property(*cell, "LEFview") == "TRUE");
  const float s = lef::lef_scale(editor.mask.output_styles.current_style());
  const auto* box = std::get_if<db::DimensionList>(cell->properties().find("FIXED_BBOX"));
  REQUIRE(box != nullptr);
  CHECK(box->values == std::vector<std::int32_t>{0, 0, *lef::to_internal(text.width, s),
                                                 *lef::to_internal(text.height, s)});
  std::set<std::string> labelled;
  for (const db::Label& l : cell->labels()) {
    CHECK(l.is_port());
    CHECK(l.sticky);
    labelled.insert(l.text);
  }
  CHECK(labelled == text.pins);
}

}  // namespace

TEST_CASE("the sky130_fd_sc_hd LEF reads at the pin", PDK_TAGS) {
  read_pdk(hefesto::test::kSky130FdScHdLef, "sky130_fd_sc_hd__inv_1");
}

TEST_CASE("the gf180mcu_fd_sc_mcu7t5v0 LEF reads at the pin", PDK_TAGS) {
  read_pdk(hefesto::test::kGf180mcu7t5v0Lef, "gf180mcu_fd_sc_mcu7t5v0__inv_1");
}

TEST_CASE("the sg13g2_stdcell LEF reads at the pin", PDK_TAGS) {
  read_pdk(hefesto::test::kSg13g2StdcellLef, "sg13g2_inv_1");
}
