// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TEXT elements, label layers, pin shapes and port labels read through
// gds::read (tasks.md 9.4), in both magic-compat settings where a decision
// applies (Q-MASK-120, 121, 122, 123, 133). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

constexpr std::string_view kStyle10 =
    "style test\nscalefactor 10\nlayer m1 M1\nlabels M1\ncalma M1 68 20\ncalma M1 68 5\n";

GdsFile one_structure(const std::function<void(GdsFile&)>& body, std::string_view name = "top") {
  GdsFile f;
  f.library().begin(name);
  body(f);
  f.end().end_library();
  return f;
}

/// A TEXT built record by record (for record sequences TextSpec cannot make).
void raw_text(GdsFile& f, const std::function<void(GdsFile&)>& middle, P at, std::string_view text) {
  f.empty(0x0c).i2(0x0d, {68}).i2(0x16, {5});
  middle(f);
  f.xy({at}).str(0x19, text).empty(0x11);
}

}  // namespace

TEST_CASE("TEXT presentation, magnification and points", "[cap:gds-read][req:TEXT element parsing]") {
  SECTION("presentation anchor") {
    Session s(kStyle10);
    s.add_fonts(4);
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {1000, 2000}, "VDD", TextSpec{0x0005, {}, false, {}, {}}); }));
    const db::Label* l = label(s.cell("top"), "VDD");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{100, 200, 100, 200});
    CHECK(l->font == 0);
    CHECK(l->justification == db::kCenter);
  }
  SECTION("no presentation: a plain label at southeast") {
    Session s(kStyle10);
    s.add_fonts(4);
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {1000, 2000}, "A", TextSpec{{}, {}, true, 2.0, {}}); }));
    const db::Label* l = label(s.cell("top"), "A");
    REQUIRE(l != nullptr);
    CHECK(l->font == -1);
    CHECK(l->justification == db::kSouthEast);
  }
  SECTION("MAG sets the size with magscale") {
    Session s;
    s.add_fonts(1);
    s.options.settings.magscale = 2.0;
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {10, 10}, "M", TextSpec{0x0000, {}, true, 0.5, {}}); }));
    const db::Label* l = label(s.cell("top"), "M");
    REQUIRE(l != nullptr);
    CHECK(l->size == 1000);
  }
  SECTION("invalid magnification keeps the default size") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {10, 10}, "M", TextSpec{0x0000, {}, true, 20000.0, {}}); }));
    CHECK(s.said("Invalid text magnification 20000."));
  }
  SECTION("empty string ignored") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {3, 4}, ""); }));
    CHECK(s.said("Warning:  Ignoring empty string label at (3, 4)"));
    CHECK(s.cell("top").labels().empty());
  }
  SECTION("negative off-grid label point") {
    for (const bool magic_compat : {true, false}) {
      Session s(kStyle10, magic_compat);
      s.read(one_structure([](GdsFile& f) { f.text(68, 5, {-15, 25}, "N"); }));
      const db::Label* l = label(s.cell("top"), "N");
      REQUIRE(l != nullptr);
      CHECK(l->rect == (magic_compat ? db::Rect{-1, 2, -1, 2} : db::Rect{-2, 2, -2, 2}));
    }
  }
  SECTION("WIDTH after PATHTYPE (Q-MASK-120)") {
    for (const bool magic_compat : {true, false}) {
      Session s(kStyle, magic_compat);
      s.add_fonts(1);
      s.read(one_structure([](GdsFile& f) {
        raw_text(f, [](GdsFile& t) { t.i2(0x17, {0}).i2(0x21, {0}).i4(0x0f, {30}); }, {10, 10}, "W");
      }));
      const db::Label* l = label(s.cell("top"), "W");
      REQUIRE(l != nullptr);
      if (magic_compat) {
        CHECK(l->size == 800);  // the WIDTH was skipped: the 1 um default (800 * M / S)
      } else {
        CHECK(l->size == 30);
      }
    }
  }
}

TEST_CASE("label layers and kinds", "[cap:gds-read][req:Label layer assignment]") {
  SECTION("port layer labels get increasing indices") {
    Session s;
    s.read(one_structure([](GdsFile& f) {
      f.text(68, 5, {5, 5}, "A");
      f.text(68, 5, {15, 5}, "B");
    }));
    const db::Label* a = label(s.cell("top"), "A");
    const db::Label* b = label(s.cell("top"), "B");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->port_index == 1);
    CHECK(b->port_index == 2);
    CHECK(a->sticky);
    CHECK(a->port_directions == (db::kPortNorth | db::kPortSouth | db::kPortEast | db::kPortWest));
  }
  SECTION("cell-id label renames the cell") {
    Session s("style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\nlayer m2 CID\nlabels CID cell\ncalma CID 63 0\n");
    s.read(one_structure([](GdsFile& f) { f.text(63, 0, {0, 0}, "real_name"); }, "XYZ"));
    CHECK(s.has_cell("real_name"));
    CHECK_FALSE(s.has_cell("XYZ"));
  }
  SECTION("unmapped pair") {
    Session s;
    s.read(one_structure([](GdsFile& f) { f.text(99, 0, {0, 0}, "U"); }));
    CHECK(s.said("Label on unknown layer/datatype, layer=99 type=0"));
    const db::Label* l = label(s.cell("top"), "U");
    REQUIRE(l != nullptr);
    CHECK(l->type == tech::kSpace);
    CHECK_FALSE(l->sticky);
  }
  SECTION("unmapped pair with ignore-unknown-layer-labels") {
    Session s("style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\noptions ignore-unknown-layer-labels\n");
    s.read(one_structure([](GdsFile& f) { f.text(99, 0, {0, 0}, "U"); }));
    CHECK_FALSE(s.said("Label on unknown layer/datatype"));
    CHECK(label(s.cell("top"), "U") == nullptr);
  }
}

TEST_CASE("pin shapes attach labels and ports", "[cap:gds-read][req:Pin shapes and port labels]") {
  const auto pin = [](GdsFile& f) { f.boundary(68, 16, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}}); };
  const auto text = [](GdsFile& f) { f.text(68, 5, {5, 5}, "IN"); };
  SECTION("pin shape after text") {
    Session s;
    s.read(one_structure([&](GdsFile& f) {
      text(f);
      pin(f);
    }));
    const db::Label* l = label(s.cell("top"), "IN");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{0, 0, 10, 10});
    CHECK(l->port_index == 1);
  }
  SECTION("pin shape before text") {
    Session s;
    s.read(one_structure([&](GdsFile& f) {
      pin(f);
      text(f);
    }));
    REQUIRE(s.cell("top").labels().size() == 1);
    const db::Label* l = label(s.cell("top"), "IN");
    REQUIRE(l != nullptr);
    CHECK(l->rect == db::Rect{0, 0, 10, 10});
    CHECK(l->port_index == 1);
  }
  SECTION("text-kind pin shape (Q-MASK-123)") {
    for (const bool magic_compat : {true, false}) {
      Session s("style test\nscalefactor 1\nlayer m1 M1T\nlabels M1T text\ncalma M1T 68 16\ncalma M1T 68 5\n",
                magic_compat);
      s.read(one_structure([&](GdsFile& f) {
        pin(f);
        text(f);
      }));
      const db::Label* l = label(s.cell("top"), "IN");
      REQUIRE(l != nullptr);
      CHECK(l->rect == db::Rect{0, 0, 10, 10});
      CHECK(l->is_port() == magic_compat);
      if (magic_compat) {
        // calmaElementText numbers the consumed placeholder's port over the
        // remaining labels: none here, so index 0 (the scenario's 1 holds
        // for a port-kind layer, which renumbers after placing).
        CHECK(l->port_index == 0);
      }
    }
  }
  SECTION("pin with no text") {
    Session s;
    s.read(one_structure([&](GdsFile& f) { pin(f); }));
    CHECK(s.cell("top").labels().empty());
  }
  SECTION("same-named ports share an index") {
    Session s;
    s.read(one_structure([](GdsFile& f) {
      f.text(68, 5, {1, 1}, "A");
      f.text(68, 5, {2, 2}, "B");
      f.text(68, 5, {3, 3}, "A");
    }));
    std::vector<std::uint32_t> indices;
    for (const db::Label& l : s.cell("top").labels()) {
      indices.push_back(l.port_index);
    }
    CHECK(indices == std::vector<std::uint32_t>{1, 2, 1});
  }
}

TEST_CASE("labels are adjusted to the material under them", "[cap:gds-read][req:End-of-structure processing]") {
  Session s("style test\nscalefactor 1\nlayer m1 M1\ncalma M1 68 20\nlayer m2 M2\nlabels M2\ncalma M2 69 5\n");
  s.read(one_structure([](GdsFile& f) {
    f.boundary(68, 20, {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}});
    f.text(99, 0, {5, 5}, "onspace");
    f.text(69, 5, {50, 50}, "floating");
  }));
  const db::Label* moved = label(s.cell("top"), "onspace");
  REQUIRE(moved != nullptr);
  CHECK(moved->type == s.type("m1"));  // a space label moves onto the metal under it
  CHECK(s.said("Moving label \"onspace\" from space to metal1 in cell top."));
  const db::Label* floating = label(s.cell("top"), "floating");
  REQUIRE(floating != nullptr);
  CHECK(floating->type == s.type("m2"));
  CHECK(floating->sticky);  // no metal2 under it: made sticky (DBAdjustLabelsNew)
  CHECK(s.said("Making label \"floating\" on type metal2 in cell top sticky."));
}

TEST_CASE("the magscale setting reinterprets text MAG", "[cap:gds-read][req:Text magnification scale option]") {
  // The read side of `gds magscale`: the factor applies to MAG records (the
  // option's syntax and `default` are the gds command's, GDSW).
  for (const double magscale : {1.0, 0.1}) {
    Session s;
    s.add_fonts(1);
    s.options.settings.magscale = magscale;
    s.read(one_structure([](GdsFile& f) { f.text(68, 5, {10, 10}, "M", TextSpec{0x0000, {}, true, 1.0, {}}); }));
    const db::Label* l = label(s.cell("top"), "M");
    REQUIRE(l != nullptr);
    CHECK(l->size == (magscale == 1.0 ? 1000 : 100));
  }
}

TEST_CASE("GDS fonts that are not loaded read as the default font", "[cap:font-labels][req:GDS representation of font attributes]") {
  // The read side of the font attributes: PRESENTATION bits 4-5 name the
  // font; an index without a loaded font makes a default-font label (the
  // written records are gds-write's "Label TEXT records", GDSW).
  Session s;
  s.add_fonts(2);
  s.read(one_structure([](GdsFile& f) {
    f.text(68, 5, {1, 1}, "f1", TextSpec{0x0015, {}, false, {}, {}});
    f.text(68, 5, {2, 2}, "f3", TextSpec{0x0035, {}, false, {}, {}});
  }));
  const db::Label* f1 = label(s.cell("top"), "f1");
  const db::Label* f3 = label(s.cell("top"), "f3");
  REQUIRE(f1 != nullptr);
  REQUIRE(f3 != nullptr);
  CHECK(f1->font == 1);
  CHECK(f1->size == 800);
  CHECK(f3->font == -1);
  CHECK(f3->size == 0);
  CHECK(f3->justification == db::kCenter);
}
