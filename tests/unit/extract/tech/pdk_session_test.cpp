// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract-tech-section on the three pinned PDKs through the editor's
// tech-load hook (openspec/changes/m7-extraction tasks.md 4.7, design D7
// "No new diagnostics on PDKs"): the extract section's messages of a tech
// load are exactly the lines Magic prints for it in the golden cases'
// captured stdout (tests/golden/LEFDEF/cases/LEFDEF-RT-*), and the
// session's current style is the first one, as StyleView and HeightView
// see it. Skipped when the PDK is not in .cache/pdks. Owner: EXTTECH.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/editor/extract_hook.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace {

namespace diag = hefesto::diag;
namespace extract = hefesto::extract;
namespace tech = hefesto::tech;

// The extract-section lines of a golden case's captured Magic stdout: the
// non-electrical list and the line after it, joined as one message.
std::vector<std::string> golden_extract_lines(std::string_view case_id) {
  const std::filesystem::path path = std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/LEFDEF/cases" /
                                     std::string{case_id} / "expected/stdout.txt";
  std::ifstream in(path);
  REQUIRE(in.good());
  std::vector<std::string> out;
  std::string line;
  while (std::getline(in, line)) {
    if (line.starts_with("The following types are not handled by extraction")) {
      std::string types;
      std::getline(in, types);
      out.push_back(line + "\n" + types);
    }
  }
  return out;
}

void check_pdk(const std::optional<std::filesystem::path>& file, std::string_view case_id) {
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  hefesto::platform::LocalFileSystem files;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  hefesto::editor::EditorState editor;
  editor.technology = loaded.value();
  diag::CollectingSink sink;
  hefesto::editor::install_extract_styles(editor, sink);
  std::vector<std::string> messages;
  for (const auto& d : sink.take()) {
    UNSCOPED_INFO(d.code << ": " << d.text);
    messages.push_back(d.text);
  }
  CHECK(messages == golden_extract_lines(case_id));
  REQUIRE(editor.extract.current != nullptr);
  CHECK(editor.extract.current->name == "ngspice()");
  const extract::StyleView view = extract::StyleView::from_style(editor.extract.current);
  CHECK(view.style_name() == "ngspice()");
  CHECK_FALSE(view.gate_types()->empty());
  CHECK(extract::HeightView::from_style(*editor.extract.current).style_name() == "ngspice()");
}

TEST_CASE("sky130A's tech load reports exactly Magic's extract-section lines",
          "[cap:extract-tech-section][req:Non-electrical types]") {
  check_pdk(hefesto::test::pinned_sky130a_tech(), "LEFDEF-RT-sky130_fd_sc_hd");
}

// sky130A.magicrc runs `scalegrid 1 2` before `tech load`, so the
// cifoutput style is installed at half-lambda units; the micron conversion
// must still use microns per lambda (0.01), as Magic's TechLoad does. With
// the installed style (0.005) the `w<0.42` limit of special_nfet_01v8 became
// 166 internal units instead of 82, and every scnfet of width 0.42-0.83 um
// was written as special_nfet_01v8 (EXTRACT-PDK-sky130_fd_sc_hd).
TEST_CASE("sky130A on its half-lambda grid converts the w<0.42 limit of special_nfet_01v8 to 82 internal units",
          "[cap:extract-tech-section][req:Micron conversion]") {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("PDK tech file not in .cache/pdks");
  }
  hefesto::platform::LocalFileSystem files;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology(files, file->string(), tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  const tech::Technology& t = *loaded.value();
  const tech::RawSection* section = nullptr;
  for (const tech::RawSection& raw : t.raw_sections()) {
    if (raw.name == "extract") {
      section = &raw;
    }
  }
  REQUIRE(section != nullptr);
  for (const bool on : {true, false}) {
    CAPTURE(on);
    hefesto::compat::CompatSettings compat;
    compat.magic_compat = on;
    const extract::StyleTable table = extract::load_styles(*section, t, sink, compat);
    const auto parsed = table.select(table.names().front());
    REQUIRE(parsed != nullptr);
    const std::optional<hefesto::mask::OutputStyle> cif = hefesto::mask::read_first_output_style(t);
    REQUIRE(cif.has_value());
    const auto installed = cif->rescale(1, 2, compat);
    const extract::ExtStyle style = parsed->micron_convert(installed.get(), 1, 2, compat, sink).rescale(1, 2, compat);
    CHECK(style == parsed->micron_convert(&*cif).rescale(1, 2, compat));
    const auto scnfet = t.find_type("scnfet");
    REQUIRE(scnfet.ok());
    const auto records = style.records_for(*scnfet.id);
    REQUIRE(records.size() == 2);
    CHECK(records[0]->model == "sky130_fd_pr__special_nfet_01v8");
    CHECK(records[1]->model == "sky130_fd_pr__nfet_01v8");
    double maximum = -1;
    for (const extract::DeviceParam& p : records[0]->params) {
      if (p.code == "w" && p.maximum >= p.minimum) {
        maximum = p.maximum;
      }
    }
    // ON: 419 / 0.01 = 41900 -> 41 lambda -> 82 internal units. OFF keeps
    // milli-units without truncation: about 41900 milli-lambda -> 83800.
    if (on) {
      CHECK(maximum == 82.0);
    } else {
      CHECK(std::abs(maximum - 83800.0) < 0.1);
    }
  }
}

TEST_CASE("gf180mcuD's tech load reports exactly Magic's extract-section lines",
          "[cap:extract-tech-section][req:Non-electrical types]") {
  check_pdk(hefesto::test::pinned_gf180mcud_tech(), "LEFDEF-RT-gf180mcu_fd_sc_mcu7t5v0");
}

TEST_CASE("ihp-sg13g2's tech load reports exactly Magic's extract-section lines",
          "[cap:extract-tech-section][req:Non-electrical types]") {
  check_pdk(hefesto::test::pinned_ihp_sg13g2_tech(), "LEFDEF-RT-sg13g2_stdcell");
}

}  // namespace
