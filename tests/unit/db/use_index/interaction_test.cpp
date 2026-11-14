// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine "Interaction area" with both magic-compat settings
// (Q-MASK-400), and gds-write "Structure ordering": the interaction
// finder's use searches go through the cell's use index, so a DRC check
// bins a cell as Magic's does. Uses the DRC engine's test technology
// (tests/data/drc/engine/engine.tech).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;

namespace {

std::shared_ptr<const tech::Technology> engine_tech() {
  static const std::shared_ptr<const tech::Technology> loaded = [] {
    const std::filesystem::path path = std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/data/drc/engine/engine.tech";
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    hefesto::platform::MemoryFileSystem fs;
    hefesto::diag::CollectingSink sink;
    auto t = tech::load_technology_text(text.str(), "/t/engine.tech", fs, tech::LoadOptions{}, sink);
    REQUIRE(t.ok());
    return t.value();
  }();
  return loaded;
}

struct Session {
  hefesto::editor::EditorState ed;
  std::shared_ptr<const tech::Technology> tech = engine_tech();
  hefesto::compat::CompatSettings compat;

  explicit Session(bool magic_compat) {
    compat.magic_compat = magic_compat;
    ed.technology = tech;
    ed.drc.compat = &compat;
    drc::RuleSetBuilder b(tech->type_count());
    b.halo(drc::Halo{5, 5, 0});
    b.step_size(1000);
    ed.drc.rules = b.build();
  }

  db::CellDef& cell(const std::string& name) {
    auto made = ed.cells.create(name, tech->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  void paint(db::CellDef& def, const db::Rect& r) {
    const auto poly = tech->find_type("poly");
    REQUIRE(poly.ok());
    REQUIRE(db::paint_type(def, *tech, r, *poly.id).ok());
    db::recompute_bbox(def);
  }

  void place(db::CellDef& parent, db::CellDef& child, const db::Transform& t) {
    db::PlaceOptions o;
    o.transform = t;
    REQUIRE(db::place(parent, child, o).ok());
    db::recompute_bbox(parent);
  }

  std::optional<db::Rect> interaction(const db::CellDef& def, const db::Rect& area) {
    const auto found = drc::interaction_area(ed, def, area, 5);
    REQUIRE(found.ok());
    return found.value();
  }
};

}  // namespace

TEST_CASE("Subcell far from paint, in both magic-compat settings", "[cap:drc-engine][req:Interaction area]") {
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << magic_compat);
    Session s(magic_compat);
    db::CellDef& child = s.cell("child");
    db::CellDef& parent = s.cell("parent");
    s.paint(child, db::Rect{0, 0, 10, 10});
    s.place(parent, child, db::kIdentity);
    s.paint(parent, db::Rect{100, 0, 110, 10});
    // ON: I = [-5,-5,5,5] from the (0,0,0,0) seed, with one source only;
    // OFF: I is empty. Both: none.
    CHECK_FALSE(s.interaction(parent, db::Rect{-50, -50, 200, 50}).has_value());
  }
}

TEST_CASE("A use without siblings adds the phantom origin area only with magic-compat ON",
          "[cap:drc-engine][req:Interaction area]") {
  const auto area_with = [](bool magic_compat) {
    Session s(magic_compat);
    db::CellDef& child = s.cell("child");
    db::CellDef& parent = s.cell("parent");
    s.paint(child, db::Rect{0, 0, 10, 10});
    s.place(parent, child, db::kIdentity);
    s.paint(parent, db::Rect{12, 0, 20, 10});
    return s.interaction(parent, db::Rect{-50, -50, 200, 50});
  };
  // (P + h) clipped to the use's halo area is [7,-5,15,15]; ON joins
  // [-5,-5,5,5], the null rectangle expanded by the halo (Q-MASK-400).
  CHECK(area_with(true) == db::Rect{-5, -5, 15, 15});
  CHECK(area_with(false) == db::Rect{7, -5, 15, 15});
}

TEST_CASE("A use with a sibling nearby is the same in both settings", "[cap:drc-engine][req:Interaction area]") {
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << magic_compat);
    Session s(magic_compat);
    db::CellDef& child = s.cell("child");
    db::CellDef& parent = s.cell("parent");
    s.paint(child, db::Rect{0, 0, 10, 10});
    s.place(parent, child, db::translation(100, 0));
    s.place(parent, child, db::translation(111, 0));
    CHECK(s.interaction(parent, db::Rect{0, -50, 300, 50}) == db::Rect{106, -5, 115, 15});
  }
}

TEST_CASE("The interaction finder's searches bin the use index", "[cap:gds-write][req:Structure ordering]") {
  Session s(true);
  db::CellDef& child = s.cell("child");
  db::CellDef& parent = s.cell("parent");
  s.paint(child, db::Rect{0, 0, 10, 10});
  for (int i = 0; i < 12; ++i) {
    s.place(parent, child, db::translation(20 * i, 0));
  }
  REQUIRE_FALSE(parent.use_index().state_for_test().binned);
  (void)s.interaction(parent, db::Rect{0, 0, 30, 30});  // a partial search over 12 uses
  CHECK(parent.use_index().state_for_test().binned);
}
