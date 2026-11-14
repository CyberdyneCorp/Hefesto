// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The standard-cell library GDS of each data set (tasks.md 9.8):
// sky130_fd_sc_hd, gf180mcu_fd_sc_mcu7t5v0 and sg13g2_stdcell read with
// their PDK's technology at the M4 pins, in both magic-compat settings,
// without a read error and with every structure becoming a defined cell.
// The macos-asan preset and the Linux ASan leg run them under the
// sanitizers. The cases skip when the GDS data set is not installed
// (scripts/fetch_pdk.py sky130-gds-sc gf180mcu-gds-sc ihp-gds-sc).
// Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/gds/read/stamps.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace gds = hefesto::gds;
namespace mask = hefesto::mask;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace fs = std::filesystem;

namespace {

struct Library {
  const hefesto::test::PinnedGds* gds;
};

/// The structure names of a GDS file.
std::set<std::string> structure_names(const fs::path& file) {
  platform::LocalFileSystem files;
  auto text = files.read(file);
  REQUIRE(text.ok());
  std::vector<std::byte> bytes(text.value().size());
  std::copy_n(reinterpret_cast<const std::byte*>(text.value().data()), bytes.size(), bytes.begin());
  std::set<std::string> names;
  for (const gds::StructureStamp& s : gds::content_stamps(bytes)) {
    names.insert(s.name);
  }
  return names;
}

void read_library(const hefesto::test::PinnedGds& pinned, bool magic_compat) {
  const auto tech_file = hefesto::test::pinned_tech(*pinned.pdk);
  const auto gds_file = hefesto::test::pinned_gds(pinned);
  if (!tech_file || !gds_file) {
    SKIP("library GDS data set not installed: " << pinned.library);
  }
  INFO(pinned.library << ", magic-compat " << (magic_compat ? "ON" : "OFF"));
  platform::LocalFileSystem files;
  diag::CollectingSink quiet;
  auto loaded = tech::load_technology(files, tech_file->string(), tech::LoadOptions{}, quiet);
  REQUIRE(loaded.ok());
  hefesto::compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  hefesto::editor::EditorState editor;
  editor.drc.compat = &compat;
  editor.technology = loaded.value();
  editor.grid = tech::apply_post_load(*editor.technology, editor.grid, quiet);
  editor.mask.output_styles = mask::read_output_styles(*editor.technology, quiet, compat);
  if (editor.grid.lambda != editor.grid.internal) {
    (void)mask::rescale_output_styles(editor.mask.output_styles, static_cast<std::int32_t>(editor.grid.lambda),
                                      static_cast<std::int32_t>(editor.grid.internal), compat);
  }
  editor.mask.input_styles = mask::read_input_styles(*editor.technology, compat, quiet);
  (void)mask::rescale_input_styles(editor.mask.input_styles, editor.grid.lambda, editor.grid.internal, compat);
  REQUIRE(editor.mask.input_styles.current_style() != nullptr);

  gds::GdsReadOptions options;
  options.time_zone = "UTC0";
  options.settings.datestamp = 0;
  const hefesto::cmd::CommandResult result = gds::read(editor, *gds_file, options);
  REQUIRE(result.ok());
  std::vector<std::string> errors;
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.severity == diag::Severity::error) {
      errors.push_back(d.code + ": " + d.text);
    }
  }
  CHECK(errors.empty());
  for (const std::string& name : structure_names(*gds_file)) {
    const hefesto::db::CellDef* def = editor.cells.find(name);
    INFO("structure " << name);
    REQUIRE(def != nullptr);
    CHECK(def->flags().available);
  }
}

}  // namespace

TEST_CASE("standard-cell library GDS reads without errors", "[cap:gds-read][req:Structure processing and cell creation]") {
  for (const hefesto::test::PinnedGds* pinned :
       {&hefesto::test::kSky130FdScHdGds, &hefesto::test::kGf180mcu7t5v0Gds, &hefesto::test::kSg13g2StdcellGds}) {
    for (const bool magic_compat : {true, false}) {
      read_library(*pinned, magic_compat);
    }
  }
}
