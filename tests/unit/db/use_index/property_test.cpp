// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property test of the security.md M5-S3 fix (m5-mask-gds-cif tasks.md 4.8,
// round 6 review): db::place and db::remove_use no longer mark the use list
// touched, so the index's next read skips the rescan of the list. The rescan
// must never have changed anything after those calls: two parents get the
// same random sequence of placements, removals, child bounding-box changes
// (re-placed or refreshed by db::recompute_bbox), partial searches and grid
// scalings, and one of them has its list marked touched before every read,
// so that index rescans the list each time. Both must give the same
// enumeration orders and search results, in both magic-compat settings.
// Seeded and reproducible.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"

namespace db = hefesto::db;
using hefesto::compat::CompatSettings;

namespace {

const CompatSettings kOn{};
const CompatSettings kOff{false, true};

std::vector<std::string> ids(const std::vector<db::CellUse*>& uses) {
  std::vector<std::string> out;
  out.reserve(uses.size());
  for (const db::CellUse* u : uses) {
    out.push_back(u->id);
  }
  return out;
}

/// One parent under test; `force` marks its use list touched before every
/// read, so its index rescans the list each time (the behaviour before M5-S3).
struct Parent {
  db::CellDef def;
  bool force = false;

  Parent(const char* name, bool forced) : def(name, 1), force(forced) {}

  const db::CellDef& read() {
    if (force) {
      (void)def.uses();
    }
    return def;
  }
  std::vector<std::string> order(const CompatSettings& compat) { return ids(read().use_index().order(compat)); }
  std::vector<std::string> search(const db::Rect& area, const CompatSettings& compat) {
    std::vector<std::string> out;
    (void)db::uses_overlapping(read(), area, compat, [&](db::CellUse& u) {
      out.push_back(u.id);
      return true;
    });
    return out;
  }
};

/// The random operations, applied to both parents alike.
struct Run {
  std::mt19937 rng;
  std::vector<db::CellDef*> children;
  Parent fast{"fast", false};
  Parent forced{"forced", true};
  bool magic_compat = true;

  Run(std::uint32_t seed, bool compat) : rng(seed), magic_compat(compat) {}

  db::Coord coord(db::Coord limit) { return std::uniform_int_distribution<db::Coord>(0, limit)(rng); }

  void place() {
    db::PlaceOptions o;
    o.transform = db::translation(coord(2000), coord(400));
    if (coord(4) == 0) {
      o.array = db::ArrayInfo{0, coord(3), 0, 0, 30, 0};
    }
    db::CellDef* child = children[static_cast<std::size_t>(coord(static_cast<db::Coord>(children.size()) - 1))];
    for (Parent* p : {&fast, &forced}) {
      REQUIRE(db::place(p->def, *child, o).ok());
    }
  }
  void remove() {
    const auto& list = static_cast<const db::CellDef&>(fast.def).uses();
    if (list.empty()) {
      return;
    }
    const auto k = static_cast<std::size_t>(coord(static_cast<db::Coord>(list.size()) - 1));
    for (Parent* p : {&fast, &forced}) {
      REQUIRE(db::remove_use(*static_cast<const db::CellDef&>(p->def).uses()[k]).ok());
    }
  }
  /// A child grows (or gets back its box): DBReComputeBbox re-places or
  /// refreshes its uses in both parents.
  void change_child() {
    db::CellDef& child = *children[static_cast<std::size_t>(coord(static_cast<db::Coord>(children.size()) - 1))];
    db::Label l;
    l.text = "l";
    l.rect = db::Rect{0, 0, 5 + coord(40), 5 + coord(40)};
    child.labels().assign(1, l);
    db::recompute_bbox(child, magic_compat);
  }
  /// DBScaleEverything's in-place scaling of every use (the list taken for
  /// writing, so both indexes rescan it).
  void scale() {
    for (Parent* p : {&fast, &forced}) {
      for (const auto& u : p->def.uses()) {
        u->bbox = db::Rect{u->bbox.x0 * 2, u->bbox.y0 * 2, u->bbox.x1 * 2, u->bbox.y1 * 2};
        u->transform.c *= 2;
        u->transform.f *= 2;
      }
    }
  }
  void step() {
    switch (coord(9)) {
      case 0: case 1: case 2: case 3: place(); break;
      case 4: case 5: remove(); break;
      case 6: change_child(); break;
      case 7: if (coord(9) == 0) scale(); break;
      default: break;  // a read only
    }
  }
  void check() {
    const db::Coord x = coord(2500);
    const db::Coord y = coord(500);
    const db::Rect area{x, y, x + coord(600), y + coord(200)};
    // The searches first: they may bin both indexes, and the orders after
    // them must still agree.
    CHECK(fast.search(area, kOn) == forced.search(area, kOn));
    CHECK(fast.search(area, kOff) == forced.search(area, kOff));
    CHECK(fast.order(kOn) == forced.order(kOn));
    CHECK(fast.order(kOff) == forced.order(kOff));
  }
};

}  // namespace

TEST_CASE("Use index property: placing and removing without marking the list touched changes no order",
          "[cap:hefesto-testing-strategy][req:Geometry property tests]") {
  for (const bool magic_compat : {true, false}) {
    for (std::uint32_t seed = 1; seed <= 24; ++seed) {
      CAPTURE(magic_compat, seed);
      std::vector<std::unique_ptr<db::CellDef>> owned;
      Run run(seed, magic_compat);
      for (const char* name : {"c0", "c1", "c2"}) {
        owned.push_back(std::make_unique<db::CellDef>(name, 1));
        run.children.push_back(owned.back().get());
        db::Label l;
        l.text = "l";
        l.rect = db::Rect{0, 0, 10, 10};
        owned.back()->labels().push_back(l);
        db::recompute_bbox(*owned.back(), magic_compat);
      }
      for (int i = 0; i < 160; ++i) {
        run.step();
        run.check();
      }
      // The children outlive the parents' uses.
      for (Parent* p : {&run.fast, &run.forced}) {
        while (!static_cast<const db::CellDef&>(p->def).uses().empty()) {
          REQUIRE(db::remove_use(*static_cast<const db::CellDef&>(p->def).uses().front()).ok());
        }
      }
    }
  }
}
