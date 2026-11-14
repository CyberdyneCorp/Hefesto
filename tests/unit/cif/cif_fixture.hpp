// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the cif/write and cif/command unit tests: the technology
// tests/data/cif/masktest.tech (output style `gds`: scalefactor 10, reducer
// 10, expander 1; layer CMF = metal1 with metal1 labels; input style `gds`),
// an editor session with its styles installed, cells built in memory, and
// writes into a memory file system under TZ "UTC0" at a fixed time.
// Owner: CIF.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cif/write/write.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace cif_test {

namespace cif = hefesto::cif;
namespace cmd = hefesto::cmd;
namespace compat = hefesto::compat;
namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace mask = hefesto::mask;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;

/// The current time of the tests' `@@date` line: Tue Nov 14 22:13:20 2023 UTC.
inline constexpr std::int64_t kNow = 1700000000;

inline std::string masktest_text() {
  std::ifstream in{std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/cif/masktest.tech", std::ios::binary};
  REQUIRE(in.good());
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// The lines of `text` (without line ends).
inline std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in{text};
  for (std::string line; std::getline(in, line);) {
    out.push_back(line);
  }
  return out;
}

/// The lines after the preamble (the `( @@... );` comments).
inline std::vector<std::string> body(const std::string& text) {
  std::vector<std::string> out;
  for (std::string& line : lines_of(text)) {
    if (!line.starts_with("( @@")) {
      out.push_back(std::move(line));
    }
  }
  return out;
}

struct Fixture {
  compat::CompatSettings compat;
  hefesto::editor::EditorState ed;
  platform::MemoryFileSystem fs;
  cmd::CommandResult last = cmd::CommandResult::success();

  explicit Fixture(bool magic_compat = true) {
    compat.magic_compat = magic_compat;
    diag::CollectingSink quiet;
    platform::MemoryFileSystem tech_fs;
    auto loaded =
        tech::load_technology_text(masktest_text(), "/t/masktest.tech", tech_fs, tech::LoadOptions{}, quiet);
    REQUIRE(loaded.ok());
    ed.technology = loaded.value();
    ed.drc.compat = &compat;
    ed.mask.output_styles = mask::read_output_styles(*ed.technology, quiet, compat);
    ed.mask.input_styles = mask::read_input_styles(*ed.technology, compat, quiet);
    REQUIRE(ed.mask.output_styles.current_style() != nullptr);
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = ed.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  db::CellDef& cell(const std::string& name) {
    auto made = ed.cells.create(name, ed.technology->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, std::string_view type_name, const db::Rect& r,
             std::optional<db::TriangleSpec> triangle = std::nullopt) {
    REQUIRE(db::paint_type(def, *ed.technology, r, type(type_name), triangle).ok());
    db::recompute_bbox(def);
  }

  void label(db::CellDef& def, std::string text, const db::Rect& r, std::string_view type_name) {
    db::Label l;
    l.text = std::move(text);
    l.rect = r;
    l.type = type(type_name);
    REQUIRE(db::put_label(def, std::move(l)).ok());
  }

  db::CellUse& place(db::CellDef& parent, db::CellDef& child, db::Transform t = {}, db::ArrayInfo array = {},
                     std::optional<std::string> id = {}) {
    db::PlaceOptions o;
    o.transform = t;
    o.array = array;
    o.id = std::move(id);
    auto u = db::place(parent, child, o);
    REQUIRE(u.ok());
    db::recompute_bbox(parent);
    return *u.value();
  }

  [[nodiscard]] cif::CifWriteOptions options(bool flat = false) {
    cif::CifWriteOptions o;
    o.settings = ed.mask.cif;
    o.flat = flat;
    o.typed_name = "out";
    o.files = &fs;
    o.time_zone = "UTC0";
    o.now = kNow;
    return o;
  }

  /// Writes `root` to /out.cif; the file (empty when none was made).
  std::string write(db::CellDef& root, bool flat = false) { return write_with(root, options(flat)); }

  std::string write_with(db::CellDef& root, const cif::CifWriteOptions& o) {
    last = cif::write(ed, root, "/out.cif", o);
    return file("/out.cif");
  }

  [[nodiscard]] std::string file(const std::string& path) {
    if (!fs.exists(path)) {
      return {};
    }
    auto read = fs.read(path);
    REQUIRE(read.ok());
    return read.value();
  }

  [[nodiscard]] bool has_code(std::string_view code) const {
    for (const auto& d : last.diagnostics) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const auto& d : last.diagnostics) {
      out.push_back(d.text);
    }
    return out;
  }
};

}  // namespace cif_test
