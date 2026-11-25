// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the extract/hier tests (openspec/changes/m7-extraction tasks.md
// 8.2-8.5, 7.7): the hierarchical EX cases' technology and cells
// (tests/golden/EXTRACT/cases/EX-*/input/*.mag, read with their `use`
// blocks), the cell's own node pass (its slots kept, as extract_cell leaves
// them) and extract_subtree's records, with the captured `.ext` files to
// compare against.
//
// Owner: EXTHIER.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/hier/subtree.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::exthier {

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

inline std::string case_root(std::string_view id) {
  return std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/EXTRACT/cases/" + std::string{id} + "/";
}

inline std::vector<std::string> words(const std::string& line) {
  std::istringstream is(line);
  std::vector<std::string> out;
  for (std::string w; is >> w;) {
    out.push_back(w);
  }
  return out;
}

/// The lines of `text` that start with `prefix`.
inline std::vector<std::string> lines_with(const std::string& text, std::string_view prefix) {
  std::istringstream is(text);
  std::vector<std::string> out;
  for (std::string line; std::getline(is, line);) {
    if (line.rfind(prefix, 0) == 0) {
      out.push_back(line);
    }
  }
  return out;
}

/// The captured `.ext` of cell `cell` in case `id`.
inline std::string captured_ext(std::string_view id, std::string_view cell = "ex") {
  return read_file(case_root(id) + "expected/collected/input/" + std::string{cell} + ".ext");
}

inline extract::ExtStyle first_style(const tech::Technology& t, bool magic_compat) {
  const tech::RawSection* section = nullptr;
  for (const tech::RawSection& raw : t.raw_sections()) {
    if (raw.name == "extract") {
      section = &raw;
    }
  }
  REQUIRE(section != nullptr);
  diag::CollectingSink sink;
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  const extract::StyleTable table = extract::load_styles(*section, t, sink, c);
  REQUIRE_FALSE(table.names().empty());
  const auto style = table.select(table.names().front());
  REQUIRE(style != nullptr);
  const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(t);
  return style->micron_convert(cif ? &*cif : nullptr);
}

/// One hierarchical EX case: its technology, style and cells.
class HierCase {
 public:
  HierCase(std::string_view id, bool magic_compat) : HierCase(id, magic_compat, case_root(id) + "input/") {}

  /// The technology of case `id` with the cells read from `cells_dir`.
  HierCase(std::string_view id, bool magic_compat, std::string cells_dir)
      : dir_(case_root(id) + "input/"), cells_dir_(std::move(cells_dir)) {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(read_file(dir_ + "exttest.tech"), "/t/exttest.tech", fs,
                                             tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    tech_ = loaded.value();
    style_ = std::make_shared<const extract::ExtStyle>(first_style(*tech_, magic_compat));
    compat_.magic_compat = magic_compat;
  }

  [[nodiscard]] const tech::Technology& tech() const { return *tech_; }
  [[nodiscard]] const extract::ExtStyle& style() const { return *style_; }
  [[nodiscard]] std::shared_ptr<const extract::ExtStyle> style_ptr() const { return style_; }
  [[nodiscard]] const compat::CompatSettings& compat() const { return compat_; }
  extract::ExtractOptions options;
  const mask::Progress* progress = nullptr;  // ExtractContext::progress of run()

  /// The cell `name` read from input/<name>.mag with its subcells.
  db::CellDef& cell(const std::string& name) {
    if (db::CellDef* found = lib_.find(name)) {
      return *found;
    }
    auto created = lib_.create(name, tech_->plane_count());
    REQUIRE(created.ok());
    db::CellDef& def = *created.value();
    read_mag(def, read_file(cells_dir_ + name + ".mag"));
    db::recompute_bbox(def);
    return def;
  }

  /// extract_subtree on `name` after its own node pass; the `merge` and
  /// `cap` text, and the result.
  struct Run {
    std::string text;
    extract::SubtreeResult result;
  };
  Run run(const std::string& name) {
    db::CellDef& top = cell(name);
    extract::RegionFinder finder(*tech_);
    finder.keep_clients = true;
    auto found = finder.find_nodes(top, *style_, pass_options(top));
    REQUIRE(found.ok());
    extract::NodeRegionList nodes = std::move(found).value();
    extract::attach_labels(top, finder, nodes, *style_, *tech_, extract::LabelPassOptions{}, compat_);
    const std::string subs = extract::substrate_name(style_->substrate.node_name,
                                                     [](std::string_view) { return std::optional<std::string>{}; });
    std::optional<std::size_t> substrate;
    if (finder.substrate_region()) {
      substrate = extract::node_index(nodes.size(), *finder.substrate_region() - finder.first_region());
    }
    std::vector<std::string> names;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      const extract::LabelRegion* lab = extract::name_label(nodes[i]);
      names.push_back(lab != nullptr ? lab->text
                                     : extract::number_name(nodes, i, *tech_, extract::SubstrateName{substrate, subs}));
    }
    extract::ExtractContext ctx;
    ctx.style = style_;
    ctx.options = options;
    ctx.compat = compat_;
    ctx.progress = progress;
    extract::SubtreeInputs in;
    in.technology = tech_.get();
    in.parent = extract::ParentNodes{&finder, nodes.size(), names, substrate};
    in.substrate_name = subs;
    extract::ExtWriter writer;
    extract::ExtHeader header;
    header.resist_classes = extract::resistance_class_header(*style_);
    writer.header(header);
    auto out = extract::extract_subtree(ctx, top, writer, in);
    REQUIRE(out.ok());
    extract::reset(top);
    auto text = writer.finish();
    REQUIRE(text.ok());
    return Run{text.value(), out.value()};
  }

 private:
  extract::NodePassOptions pass_options(const db::CellDef& def) const {
    extract::NodePassOptions o;
    o.magic_compat = compat_.magic_compat;
    o.resistance = options.lumped;
    const extract::SubstrateSeeds seeds = extract::substrate_seeds(def, *tech_, *style_);
    for (const auto& [plane, tile] : seeds.tiles) {
      o.substrate_seeds.push_back(extract::RegionSeed{plane, {tile.area.x0, tile.area.y0}, tile.right_side});
    }
    if (seeds.tiles.empty() && seeds.tileless) {
      o.substrate_without_tiles = style_->substrate.plane;
    }
    return o;
  }

  tech::TileType type_of(const std::string& name) const {
    const auto found = tech_->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  struct PendingUse {
    std::string cell;
    std::string id;
    db::Transform transform;
    db::ArrayInfo array;
  };

  void place(db::CellDef& parent, const PendingUse& u) {
    db::CellDef& child = cell(u.cell);
    db::PlaceOptions o;
    o.id = u.id;
    o.transform = u.transform;
    o.array = u.array;
    REQUIRE(db::place(parent, child, o).ok());
  }

  void read_label(db::CellDef& def, const std::vector<std::string>& w) {
    db::Label l;
    std::size_t i = 1;
    l.type = type_of(w[i++]);
    if (w[i] == "s") {
      l.sticky = true;
      ++i;
    }
    l.rect = {std::stoi(w[i]), std::stoi(w[i + 1]), std::stoi(w[i + 2]), std::stoi(w[i + 3])};
    l.text = w[i + 5];
    def.labels().push_back(l);
  }

  void read_mag(db::CellDef& def, const std::string& text) {
    std::istringstream is(text);
    std::string section;
    std::optional<PendingUse> use;
    for (std::string line; std::getline(is, line);) {
      const std::vector<std::string> w = words(line);
      if (w.empty()) {
        continue;
      }
      if (w[0] == "use") {
        if (use) {
          place(def, *use);
        }
        use = PendingUse{w[1], w[2], {}, {}};
      } else if (w[0] == "array" && use) {
        use->array = {std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[4]), std::stoi(w[5]), std::stoi(w[3]),
                      std::stoi(w[6])};
      } else if (w[0] == "transform" && use) {
        use->transform = {std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]),
                          std::stoi(w[4]), std::stoi(w[5]), std::stoi(w[6])};
      } else if (w[0] == "<<") {
        if (use) {
          place(def, *use);
          use.reset();
        }
        section = w[1];
      } else if (w[0] == "rect" && w.size() == 5) {
        const db::Rect r{std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]), std::stoi(w[4])};
        REQUIRE(db::paint(def, *tech_, r, type_of(section)).ok());
      } else if (w[0] == "rlabel") {
        read_label(def, w);
      } else if (w[0] == "port" && !def.labels().empty()) {
        def.labels().back().port_directions = 0x0F;
      }
    }
  }

  std::string dir_;
  std::string cells_dir_;
  std::shared_ptr<const tech::Technology> tech_;
  std::shared_ptr<const extract::ExtStyle> style_;
  compat::CompatSettings compat_;
  db::CellLibrary lib_;
};

}  // namespace hefesto::test::exthier
