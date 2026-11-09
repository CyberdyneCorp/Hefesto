// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Labels and ports (layout-labels-ports) and typed properties
// (layout-properties).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;

namespace {

db::Label make_label(const char* text, Rect r, tech::TileType type = tech::kSpace) {
  db::Label l;
  l.text = text;
  l.rect = r;
  l.type = type;
  return l;
}

db::Label port(const char* text) {
  db::Label l = make_label(text, {0, 0, 0, 0});
  l.port_directions = db::kPortNorth;
  return l;
}

}  // namespace

#define LP "[cap:layout-labels-ports]"
#define PR "[cap:layout-properties]"

TEST_CASE("labels are appended at the end", LP "[req:Label data model]") {
  db::CellDef d("a", 8);
  for (const char* t : {"a", "b", "c"}) {
    REQUIRE(db::put_label(d, make_label(t, {0, 0, 0, 0})).ok());
  }
  CHECK(d.labels().back().text == "c");
  CHECK(d.flags().modified);
  const auto empty = db::put_label(d, make_label("", {0, 0, 0, 0}));
  REQUIRE_FALSE(empty.ok());
  CHECK(empty.error().code == db::codes::kLabelEmpty);
  db::Label plain = make_label("f", {0, 0, 0, 0});
  plain.size = 8;
  plain.rotation = 90;
  REQUIRE(db::put_label(d, plain).ok());
  CHECK(d.labels().back().size == 0);  // default font: no size or rotation
  CHECK(d.labels().back().rotation == 0);
}

TEST_CASE("automatic justification", LP "[req:Automatic justification]") {
  const Rect cell{0, 0, 30, 30};
  CHECK(db::auto_justification(cell, {2, 2, 2, 2}) == db::kNorthEast);
  CHECK(db::auto_justification(cell, {15, 15, 15, 15}) == db::kNorth);
  CHECK(db::auto_justification(cell, {28, 28, 28, 28}) == db::kSouthWest);
  CHECK(db::auto_justification(cell, {2, 28, 2, 28}) == db::kSouthEast);
  CHECK(db::auto_justification(cell, {15, 2, 15, 2}) == db::kNorth);
  CHECK(db::auto_justification(cell, {28, 15, 28, 15}) == db::kWest);
  db::CellDef d("a", 8);
  d.set_bboxes(cell, cell);
  auto l = db::put_label(d, make_label("x", {2, 2, 2, 2}), true);
  REQUIRE(l.ok());
  CHECK(l.value()->justification == db::kNorthEast);
}

TEST_CASE("label migrates onto a contact", LP "[req:Choosing a label's layer]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = lib.create("a", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 6, 6}, mt.m1).ok());
  REQUIRE(db::put_label(*d, make_label("A", {2, 2, 4, 4}, mt.m1)).ok());
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 6, 6}, mt.via1).ok());
  CHECK(db::adjust_labels(*d, *mt.tech, {0, 0, 6, 6}));
  CHECK(d->labels()[0].type == mt.via1);
}

TEST_CASE("a label without a layer picks the covering material", LP "[req:Choosing a label's layer]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = lib.create("a", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 10, 10}, mt.m2).ok());
  CHECK(db::choose_label_layer(*d, *mt.tech, make_label("p", {5, 5, 5, 5})) == mt.m2);
  CHECK(db::choose_label_layer(*d, *mt.tech, make_label("r", {2, 2, 4, 4})) == mt.m2);
  CHECK(db::choose_label_layer(*d, *mt.tech, make_label("o", {20, 20, 22, 22})) == tech::kSpace);
}

TEST_CASE("label shrinks onto remaining metal, falls to space, sticky stays",
          LP "[req:Label adjustment after paint changes]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = lib.create("a", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(db::put_label(*d, make_label("A", {2, 2, 8, 8}, mt.m1)).ok());
  REQUIRE(db::put_label(*d, make_label("P", {5, 5, 5, 5}, mt.m1)).ok());
  db::Label sticky = make_label("S", {2, 2, 8, 8}, mt.m1);
  sticky.sticky = true;
  REQUIRE(db::put_label(*d, sticky).ok());
  REQUIRE(db::erase(*d, *mt.tech, {0, 0, 5, 10}, tech::TypeMask::of(mt.m1)).ok());
  std::vector<std::string> messages;
  CHECK(db::adjust_labels(*d, *mt.tech, {0, 0, 5, 10}, &messages));
  CHECK(d->labels()[0].type == mt.m1);
  CHECK(d->labels()[0].rect == Rect{5, 2, 8, 8});
  REQUIRE_FALSE(messages.empty());
  CHECK(messages[0] == "Adjusting size of label \"A\" in cell a.");
  REQUIRE(db::erase(*d, *mt.tech, {0, 0, 10, 10}, tech::TypeMask::of(mt.m1)).ok());
  CHECK(db::adjust_labels(*d, *mt.tech, {0, 0, 10, 10}));
  CHECK(d->labels()[1].type == tech::kSpace);
  CHECK(d->labels()[2].type == mt.m1);
  CHECK(d->labels()[2].rect == Rect{2, 2, 8, 8});
}

TEST_CASE("a label survives a partial erase", LP "[req:Erasing labels together with paint]") {
  HEFESTO_REQUIRE_MT(mt);
  db::CellLibrary lib;
  auto* d = lib.create("a", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*d, *mt.tech, {0, 0, 20, 10}, mt.m1).ok());
  REQUIRE(db::put_label(*d, make_label("A", {10, 5, 10, 5}, mt.m1)).ok());
  REQUIRE(db::erase(*d, *mt.tech, {0, 0, 10, 10}, tech::TypeMask::of(mt.m1)).ok());
  d->flags() = db::CellFlags{};
  CHECK(db::erase_labels(*d, *mt.tech, {0, 0, 10, 10}, tech::TypeMask::of(mt.m1)) == 0);
  CHECK(d->labels().size() == 1);
  CHECK_FALSE(d->flags().modified);
  tech::TypeMask with_labels = tech::TypeMask::of(mt.m1);
  with_labels.set(db::kLabelsPseudoType);
  CHECK(db::erase_labels(*d, *mt.tech, {0, 0, 10, 10}, with_labels, "B*") == 0);
  CHECK(db::erase_labels(*d, *mt.tech, {0, 0, 10, 10}, with_labels, "A") == 1);
  CHECK(d->flags().modified);
}

TEST_CASE("glob matching follows Magic's Match", LP "[req:Erasing labels together with paint]") {
  CHECK(db::glob_match("a*c", "abbbc"));
  CHECK(db::glob_match("a?c", "abc"));
  CHECK(db::glob_match("[a-c]x", "bx"));
  CHECK_FALSE(db::glob_match("[a-c]x", "dx"));
  CHECK(db::glob_match("\\*", "*"));
  CHECK_FALSE(db::glob_match("a", "ab"));
}

TEST_CASE("stacked label type maps to the larger component", LP "[req:Labels on contacts and stacked contacts]") {
  HEFESTO_REQUIRE_MT(mt);
  tech::TileType stacked = tech::kSpace;
  for (const auto& c : mt.tech->contacts()) {
    if (c.stacked) {
      stacked = c.type;
    }
  }
  REQUIRE(stacked != tech::kSpace);
  CHECK(db::label_layer_for(*mt.tech, stacked) == std::max(mt.via1, mt.via2));
  CHECK(db::label_layer_for(*mt.tech, mt.m1) == mt.m1);
}

TEST_CASE("natural ordering of port indices", LP "[req:Port renumbering]") {
  db::CellDef d("a", 8);
  for (const char* t : {"b", "A10", "A2"}) {
    REQUIRE(db::put_label(d, port(t)).ok());
  }
  REQUIRE(db::put_label(d, make_label("c", {0, 0, 0, 0})).ok());
  db::renumber_ports(d);
  CHECK(d.labels()[2].port_index == 0);  // A2
  CHECK(d.labels()[1].port_index == 1);  // A10
  CHECK(d.labels()[0].port_index == 2);  // b
}

TEST_CASE("duplicate port text shares an index", LP "[req:Port renumbering]") {
  db::CellDef d("a", 8);
  for (const char* t : {"VDD", "GND", "VDD", "vdd"}) {
    REQUIRE(db::put_label(d, port(t)).ok());
  }
  db::renumber_ports(d, true);
  CHECK(d.labels()[1].port_index == 0);
  CHECK(d.labels()[0].port_index == 1);
  CHECK(d.labels()[2].port_index == 1);
  CHECK(d.labels()[3].port_index == 2);  // case-sensitive sharing (Q-MODEL-10)
  db::renumber_ports(d, false);
  CHECK(d.labels()[3].port_index == 1);
}

TEST_CASE("the natural comparator is a total order", LP "[req:Port renumbering]") {
  CHECK(db::natural_compare("a2", "a10") < 0);
  CHECK(db::natural_compare("a10", "B1") < 0);
  CHECK(db::natural_compare("x", "x2") < 0);
  CHECK(db::natural_compare("x2", "x") > 0);
  CHECK(db::natural_compare("VDD", "vdd") < 0);  // byte tie-break
  CHECK(db::natural_compare("a01", "a1") < 0);
  CHECK(db::natural_compare("same", "same") == 0);
}

TEST_CASE("a port makes a subcircuit", LP "[req:Subcircuit determination]") {
  db::CellDef d("a", 8);
  REQUIRE(db::put_label(d, make_label("plain", {0, 0, 0, 0})).ok());
  CHECK_FALSE(db::is_subcircuit(d));
  REQUIRE(db::put_label(d, port("p")).ok());
  CHECK(db::is_subcircuit(d));
}

TEST_CASE("a label in a subcell is found with its path", LP "[req:Hierarchical label search]") {
  db::CellLibrary lib;
  auto* top = lib.create("top", 8).value();
  auto* child = lib.create("child", 8).value();
  REQUIRE(db::put_label(*child, make_label("out", {1, 1, 1, 1})).ok());
  db::PlaceOptions o;
  o.id = "u_0";
  REQUIRE(db::place(*top, *child, o).ok());
  std::vector<std::string> found;
  db::search_labels(*top, {0, 0, 2, 2}, [&](const db::LabelHit& h) {
    found.push_back(h.path + h.label->text);
    return true;
  });
  CHECK(found == std::vector<std::string>{"u_0/out"});
}

TEST_CASE("replacing a property changes its type", PR "[req:Typed property store]") {
  db::CellDef d("a", 8);
  db::set_property(d, "note", db::PropertyValue{std::string{"x"}});
  db::set_property(d, "note", db::PropertyValue{db::IntegerList{{5}}});
  const auto* v = d.properties().find("note");
  REQUIRE(v != nullptr);
  CHECK(*v == db::PropertyValue{db::IntegerList{{5}}});
  db::set_property(d, "note", std::nullopt);
  CHECK(d.properties().find("note") == nullptr);
}

TEST_CASE("a read-only cell ignores properties", PR "[req:Typed property store]") {
  db::CellDef d("ro", 8);
  d.flags().no_edit = true;
  db::set_property(d, "foo", db::PropertyValue{std::string{"bar"}});
  CHECK(d.properties().find("foo") == nullptr);
}

TEST_CASE("clearing properties clears the fixed-bbox flag", PR "[req:Typed property store]") {
  db::CellDef d("a", 8);
  db::set_property(d, "FIXED_BBOX", db::coerce_reserved_property("FIXED_BBOX", std::string{"0 0 100 200"}));
  CHECK(d.flags().fixed_bbox);
  db::clear_properties(d);
  CHECK_FALSE(d.flags().fixed_bbox);
  CHECK(d.properties().size() == 0);
}

TEST_CASE("properties iterate in Magic's hash order", PR "[req:Typed property store]") {
  db::PropertyStore store;
  hefesto::util::MagicHashTable<int> reference(8);
  for (const char* n : {"FIXED_BBOX", "LEFclass", "LEFsite", "a", "b", "c", "path_1", "path_2", "device"}) {
    store.put(n, std::string{"v"});
    reference.insert(n, 0);
  }
  std::vector<std::string> order;
  store.for_each([&](std::string_view n, const db::PropertyValue&) { order.emplace_back(n); });
  CHECK(order == reference.keys());
}

TEST_CASE("reserved names are coerced to their types", PR "[req:Reserved-name type coercion]") {
  using db::coerce_reserved_property;
  CHECK(coerce_reserved_property("FIXED_BBOX", std::string{"0 0 100 200"}) ==
        db::PropertyValue{db::DimensionList{{0, 0, 100, 200}}});
  CHECK(coerce_reserved_property("FIXED_BBOX", std::string{"0 0 100"}) ==
        db::PropertyValue{std::string{"0 0 100"}});  // fallback to string
  CHECK(coerce_reserved_property("GDS_START", std::string{"1234"}) == db::PropertyValue{db::DoubleList{{1234}}});
  CHECK(coerce_reserved_property("GDS_FILE", db::PropertyValue{db::IntegerList{{7}}}) ==
        db::PropertyValue{std::string{"7"}});
  CHECK(coerce_reserved_property("MASKHINTS_X", std::string{"0 0 4 4"}) ==
        db::PropertyValue{db::PlaneValue{{Rect{0, 0, 4, 4}}}});
  CHECK(coerce_reserved_property("other", std::string{"1 2"}) == db::PropertyValue{std::string{"1 2"}});
}

TEST_CASE("GDS_FILE toggles vendor status", PR "[req:Reserved property names and side effects]") {
  db::CellDef pad("pad", 8);
  db::set_property(pad, "GDS_FILE", db::PropertyValue{std::string{"/lib/pads.gds"}});
  CHECK(pad.flags().vendor_gds);
  db::set_property(pad, "GDS_FILE", std::nullopt);
  CHECK_FALSE(pad.flags().vendor_gds);
}

TEST_CASE("typed property values parse and print", PR "[req:Typed property store]") {
  auto v = db::parse_property_value("integer", "7 -3");
  REQUIRE(v.ok());
  CHECK(db::property_text(v.value()) == "7 -3");
  CHECK(db::parse_property_value("double", "12345678901").ok());
  const auto bad = db::parse_property_value("integer", "x");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == db::codes::kPropertyValue);
}

TEST_CASE("scaling FIXED_BBOX leaves integer properties alone", PR "[req:Geometric properties follow the geometry]") {
  const db::PropertyValue bbox{db::DimensionList{{0, 0, 100, 200}}};
  CHECK(db::scale_property(bbox, 2, 1) == db::PropertyValue{db::DimensionList{{0, 0, 200, 400}}});
  const db::PropertyValue n{db::IntegerList{{7}}};
  CHECK(db::scale_property(n, 2, 1) == n);
  const db::PropertyValue odd{db::DimensionList{{1, 2, 3}}};
  CHECK(db::translate_property(odd, 10, 20) == db::PropertyValue{db::DimensionList{{11, 22, 3}}});
  const db::PropertyValue plane{db::PlaneValue{{Rect{0, 0, 4, 4}}}};
  CHECK(db::scale_property(plane, 3, 1) == db::PropertyValue{db::PlaneValue{{Rect{0, 0, 12, 12}}}});
}
