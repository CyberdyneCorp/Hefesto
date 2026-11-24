// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the extract/cap tests (openspec/changes/m7-extraction tasks.md
// 6.1-6.7): the EXTRACT golden cases' technology (tools/golden/authoring/
// exttest.tech, its planes, types, contacts and connectivity) with a
// test's own `extract` style lines, a cell painted by hand, and a small
// stand-in for EXTNODE's RegionFinder that floods each connected region
// and leaves its index in the tiles' client slots (design D6), as the
// accumulators read them. The slots are reset when the fixture goes away.
//
// Owner: EXTCAP.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/cap/caps.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::cap {

namespace extract = hefesto::extract;
namespace tech = hefesto::tech;
namespace db = hefesto::db;

// The base style lines every EX case starts from (exttest.tech).
inline const std::string kBase =
    " cscale 1\n rscale 1\n lambda 1\n step 100\n sidehalo 0\n"
    " planeorder well 0\n planeorder active 1\n planeorder metal1 2\n planeorder metal2 3\n"
    " planeorder metal3 4\n planeorder mimcap 5\n planeorder oxide 6\n";

inline std::string exttest_tech(const std::string& extract_section) {
  return "tech\n format 35\n exttest\nend\n"
         "planes\n well\n active\n metal1\n metal2\n metal3\n mimcap\n oxide\nend\n"
         "types\n well pwell,pw\n well nwell,nw\n well dnwell,dnw\n active ndiff,nd\n active pdiff,pd\n"
         " active psd\n active ndiffh\n active nfet\n active pfet\n active poly,p\n active rpoly\n"
         " active ndc\n active m1c\n metal1 metal1,m1\n metal1 via\n metal2 metal2,m2\n metal3 metal3,m3\n"
         " mimcap mimcap\n oxide thkox\nend\n"
         "contact\n ndc ndiff metal1\n m1c poly metal1\n via metal1 metal2\nend\n"
         "styles\n styletype mos\nend\n"
         "compose\nend\n"
         "connect\n pwell psd\n ndiff ndiffh,ndc\n ndiffh ndc\n poly m1c,nfet,pfet\n metal1 m1c,ndc,via\n"
         " metal2 via\nend\n"
         "cifoutput\nstyle lambda\n scalefactor 50\n layer M1 metal1,m1c,ndc,via\n calma 1 0\nend\n"
         "cifinput\nend\ndrc\nend\n"
         "extract\n" +
         extract_section + "end\n";
}

/// One style `exttest` made of the base lines and `rules`.
inline std::string style(const std::string& rules) {
  return "style exttest\n" + kBase + rules;
}

/// A technology, its finished extraction style and a cell to paint.
class Layout {
 public:
  explicit Layout(const std::string& extract_section, bool magic_compat = true) {
    compat_.magic_compat = magic_compat;
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(exttest_tech(extract_section), "/t/exttest.tech", fs,
                                             tech::LoadOptions{}, sink);
    for (const auto& d : sink.snapshot()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(loaded.ok());
    tech_ = loaded.value();
    const tech::RawSection* section = nullptr;
    for (const tech::RawSection& raw : tech_->raw_sections()) {
      if (raw.name == "extract") {
        section = &raw;
      }
    }
    REQUIRE(section != nullptr);
    diag::CollectingSink style_sink;
    table_ = extract::load_styles(*section, *tech_, style_sink, compat_);
    select(table_.names().front());
    cell_ = std::make_unique<db::CellDef>("ex", tech_->plane_count());
  }

  Layout(const Layout&) = delete;
  Layout& operator=(const Layout&) = delete;
  ~Layout() { reset_slots(); }

  /// Selects a style of the section (as `extract style`), finished as the
  /// tech-load hook does at a 1:1 grid.
  void select(std::string_view name) {
    const auto parsed = table_.select(name);
    REQUIRE(parsed != nullptr);
    const std::optional<mask::OutputStyle> cif = mask::read_first_output_style(*tech_);
    style_ = parsed->micron_convert(cif ? &*cif : nullptr);
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech_->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }
  [[nodiscard]] tech::PlaneId plane(std::string_view name) const {
    const auto found = tech_->find_plane(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  void paint(std::string_view type_name, db::Rect r) {
    REQUIRE(db::paint_type(*cell_, *tech_, r, type(type_name)).ok());
  }
  /// `tri x0 y0 x1 y1 <corner>` of a .mag file.
  void triangle(std::string_view type_name, db::Rect r, db::TriangleCorner corner) {
    const db::TriangleSide side = db::triangle_side(corner);
    REQUIRE(db::paint_type(*cell_, *tech_, r, type(type_name), db::TriangleSpec{side.diagonal, side.right_side})
                .ok());
  }

  /// RegionFinder's stand-in: floods every connected region of non-space
  /// tiles (node connectivity of the style, contacts across their planes)
  /// and stores the region index in the client slots.
  extract::NodeRegionList find_nodes() {
    reset_slots();
    extract::NodeRegionList nodes;
    tech::TypeMask material = tech::TypeMask::all();
    material.clear_type(tech::kSpace);
    for (std::size_t p = tech::kFirstUserPlane; p < tech_->plane_count(); ++p) {
      const auto pid = static_cast<tech::PlaneId>(p);
      std::vector<db::TileView> seeds;
      cell_->plane(pid).enumerate(db::whole_plane(), material, [&](const db::TileView& t) {
        seeds.push_back(t);
        return true;
      });
      for (const db::TileView& seed : seeds) {
        if (cell_->plane(pid).client(seed) != db::kClientDefault) {
          continue;
        }
        extract::NodeRegion n;
        n.reference = db::Point{seed.area.x0, seed.area.y0};
        n.plane = pid;
        n.type = seed.type;
        nodes.push_back(n);
        flood(pid, seed, nodes.size() - 1);
      }
    }
    return nodes;
  }

  /// Gives every tile of `plane` (space included) to node `index`: the
  /// substrate node of a `substrate` declaration, which EXTDEV prepares.
  void give_plane(std::string_view plane_name, std::size_t index) {
    const db::Plane& p = cell_->plane(plane(plane_name));
    p.enumerate(db::whole_plane(), tech::TypeMask::all(), [&](const db::TileView& t) {
      p.set_client(t, index);
      return true;
    });
  }

  /// The node index of the tile under (x, y) on the plane of `type_name`.
  [[nodiscard]] std::size_t node_at(std::string_view plane_name, db::Coord x, db::Coord y) const {
    const db::Plane& p = cell_->plane(plane(plane_name));
    const std::uintptr_t c = p.client(p.tile_at(db::Point{x, y}));
    REQUIRE(c != db::kClientDefault);
    return static_cast<std::size_t>(c);
  }

  [[nodiscard]] extract::CapInputs inputs() const {
    extract::CapInputs in;
    in.cell = cell_.get();
    in.style = &style_;
    in.compat = &compat_;
    in.technology = tech_.get();
    in.options = &options_;
    return in;
  }

  extract::ExtractOptions& options() { return options_; }
  [[nodiscard]] const extract::ExtStyle& style() const { return style_; }
  [[nodiscard]] const tech::Technology& technology() const { return *tech_; }
  [[nodiscard]] db::CellDef& cell() { return *cell_; }
  [[nodiscard]] const compat::CompatSettings& compat() const { return compat_; }

  /// substrate_caps then find_coupling, as extBasic does.
  struct Result {
    extract::NodeRegionList nodes;
    extract::CouplingTable coupling;
  };
  Result extract_all() {
    Result r;
    r.nodes = find_nodes();
    REQUIRE(extract::substrate_caps(r.nodes, inputs()).ok());
    REQUIRE(extract::find_coupling(r.nodes, inputs(), r.coupling).ok());
    return r;
  }

 private:
  void reset_slots() {
    if (!cell_) {
      return;
    }
    for (std::size_t p = 0; p < cell_->plane_count(); ++p) {
      cell_->plane(static_cast<tech::PlaneId>(p)).reset_clients(db::whole_plane());
    }
  }

  [[nodiscard]] const tech::TypeMask& connects(tech::TileType t) const {
    return t < style_.node_conn.size() ? style_.node_conn[t] : tech_->connects_to(t);
  }

  void flood(tech::PlaneId plane0, const db::TileView& seed, std::size_t index) {
    std::vector<std::pair<tech::PlaneId, db::TileView>> stack{{plane0, seed}};
    std::vector<db::TileView> out;
    while (!stack.empty()) {
      const auto [pid, t] = stack.back();
      stack.pop_back();
      const db::Plane& p = cell_->plane(pid);
      if (p.client(t) != db::kClientDefault) {
        continue;
      }
      p.set_client(t, index);
      const tech::TileType type = t.split ? (t.right_side ? t.right_type : t.left_type) : t.type;
      for (const db::Side side : {db::Side::top, db::Side::left, db::Side::bottom, db::Side::right}) {
        const bool vertical = side == db::Side::left || side == db::Side::right;
        p.neighbors(t, side, vertical ? t.area.y0 : t.area.x0, vertical ? t.area.y1 : t.area.x1, out);
        for (const db::TileView& n : out) {
          if (!n.split && connects(type).test(n.type) && p.client(n) == db::kClientDefault) {
            stack.emplace_back(pid, n);
          }
        }
      }
      if (!tech_->is_contact(type)) {
        continue;
      }
      for (std::size_t q = tech::kFirstUserPlane; q < tech_->plane_count(); ++q) {
        const auto qid = static_cast<tech::PlaneId>(q);
        if (qid == pid || (tech_->type_planes(type) & tech::plane_bit(qid)) == 0) {
          continue;
        }
        const db::TileView other = cell_->plane(qid).tile_at(db::Point{t.area.x0, t.area.y0});
        if (connects(type).test(other.type) && cell_->plane(qid).client(other) == db::kClientDefault) {
          stack.emplace_back(qid, other);
        }
      }
    }
  }

  compat::CompatSettings compat_;
  std::shared_ptr<const tech::Technology> tech_;
  extract::StyleTable table_;
  extract::ExtStyle style_;
  extract::ExtractOptions options_;
  std::unique_ptr<db::CellDef> cell_;
};

/// C's `%lg` of a value (6 significant digits), as the `.ext` writer prints it.
inline std::string lg(double v) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%lg", v);
  return buf;
}

}  // namespace hefesto::test::cap
