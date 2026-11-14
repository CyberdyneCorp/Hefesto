// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Randomized write-then-read property tests (m5-mask-gds-cif task 10.10):
// random hierarchies of Manhattan geometry, labels and uses, written with
// magic-compat OFF and `gds merge no` and `yes`, then read back through
// gds::read with the matching cifinput style
// (tests/data/gds/write/roundtrip.tech), give the same flattened geometry
// per type, the same labels and the same uses per cell. Hierarchical
// generation may move a parent's paint into its children's interaction
// areas, so geometry is compared flattened from the top cell.
//
// The seed comes from Catch2's --rng-seed (printed on failure); a failing
// seed goes into kRegressionSeeds, which every run replays.
// Owner: GDSW.

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <tuple>

#include "hefesto/db/use_index.hpp"
#include "hefesto/gds/read/read.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/tech/loader.hpp"
#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

// Seeds replayed on every run besides Catch2's: a fixed one, then the seeds
// that once failed (none so far).
constexpr std::array<std::uint32_t, 1> kRegressionSeeds{1};
constexpr int kLayoutsPerSeed = 12;
constexpr db::Coord kSpan = 40;  // cell contents lie in [0, kSpan)^2
constexpr std::array<std::string_view, 3> kTypes{"poly", "metal1", "metal2"};

std::shared_ptr<const tech::Technology> roundtrip_technology() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "roundtrip.tech");
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf();
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = hefesto::tech::load_technology_text(text.str(), "/t/roundtrip.tech", fs, hefesto::tech::LoadOptions{},
                                                    sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

// One session with the round-trip technology and both style sets.
struct Session {
  hefesto::editor::EditorState ed;
  compat::CompatSettings compat{false};

  explicit Session(const std::shared_ptr<const tech::Technology>& technology) {
    ed.technology = technology;
    ed.drc.compat = &compat;
    diag::CollectingSink quiet;
    ed.mask.output_styles = mask::read_output_styles(*technology, quiet, compat);
    ed.mask.input_styles = mask::read_input_styles(*technology, compat, quiet);
    REQUIRE(ed.mask.output_styles.current_style() != nullptr);
    REQUIRE(ed.mask.input_styles.current_style() != nullptr);
  }

  tech::TileType type(std::string_view name) const { return *ed.technology->find_type(name).id; }
};

constexpr std::array<db::Transform, 8> kOrientations{{
    {1, 0, 0, 0, 1, 0},
    {0, -1, 0, 1, 0, 0},
    {-1, 0, 0, 0, -1, 0},
    {0, 1, 0, -1, 0, 0},
    {-1, 0, 0, 0, 1, 0},
    {1, 0, 0, 0, -1, 0},
    {0, 1, 0, 1, 0, 0},
    {0, -1, 0, -1, 0, 0},
}};

// A random hierarchy: cells c0..c{n-1} and top, each cell using only
// earlier ones.
void build_layout(Session& s, std::mt19937& rng) {
  const auto pick = [&rng](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  std::vector<db::CellDef*> cells;
  const int count = pick(1, 4);
  for (int i = 0; i <= count; ++i) {
    const std::string name = i == count ? "top" : "c" + std::to_string(i);
    auto made = s.ed.cells.create(name, s.ed.technology->plane_count());
    REQUIRE(made.ok());
    db::CellDef& def = *made.value();
    def.flags().available = true;
    for (int r = pick(1, 6); r > 0; --r) {
      const db::Coord x = pick(0, kSpan - 2);
      const db::Coord y = pick(0, kSpan - 2);
      const db::Rect rect{x, y, pick(x + 1, kSpan), pick(y + 1, kSpan)};
      const tech::TileType t = s.type(kTypes[static_cast<std::size_t>(pick(0, 2))]);
      REQUIRE(db::paint_type(def, *s.ed.technology, rect, t).ok());
    }
    for (int l = pick(0, 2); l > 0; --l) {
      db::Label label;
      label.text = "n" + std::to_string(pick(0, 999));
      label.type = s.type(kTypes[static_cast<std::size_t>(pick(1, 2))]);
      const db::Coord x = pick(0, kSpan);
      const db::Coord y = pick(0, kSpan);
      label.rect = db::Rect{x, y, x, y};
      def.labels().push_back(label);
    }
    for (int u = cells.empty() ? 0 : pick(0, 3); u > 0; --u) {
      db::PlaceOptions o;
      o.transform = kOrientations[static_cast<std::size_t>(pick(0, 7))];
      o.transform.c = pick(-60, 60);
      o.transform.f = pick(-60, 60);
      if (pick(0, 3) == 0) {
        o.array = db::ArrayInfo{0, pick(0, 2), 0, pick(0, 2), pick(kSpan, 2 * kSpan), pick(kSpan, 2 * kSpan)};
      }
      REQUIRE(db::place(def, *cells[static_cast<std::size_t>(pick(0, static_cast<int>(cells.size()) - 1))], o).ok());
    }
    db::recompute_bbox(def);
    cells.push_back(&def);
  }
}

// The unit squares of type `type` flattened from `def` (in top coordinates).
using Squares = std::set<std::pair<db::Coord, db::Coord>>;

void flatten(const Session& s, const db::CellDef& def, const db::Transform& t, tech::TileType type, Squares& out,
             int depth) {
  REQUIRE(depth < 16);
  tech::TypeMask mask;
  mask.set(type);
  def.plane(*s.ed.technology->home_plane(type)).enumerate(db::whole_plane(), mask, [&](const db::TileView& v) {
    const db::Rect r = db::apply(t, v.area);
    for (db::Coord x = r.x0; x < r.x1; ++x) {
      for (db::Coord y = r.y0; y < r.y1; ++y) {
        out.emplace(x, y);
      }
    }
    return true;
  });
  for (const db::CellUse* use : db::uses_in_search_order(def, s.compat)) {
    for (std::int32_t i = use->array.xlo; i <= use->array.xhi; ++i) {
      for (std::int32_t j = use->array.ylo; j <= use->array.yhi; ++j) {
        const db::Transform element = db::compose(
            db::translation((i - use->array.xlo) * use->array.xsep, (j - use->array.ylo) * use->array.ysep),
            use->transform);
        flatten(s, *use->def, db::compose(t, element), type, out, depth + 1);
      }
    }
  }
}

using Numbers = std::array<std::int64_t, 6>;
using LabelKey = std::tuple<std::string, std::string, Numbers>;
using UseKey = std::tuple<std::string, std::string, Numbers, Numbers>;

Numbers numbers(const db::Rect& r) {
  return {r.x0, r.y0, r.x1, r.y1, 0, 0};
}
Numbers numbers(const db::Transform& t) {
  return {t.a, t.b, t.c, t.d, t.e, t.f};
}
// The separation of a single column or row is not stored in GDS.
Numbers numbers(const db::ArrayInfo& a) {
  return {a.xlo, a.xhi, a.ylo, a.yhi, a.xlo == a.xhi ? 0 : a.xsep, a.ylo == a.yhi ? 0 : a.ysep};
}

struct Snapshot {
  std::map<std::string, Squares> geometry;  // type -> flattened squares of top
  std::multiset<LabelKey> labels;           // cell, text, rect (and the type by name below)
  std::multiset<std::tuple<std::string, std::string, std::string>> label_types;
  std::multiset<UseKey> uses;
};

Snapshot snapshot(const Session& s) {
  Snapshot out;
  const db::CellDef* top = s.ed.cells.find("top");
  REQUIRE(top != nullptr);
  for (const std::string_view name : kTypes) {
    flatten(s, *top, db::kIdentity, s.type(name), out.geometry[std::string{name}], 0);
  }
  // The cells the write reaches from top.
  std::set<const db::CellDef*> reached;
  std::vector<const db::CellDef*> pending{top};
  while (!pending.empty()) {
    const db::CellDef* def = pending.back();
    pending.pop_back();
    if (reached.insert(def).second) {
      for (const db::CellUse* use : db::uses_in_search_order(*def, s.compat)) {
        pending.push_back(use->def);
      }
    }
  }
  s.ed.cells.for_each([&](db::CellDef& def) {
    if (!reached.contains(&def)) {
      return;
    }
    for (const db::Label& l : def.labels()) {
      out.labels.emplace(def.name(), l.text, numbers(l.rect));
      out.label_types.emplace(def.name(), l.text, std::string{s.ed.technology->type_name(l.type)});
    }
    for (const db::CellUse* use : db::uses_in_search_order(def, s.compat)) {
      out.uses.emplace(def.name(), use->def->name(), numbers(use->transform), numbers(use->array));
    }
  });
  return out;
}

void round_trip(const std::shared_ptr<const tech::Technology>& technology, std::uint32_t seed, bool merge) {
  std::mt19937 rng(seed);
  Session original(technology);
  build_layout(original, rng);
  platform::MemoryFileSystem fs;
  gds::GdsWriteOptions write_options;
  write_options.files = &fs;
  write_options.time_zone = "UTC0";
  write_options.now = kNow;
  write_options.settings.datestamp = 0;
  write_options.settings.merge = merge;
  const cmd::CommandResult written = gds::write(original.ed, *original.ed.cells.find("top"), "/rt.gds", write_options);
  INFO("seed " << seed << (merge ? " merge yes" : " merge no"));
  REQUIRE(written.ok());

  Session copy(technology);
  gds::GdsReadOptions read_options;
  read_options.files = &fs;
  read_options.time_zone = "UTC0";
  const cmd::CommandResult read = gds::read(copy.ed, "/rt.gds", read_options);
  REQUIRE(read.ok());

  const Snapshot before = snapshot(original);
  const Snapshot after = snapshot(copy);
  for (const std::string_view name : kTypes) {
    INFO("type " << name);
    CHECK(after.geometry.at(std::string{name}) == before.geometry.at(std::string{name}));
  }
  const auto text_of = [](const auto& keys) {
    std::ostringstream o;
    for (const auto& k : keys) {
      o << std::get<0>(k) << ":" << std::get<1>(k);
      for (const auto v : std::get<2>(k)) {
        o << "," << v;
      }
      if constexpr (std::tuple_size_v<std::decay_t<decltype(k)>> == 4) {
        for (const auto v : std::get<3>(k)) {
          o << "," << v;
        }
      }
      o << " ";
    }
    return o.str();
  };
  INFO("labels before " << text_of(before.labels) << " after " << text_of(after.labels));
  INFO("uses before " << text_of(before.uses) << " after " << text_of(after.uses));
  CHECK(after.labels == before.labels);
  CHECK(after.label_types == before.label_types);
  CHECK(after.uses == before.uses);
}

}  // namespace

TEST_CASE("write then read keeps geometry, labels and uses with magic-compat OFF",
          "[cap:gds-write][req:Merged polygon output][req:Unmerged boundary output][req:Cell reference elements]") {
  const std::shared_ptr<const tech::Technology> technology = roundtrip_technology();
  std::vector<std::uint32_t> seeds(kRegressionSeeds.begin(), kRegressionSeeds.end());
  seeds.push_back(Catch::getSeed());
  for (const std::uint32_t base : seeds) {
    for (int i = 0; i < kLayoutsPerSeed; ++i) {
      const std::uint32_t seed = base * 7919U + static_cast<std::uint32_t>(i);
      round_trip(technology, seed, false);
      round_trip(technology, seed, true);
    }
  }
}

}  // namespace gds_write_test
