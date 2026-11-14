// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GDS structures read through gds::read (tasks.md 9.2, 9.5): cell creation,
// timestamps, duplicates, cells already in memory, post-order reading,
// flattening, polygon and path subcells, the end of a structure and of the
// read, in both magic-compat settings where a decision applies (Q-MASK-110,
// 111). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

void square(GdsFile& f, int layer, std::int32_t x, std::int32_t size = 10) {
  f.boundary(layer, 20, {{x, 0}, {x, size}, {x + size, size}, {x + size, 0}, {x, 0}});
}

}  // namespace

TEST_CASE("structures become cells", "[cap:gds-read][req:Structure processing and cell creation]") {
  SECTION("forward reference filled later") {
    Session s;
    GdsFile f;
    f.library().begin("TOP").sref("A", {0, 0}).end();
    f.begin("A");
    square(f, 68, 0);
    f.end().end_library();
    s.read(f);
    const auto u = uses(s.cell("TOP"));
    REQUIRE(u.size() == 1);
    CHECK(u.front()->def == &s.cell("A"));
    CHECK(s.cell("A").flags().available);
    CHECK(s.painted("A", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
    CHECK(s.said("Reading \"TOP\"."));
  }
  SECTION("malformed structure (Q-MASK-110)") {
    for (const bool magic_compat : {true, false}) {
      Session s(kStyle, magic_compat);
      GdsFile f;
      f.library().begin("A");
      square(f, 68, 0);
      f.i2(0x0d, {1});  // a LAYER record outside an element
      f.end().begin("B").end().end_library();
      s.read(f);
      CHECK(s.said("Unexpected record type in input: "));
      CHECK(s.said("    Expected ENDSTR record but got LAYER."));
      CHECK(s.painted("A", "m1").empty());
      if (magic_compat) {
        CHECK(s.painted("B", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
      } else {
        CHECK(s.painted("B", "m1").empty());
        CHECK(s.said("Q-MASK-110"));
      }
    }
  }
}

TEST_CASE("structure timestamps", "[cap:gds-read][req:Structure timestamps]") {
  SECTION("the BGNSTR creation date") {
    Session s;
    GdsFile f;
    f.library().begin("A").end().end_library();
    s.read(f);
    CHECK(s.cell("A").timestamp() == 1705314600);
    CHECK_FALSE(s.cell("A").flags().needs_new_timestamp);
  }
  SECTION("fixed datestamp") {
    Session s;
    s.options.settings.datestamp = 1700000000;
    GdsFile f;
    f.library().begin("A").end().begin("B").end().end_library();
    s.read(f);
    for (const char* name : {"A", "B"}) {
      CHECK(s.cell(name).timestamp() == 1700000000);
      CHECK(s.cell(name).flags().fixed_timestamp);
    }
  }
  SECTION("short BGNSTR") {
    Session s;
    GdsFile f;
    f.library().begin("A", {124, 1, 1, 0, 0, 0}).end().end_library();
    s.read(f);
    CHECK(s.said("Unknown timestamp format;  setting timestamp to zero."));
    CHECK(s.cell("A").timestamp() == 0);
  }
}

TEST_CASE("duplicate structures within one file", "[cap:gds-read][req:Duplicate structures within one file]") {
  Session s;
  GdsFile f;
  f.library().begin("A");
  square(f, 68, 0);
  f.end().begin("A");
  square(f, 68, 0, 20);
  f.end().end_library();
  s.read(f);
  CHECK(s.painted("A", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  CHECK(s.said("Cell \"A\" was already defined in this file."));
  CHECK(s.said("Ignoring duplicate definition"));
}

TEST_CASE("cells already in memory", "[cap:gds-read][req:Cells already in memory before reading]") {
  const auto preload = [](Session& s, std::string_view name) {
    auto def = s.editor.cells.create(name, s.editor.technology->plane_count());
    REQUIRE(def.ok());
    def.value()->flags().available = true;
    REQUIRE(db::paint_type(*def.value(), *s.editor.technology, db::Rect{0, 0, 5, 5}, s.type("m2")).ok());
    return def.value();
  };
  SECTION("overwritten by default") {
    Session s;
    preload(s, "INV");
    GdsFile f;
    f.library().begin("INV");
    square(f, 68, 0);
    f.end().end_library();
    s.read(f);
    CHECK(s.said("Warning:  cell INV already existed before reading GDS!"));
    CHECK(s.painted("INV", "m2").empty());
    CHECK_FALSE(s.painted("INV", "m1").empty());
  }
  SECTION("noduplicates keeps a pre-loaded cell") {
    Session s;
    s.options.settings.noduplicates = true;
    preload(s, "INV");
    GdsFile f;
    f.library().begin("INV");
    square(f, 68, 0);
    f.end().end_library();
    s.read(f);
    CHECK(s.said("Note:  cell INV already existed before reading GDS."));
    CHECK(s.said("Using pre-existing cell definition"));
    CHECK(s.painted("INV", "m2") == std::vector<db::Rect>{{0, 0, 5, 5}});
    CHECK(s.painted("INV", "m1").empty());
  }
  SECTION("noduplicates annotates a LEF abstract") {
    Session s;
    s.options.settings.noduplicates = true;
    db::CellDef* pad = preload(s, "PAD");
    db::set_property(*pad, "LEFview", db::PropertyValue{std::string{"TRUE"}});
    GdsFile f;
    f.library();
    const std::size_t bgnstr = f.size();
    f.begin("PAD");
    const std::size_t start = f.size();
    square(f, 68, 0);
    f.end();
    const std::size_t end = f.size();
    f.end_library();
    s.read(f);
    (void)bgnstr;
    const auto* gds_start = std::get_if<db::DoubleList>(s.cell("PAD").properties().find("GDS_START"));
    const auto* gds_end = std::get_if<db::DoubleList>(s.cell("PAD").properties().find("GDS_END"));
    REQUIRE(gds_start != nullptr);
    REQUIRE(gds_end != nullptr);
    CHECK(gds_start->values == std::vector<std::int64_t>{static_cast<std::int64_t>(start)});
    CHECK(gds_end->values == std::vector<std::int64_t>{static_cast<std::int64_t>(end)});
    CHECK(s.cell("PAD").flags().vendor_gds);
    CHECK(s.painted("PAD", "m2") == std::vector<db::Rect>{{0, 0, 5, 5}});
  }
  SECTION("unique renames the old cell") {
    Session s;
    s.options.settings.unique = true;
    preload(s, "A");
    preload(s, "A_1");
    GdsFile f;
    f.library().begin("A");
    square(f, 68, 0);
    f.end().end_library();
    s.read(f);
    CHECK(s.said("Warning: cell definition \"A\" reused."));
    REQUIRE(s.has_cell("A_2"));
    CHECK(s.painted("A_2", "m2") == std::vector<db::Rect>{{0, 0, 5, 5}});
    CHECK(s.painted("A", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
}

TEST_CASE("post-order reading", "[cap:gds-read][req:Post-order reading and file repositioning]") {
  Session s;
  s.options.settings.ordering = true;
  GdsFile f;
  f.library().begin("TOP").sref("A", {0, 0}).end();
  f.begin("A");
  square(f, 68, 0);
  f.end().end_library();
  s.read(f);
  CHECK_FALSE(s.said("already defined"));
  CHECK(s.count("Reading \"A\".") == 2);  // read ahead, then skipped silently
  CHECK(s.painted("A", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});

  SECTION("a rewind reports when ordering is off") {
    Session t;
    t.options.settings.flatglob = {"A*"};
    GdsFile g;
    g.library().begin("TOP").sref("A9", {0, 0}).end().end_library();
    t.read(g);
    CHECK(t.said("Cell \"A9\" is used but not defined in this file."));
    CHECK(t.said("Rewinding input.  Cells may have been instanced before they were defined.  Consider using \"gds "
                 "ordering on\"."));
    CHECK(t.editor.mask.gds.rewound);
    CHECK(t.cell("A9").flags().dereference);
  }
}

TEST_CASE("duplicate messages after a rewind (Q-MASK-111)", "[cap:gds-read][req:Duplicate structures within one file]") {
  for (const bool magic_compat : {true, false}) {
    Session s(kStyle, magic_compat);
    s.options.settings.flatglob = {"A*"};
    GdsFile rewinding;
    rewinding.library().begin("TOP").sref("A9", {0, 0}).end().end_library();
    s.read(rewinding);
    GdsFile duplicate;
    duplicate.library().begin("D").end().begin("D").end().end_library();
    s.read(duplicate);
    // ON: the rewind of the earlier read silences the messages for the
    // session, as Magic's CalmaRewound; OFF: only a rewind of this read does.
    CHECK(s.said("Ignoring duplicate definition") == !magic_compat);
  }
}

TEST_CASE("flattening small cells", "[cap:gds-read][req:Flattening small cells on input]") {
  SECTION("contact cell flattened into its parent") {
    Session s;
    s.options.settings.flatten = true;
    GdsFile f;
    f.library().begin("VIA");
    square(f, 68, 0, 4);
    square(f, 69, 0, 4);
    f.end().begin("TOP").aref("VIA", 2, 2, {{0, 0}, {40, 0}, {0, 40}}).end().end_library();
    s.read(f);
    CHECK(s.said("Saving contents of cell VIA"));
    CHECK(uses(s.cell("TOP")).empty());
    CHECK(s.area("TOP", "m1") == 4 * 16.0);
    CHECK(s.area("TOP", "m2") == 4 * 16.0);
    CHECK(s.cell("VIA").labels().empty());
  }
  SECTION("default flatglob") {
    Session s;
    GdsFile f;
    f.library().begin("leaf");
    square(f, 68, 0);
    f.end().begin("pcell_CDNS_123").sref("leaf", {100, 0});
    square(f, 68, 0);
    f.end().begin("TOP").sref("pcell_CDNS_123", {0, 0}).end().end_library();
    s.read(f);
    CHECK(s.said("Saving contents of cell pcell_CDNS_123"));
    CHECK(uses(s.cell("TOP")).size() == 1);  // it contains a use, so it is placed too
    CHECK(s.area("TOP", "m1") == 100.0);
  }
  SECTION("shape limit") {
    Session s;
    s.options.settings.flatten = true;
    s.options.settings.flatten_limit = 3;
    GdsFile f;
    f.library().begin("V3");
    square(f, 68, 0);
    square(f, 68, 20);
    square(f, 68, 40);
    f.end().begin("TOP").sref("V3", {0, 0}).end().end_library();
    s.read(f);
    CHECK_FALSE(s.said("Saving contents of cell V3"));
    CHECK(uses(s.cell("TOP")).size() == 1);
  }
}

TEST_CASE("polygon subcells", "[cap:gds-read][req:Polygon subcells]") {
  const auto file = [] {
    GdsFile f;
    f.library().begin("top");
    f.boundary(68, 20, {{0, 0}, {10, 10}, {10, 0}, {0, 0}});
    f.boundary(68, 20, {{20, 0}, {30, 10}, {30, 0}, {20, 0}});
    square(f, 68, 50);
    f.end().end_library();
    return f;
  };
  SECTION("keep") {
    Session s;
    s.options.settings.polygon_subcells = gds::PolygonSubcells::keep;
    s.read(file());
    REQUIRE(s.has_cell("polygon00001"));
    REQUIRE(s.has_cell("polygon00002"));
    CHECK(uses(s.cell("top")).size() == 2);
    CHECK(s.painted("top", "m1") == std::vector<db::Rect>{{50, 0, 60, 10}});
    CHECK(s.area("polygon00001", "m1") == 50.0);
  }
  SECTION("temporary") {
    Session s;
    s.options.settings.polygon_subcells = gds::PolygonSubcells::temporary;
    s.read(file());
    CHECK_FALSE(s.has_cell("polygon00001"));
    CHECK(uses(s.cell("top")).empty());
    CHECK(s.area("top", "m1") == 200.0);
  }
}

TEST_CASE("path subcells", "[cap:gds-read][req:Path subcells]") {
  Session s;
  s.options.settings.path_subcells = true;
  GdsFile f;
  f.library().begin("top");
  for (int i = 0; i < 3; ++i) {
    f.path(68, 20, PathSpec{0, 10, {}, {}}, {{0, i * 50}, {100, i * 50}});
  }
  f.end().end_library();
  s.read(f);
  for (const char* name : {"path00001", "path00002", "path00003"}) {
    REQUIRE(s.has_cell(name));
  }
  CHECK(uses(s.cell("top")).size() == 3);
  CHECK(s.painted("top", "m1").empty());
}

TEST_CASE("end-of-structure processing", "[cap:gds-read][req:End-of-structure processing]") {
  for (const bool drccheck : {true, false}) {
    Session s;
    s.options.settings.drccheck = drccheck;
    GdsFile f;
    f.library().begin("top");
    square(f, 68, 0);
    f.end().end_library();
    s.read(f);
    CHECK(s.editor.drc.pending.contains(s.cell("top")) == drccheck);
    CHECK(s.cell("top").flags().modified);
  }
}

TEST_CASE("end-of-read cleanup", "[cap:gds-read][req:End-of-read cleanup]") {
  Session s;
  GdsFile f;
  f.library().begin("TOP").sref("MISSING", {0, 0}).end().end_library();
  s.read(f);
  CHECK(s.said("Error while reading cell \"TOP\""));
  CHECK(s.said("cell MISSING was used but not defined."));
  CHECK(s.has_cell("MISSING"));
  CHECK_FALSE(s.cell("MISSING").flags().available);
}
