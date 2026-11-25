// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers for the extract/node tests (openspec/changes/m7-extraction
// tasks.md 5.1-5.4): the EXTRACT golden cases' technology and layout
// (tests/golden/EXTRACT/cases/EX-*/input), a flat extraction of one cell
// through the node pass, label attachment and the node records, and the
// read-back of the emitted text with extflat's reader. Owner: EXTNODE.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::extnode {

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace db = hefesto::db;
namespace diag = hefesto::diag;

inline std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

inline std::string case_input(std::string_view id, std::string_view file) {
  return std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/EXTRACT/cases/" + std::string{id} + "/input/" +
         std::string{file};
}

inline std::string case_expected(std::string_view id) {
  return read_file(std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/EXTRACT/cases/" + std::string{id} +
                   "/expected/collected/input/ex.ext");
}

inline std::shared_ptr<const tech::Technology> load_tech_text(const std::string& text) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/exttest.tech", fs, tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// The technology of an EX case.
inline std::shared_ptr<const tech::Technology> case_tech(std::string_view id) {
  return load_tech_text(read_file(case_input(id, "exttest.tech")));
}

/// The first extraction style, micron-converted with the first cifoutput
/// style, as the tech-load hook installs it.
inline extract::ExtStyle first_style(const tech::Technology& t, bool magic_compat = true) {
  const tech::RawSection* section = nullptr;
  for (const tech::RawSection& raw : t.raw_sections()) {
    if (raw.name == "extract") {
      section = &raw;
    }
  }
  REQUIRE(section != nullptr);
  diag::CollectingSink sink;
  compat::CompatSettings compat;
  compat.magic_compat = magic_compat;
  const extract::StyleTable table = extract::load_styles(*section, t, sink, compat);
  REQUIRE_FALSE(table.names().empty());
  const auto style = table.select(table.names().front());
  REQUIRE(style != nullptr);
  const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(t);
  return style->micron_convert(cif ? &*cif : nullptr);
}

inline tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

inline std::unique_ptr<db::CellDef> new_cell(const tech::Technology& t, std::string name = "ex") {
  auto cell = std::make_unique<db::CellDef>(std::move(name), t.plane_count());
  cell->set_timestamp(100);
  return cell;
}

inline void paint(db::CellDef& cell, const tech::Technology& t, std::string_view type, db::Rect r) {
  REQUIRE(db::paint(cell, t, r, type_of(t, type)).ok());
}

/// The `rect` and `tri` lines of an EX case's `.mag` file.
inline std::unique_ptr<db::CellDef> case_cell(std::string_view id, const tech::Technology& t) {
  auto cell = new_cell(t);
  std::istringstream in(read_file(case_input(id, "ex.mag")));
  std::string line;
  std::string layer;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string key;
    words >> key;
    if (key == "<<") {
      words >> layer;
    } else if (key == "rect") {
      db::Rect r;
      words >> r.x0 >> r.y0 >> r.x1 >> r.y1;
      paint(*cell, t, layer, r);
    } else if (key == "tri") {
      db::Rect r;
      std::string corner;
      words >> r.x0 >> r.y0 >> r.x1 >> r.y1 >> corner;
      const db::TriangleCorner c = corner == "ne"   ? db::TriangleCorner::ne
                                   : corner == "nw" ? db::TriangleCorner::nw
                                   : corner == "se" ? db::TriangleCorner::se
                                                    : db::TriangleCorner::sw;
      const db::TriangleSide side = db::triangle_side(c);
      REQUIRE(db::paint_triangle(*cell, t, r, side.diagonal, side.right_side, type_of(t, layer)).ok());
    }
  }
  return cell;
}

inline void label(db::CellDef& cell, const tech::Technology& t, std::string text, std::string_view type, db::Rect r,
                  std::uint32_t port = 0) {
  db::Label l;
  l.text = std::move(text);
  l.type = type_of(t, type);
  l.rect = r;
  if (port != 0) {
    l.port_directions = db::kPortNorth;
    l.port_index = port;
  }
  cell.labels().push_back(std::move(l));
}

inline compat::CompatSettings compat_mode(bool on) {
  compat::CompatSettings c;
  c.magic_compat = on;
  return c;
}

/// The nodes of `cell` with their labels (the node pass with its slots
/// kept, label attachment, reset).
inline extract::NodeRegionList nodes_of(const db::CellDef& cell, const tech::Technology& t,
                                        const extract::ExtStyle& style, bool magic_compat = true,
                                        extract::NodePassOptions options = {}) {
  extract::RegionFinder finder(t);
  finder.keep_clients = true;
  options.magic_compat = magic_compat;
  auto found = finder.find_nodes(cell, style, options);
  REQUIRE(found.ok());
  extract::NodeRegionList nodes = std::move(found).value();
  extract::attach_labels(cell, finder, nodes, style, t, extract::LabelPassOptions{}, compat_mode(magic_compat));
  extract::reset(cell);
  REQUIRE(extract::clients_clear(cell));
  return nodes;
}

inline extract::ExtHeader header_of(const db::CellDef& cell, const tech::Technology& t, const extract::ExtStyle& s) {
  extract::ExtHeader h;
  h.timestamp = cell.timestamp();
  h.tech = t.name();
  h.style = s.name;
  h.rscale = static_cast<std::int32_t>(s.rscale);
  h.cscale = static_cast<std::int32_t>(s.cscale);
  h.units_per_lambda = s.units_per_lambda;
  for (const extract::ResistClass& c : s.resist_classes) {
    h.resist_classes.push_back(static_cast<std::int64_t>(c.sheet_resistance));
  }
  return h;
}

struct Flat {
  std::string text;
  extract::NodeRecordResult records;
  diag::CollectingSink sink;
};

/// A flat `.ext` of `cell`: header and node records (R and C from
/// `options`, EXTCAP's), as extOutputNodes writes them.
inline void extract_flat(Flat& out, db::CellDef& cell, const tech::Technology& t, const extract::ExtStyle& style,
                         bool magic_compat = true, extract::NodeRecordOptions options = {}) {
  extract::NodeRegionList nodes = nodes_of(cell, t, style, magic_compat);
  extract::ExtWriter writer;
  writer.header(header_of(cell, t, style));
  out.records = extract::write_node_records(writer, nodes, cell, t, options, compat_mode(magic_compat), out.sink);
  auto text = writer.finish();
  REQUIRE(text.ok());
  out.text = text.value();
}

/// Reads `text` back as `<cell>.ext` with extflat's reader; returns the
/// messages it printed.
inline std::vector<diag::Diagnostic> read_back(std::string_view cell, const std::string& text) {
  platform::MemoryFileSystem fs;
  REQUIRE(fs.write_atomic("/w/" + std::string{cell} + ".ext", text).ok());
  cell::SearchPaths paths{{"/w"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};
  auto read = extflat::read(ctx, cell, extflat::ReadOptions{});
  REQUIRE(read.ok());
  std::vector<diag::Diagnostic> messages = sink.take();
  for (const diag::Diagnostic& d : messages) {
    UNSCOPED_INFO("read back: " << d.code << ": " << d.text);
  }
  return messages;
}

/// The lines of `text` starting with `prefix`.
inline std::vector<std::string> lines_with(const std::string& text, std::string_view prefix) {
  std::vector<std::string> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.starts_with(prefix)) {
      out.push_back(line);
    }
  }
  return out;
}

}  // namespace hefesto::test::extnode
