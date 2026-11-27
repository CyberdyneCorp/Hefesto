// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Path length extraction (extract/length; openspec/changes/m7-extraction
// tasks.md 9.5): ExtLength.c's flood on hand-built layouts in the EX
// technology (tests/golden/EXTRACT/cases/EX-068/input/exttest.tech), and
// EX-068's captured records. Owner: EXTCMD.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/hier/extract_cell.hpp"
#include "hefesto/extract/length/length.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::test::extlength {
namespace {

namespace extract = hefesto::extract;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;

std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

std::string case_input(const std::string& name) {
  return std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/EXTRACT/cases/EX-068/input/" + name;
}

class LengthCase {
 public:
  LengthCase() {
    platform::MemoryFileSystem fs;
    diag::CollectingSink sink;
    auto loaded = tech::load_technology_text(read_file(case_input("exttest.tech")), "/t/exttest.tech", fs,
                                             tech::LoadOptions{}, sink);
    REQUIRE(loaded.ok());
    tech_ = loaded.value();
  }

  [[nodiscard]] const tech::Technology& tech() const { return *tech_; }

  tech::TileType type(const std::string& name) const {
    const auto found = tech_->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  db::CellDef& cell(const std::string& name) {
    cells_.push_back(std::make_unique<db::CellDef>(name, tech_->plane_count()));
    return *cells_.back();
  }

  void paint(db::CellDef& c, const std::string& layer, db::Rect r) const {
    REQUIRE(db::paint(c, *tech_, r, type(layer)).ok());
  }

  void label(db::CellDef& c, const std::string& layer, db::Point at, const std::string& text) const {
    db::Label l;
    l.text = text;
    l.type = layer.empty() ? tech::kSpace : type(layer);
    l.rect = db::Rect{at.x, at.y, at.x, at.y};
    c.labels().push_back(l);
  }

 private:
  std::shared_ptr<const tech::Technology> tech_;
  std::vector<std::unique_ptr<db::CellDef>> cells_;
};

/// extLength on `root` once its cells are built (the searches prune by the
/// cells' bounding boxes).
extract::PathLengthResult lengths(db::CellDef& root, const tech::Technology& t, const extract::LengthLists& l,
                                  bool compat) {
  db::recompute_bbox(root);
  return extract::compute_path_lengths(root, t, l, compat);
}

extract::LengthLists lists(std::vector<std::string> drivers, std::vector<std::string> receivers) {
  extract::LengthLists l;
  l.drivers = std::move(drivers);
  l.receivers = std::move(receivers);
  return l;
}

/// EX-068's straight wire: metal1 (0,0)-(100,4), d at (0,2), r at (100,2).
db::CellDef& straight_wire(LengthCase& c) {
  db::CellDef& ex = c.cell("ex");
  c.paint(ex, "metal1", {0, 0, 100, 4});
  c.label(ex, "metal1", {0, 2}, "d");
  c.label(ex, "metal1", {100, 2}, "r");
  return ex;
}

TEST_CASE("Path length: a straight wire is 100 long in both modes",
          "[cap:extract-commands][req:Path length extraction]") {
  for (const bool compat : {true, false}) {
    LengthCase c;
    db::CellDef& ex = straight_wire(c);
    const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), compat);
    REQUIRE(result.records.size() == 1);
    CHECK(result.records[0] == extract::PathRecord{"d", "r", {100, 100}});
    CHECK(result.missing.empty());
  }
}

TEST_CASE("Path length: path_lengths writes the distance record and reports a missing driver",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = straight_wire(c);
  extract::ExtractContext ctx;
  ctx.technology = &c.tech();
  diag::CollectingSink sink;
  ctx.sink = &sink;
  extract::ExtWriter writer;
  writer.header(extract::ExtHeader{});
  db::recompute_bbox(ex);
  REQUIRE(extract::path_lengths(ctx, ex, lists({"nosuch", "d"}, {"r"}), writer).ok());
  const auto text = writer.finish();
  REQUIRE(text.ok());
  CHECK(text.value().find("distance d r 100 100\n") != std::string::npos);
  const auto diags = sink.take();
  REQUIRE(diags.size() == 1);
  CHECK(diags[0].code == "EXTRACT-LEN-001");
  CHECK(diags[0].text == "Can't find terminal \"nosuch\"");
}

TEST_CASE("Path length: no driver, no record and no message", "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = straight_wire(c);
  extract::ExtractContext ctx;  // no technology: never read without a driver
  extract::ExtWriter writer;
  writer.header(extract::ExtHeader{});
  REQUIRE(extract::path_lengths(ctx, ex, lists({}, {"r"}), writer).ok());
  const auto text = writer.finish();
  REQUIRE(text.ok());
  CHECK(text.value().find("distance") == std::string::npos);
}

TEST_CASE("Path length: an unreached receiver keeps Magic's INFINITY and MINFINITY",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = c.cell("ex");
  // An L: the flood crosses the lower tile and climbs the upper one; a
  // receiver off the net is never reached.
  c.paint(ex, "metal1", {0, 0, 40, 4});
  c.paint(ex, "metal1", {36, 4, 40, 40});
  c.label(ex, "metal1", {0, 2}, "d");
  c.label(ex, "metal1", {38, 40}, "r");
  const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), true);
  REQUIRE(result.records.size() == 1);
  // (0,2) to the boundary midpoint (38,4): 38 + 2; then up the vertical
  // tile to (38,40): 36.
  CHECK(result.records[0].distance == extract::PathDistance{76, 76});

  const extract::PathLabel far{"x", c.type("metal2"), {500, 500, 500, 500}};
  db::recompute_bbox(ex);
  const auto yanked = extract::yank_path(ex, c.tech(), extract::driver_labels(ex, "d"), lists({"d"}, {"r"}), true);
  const auto d = extract::driver_labels(ex, "d");
  REQUIRE(d.size() == 1);
  CHECK(extract::pair_distance(*yanked.scratch, c.tech(), d[0], far) ==
        extract::PathDistance{db::kInfinity, db::kMinusInfinity});
}

// Q-EXTRACT-42 (compat): extLengthYank lists a receiver label once per
// yanked tile whose area, grown by one at the top and right, it touches.
std::vector<extract::PathRecord> corner_receiver_records(bool magic_compat) {
  LengthCase c;
  db::CellDef& ex = c.cell("ex");
  c.paint(ex, "metal1", {0, 0, 10, 4});
  c.paint(ex, "metal1", {0, 4, 4, 10});
  c.label(ex, "metal1", {0, 2}, "d");
  c.label(ex, "metal1", {4, 4}, "r");  // on the corner of both tiles
  return lengths(ex, c.tech(), lists({"d"}, {"r"}), magic_compat).records;
}

TEST_CASE("Path length: Q-EXTRACT-42: a receiver touching two yanked tiles is listed twice with magic-compat ON",
          "[cap:extract-commands][req:Path length extraction]") {
  const auto records = corner_receiver_records(true);
  REQUIRE(records.size() == 2U);
  CHECK(records[0].receiver == "r");
  CHECK(records[1].receiver == "r");
  CHECK(records[0].distance == records[1].distance);
}

TEST_CASE("Path length: Q-EXTRACT-42: a receiver touching two yanked tiles is listed once with magic-compat OFF",
          "[cap:extract-commands][req:Path length extraction]") {
  const auto records = corner_receiver_records(false);
  REQUIRE(records.size() == 1U);
  CHECK(records[0].receiver == "r");
}

TEST_CASE("Path length: a driver label on space writes no record (Magic prints stale values)",
          "[cap:extract-commands][req:Path length extraction]") {
  for (const bool compat : {true, false}) {
    LengthCase c;
    db::CellDef& ex = straight_wire(c);
    c.label(ex, "", {50, 50}, "d");  // a second, floating label d on space
    const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), compat);
    REQUIRE(result.records.size() == 1);
    CHECK(result.records[0].distance == extract::PathDistance{100, 100});
  }
}

TEST_CASE("Path length: a contact carries the flood to the other plane at the same point",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = c.cell("ex");
  c.paint(ex, "metal1", {0, 0, 20, 4});
  c.paint(ex, "metal2", {16, 0, 60, 4});
  c.paint(ex, "via", {16, 0, 20, 4});
  c.label(ex, "metal1", {0, 2}, "d");
  c.label(ex, "metal2", {60, 2}, "r");
  const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), true);
  REQUIRE(result.records.size() == 1);
  // (0,2) to the via's left edge (16,2): 16. The via image on metal2 is
  // entered at the same point; to its right edge (20,2): 4; then (60,2): 40.
  CHECK(result.records[0].distance == extract::PathDistance{60, 60});
}

TEST_CASE("Path length: distances add in 64 bits (Magic's int sum overflows)",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = c.cell("ex");
  constexpr db::Coord far = 1'000'000'000;
  c.paint(ex, "metal1", {-far, 0, far, 4});
  c.paint(ex, "metal1", {far - 4, 4, far, 96});
  c.paint(ex, "metal1", {-far, 96, far, 100});
  c.label(ex, "metal1", {-far, 2}, "d");
  c.label(ex, "metal1", {-far, 98}, "r");
  const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), true);
  REQUIRE(result.records.size() == 1);
  // 2e9 - 2 + 2 along the bottom, 92 up the right side, 2e9 - 2 + 2 back.
  CHECK(result.records[0].distance == extract::PathDistance{4'000'000'092, 4'000'000'092});
}

TEST_CASE("Path length: a net of 60000 tiles floods without recursion",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& ex = c.cell("ex");
  constexpr db::Coord kSteps = 60000;
  for (db::Coord i = 0; i < kSteps; ++i) {
    c.paint(ex, "metal1", {i, i, i + 2, i + 1});  // a staircase: one tile per row
  }
  // Mid-edge labels: a label on a tile's outer corner starts no net.
  c.label(ex, "metal1", {1, 0}, "d");
  c.label(ex, "metal1", {kSteps, kSteps}, "r");
  // OFF lists r once (ON once per yanked tile it touches: the top two rows).
  const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), false);
  REQUIRE(result.records.size() == 1);
  CHECK(result.records[0].distance.min > 2 * (kSteps - 1));
}

TEST_CASE("Path length: a driver label on a tile's outer corner yanks nothing in both modes",
          "[cap:extract-commands][req:Path length extraction]") {
  // extLengthYank searches the label grown by one; dbcConnectFunc drops a
  // start tile that the 2x2 area meets only at its corner (DBconnect.c
  // 915-927), so the net is empty and no receiver is found.
  for (const bool compat : {true, false}) {
    LengthCase c;
    db::CellDef& ex = c.cell("ex");
    c.paint(ex, "metal1", {0, 0, 100, 4});
    c.label(ex, "metal1", {0, 0}, "d");
    c.label(ex, "metal1", {100, 2}, "r");
    const auto result = lengths(ex, c.tech(), lists({"d"}, {"r"}), compat);
    CHECK(result.records.empty());
    CHECK(result.missing.empty());
  }
}

TEST_CASE("Path length: a hierarchical driver name is found through its instance",
          "[cap:extract-commands][req:Path length extraction]") {
  LengthCase c;
  db::CellDef& leaf = c.cell("leaf");
  c.paint(leaf, "metal1", {0, 0, 10, 4});
  c.label(leaf, "metal1", {0, 2}, "d");
  db::CellDef& top = c.cell("top");
  c.paint(top, "metal1", {10, 0, 30, 4});
  c.label(top, "metal1", {30, 2}, "r");
  db::PlaceOptions o;
  o.id = "c1";
  o.transform = db::Transform{1, 0, 5, 0, 1, 0};
  db::recompute_bbox(leaf);
  REQUIRE(db::place(top, leaf, o).ok());
  const auto labels = extract::driver_labels(top, "c1/d");
  REQUIRE(labels.size() == 1);
  CHECK(labels[0] == extract::PathLabel{"c1/d", c.type("metal1"), {5, 2, 5, 2}});
  CHECK(extract::driver_labels(top, "c2/d").empty());
  const auto result = lengths(top, c.tech(), lists({"c1/d"}, {"r"}), true);
  REQUIRE(result.records.size() == 1);
  CHECK(result.records[0] == extract::PathRecord{"c1/d", "r", {25, 25}});
}

}  // namespace
}  // namespace hefesto::test::extlength
