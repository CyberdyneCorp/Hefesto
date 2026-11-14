// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Structure ordering": the cell-use index (db::UseIndex), the
// bplane port of Magic 8.3.694 (bplane/bpMain.c, bpBins.c, bpEnum.h,
// bpEnum.c). The cases follow bpBins.c and bpEnum.h: the rebuild threshold,
// the shrink of BPE_OVERLAP at the bounding-box edge, BPE_EQUAL and the
// whole-plane walk never rebuilding, the doubled box of a second rebuild,
// oversized lists, on-the-fly sub-binning and the magic-compat OFF order.

#include <catch2/catch_test_macros.hpp>

#include <memory>
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

/// Free-standing uses with given boxes (identity, no parent) and an index.
struct Uses {
  std::vector<std::unique_ptr<db::CellUse>> all;
  db::UseIndex index;

  db::CellUse& add(const db::Rect& r, bool place = true) {
    auto u = std::make_unique<db::CellUse>();
    u->id = "u" + std::to_string(all.size());
    u->bbox = r;
    u->serial = all.size() + 1;
    db::CellUse& ref = *u;
    all.push_back(std::move(u));
    if (place) {
      index.place(ref);
    }
    return ref;
  }
  [[nodiscard]] db::CellUse* operator[](std::size_t i) const { return all[i].get(); }
};

/// The 10 uses of MASK-UO-003 (ca 4x4, cb 6x3), placed in that order.
void add_ten(Uses& u) {
  const db::Rect boxes[] = {{0, 0, 4, 4},     {20, 20, 26, 23}, {40, 40, 44, 44}, {60, 60, 66, 63},
                            {80, 80, 84, 84}, {100, 100, 106, 103}, {0, 20, 4, 24}, {20, 40, 26, 43},
                            {40, 60, 44, 64}, {60, 80, 66, 83}};
  for (const db::Rect& r : boxes) {
    u.add(r);
  }
}

std::vector<db::CellUse*> searched(const db::UseIndex& index, const db::Rect& area) {
  std::vector<db::CellUse*> out;
  (void)index.search(area, [&](db::CellUse& use) {
    out.push_back(&use);
    return true;
  });
  return out;
}

std::vector<std::string> ids(const std::vector<db::CellUse*>& uses) {
  std::vector<std::string> out;
  for (const db::CellUse* use : uses) {
    out.push_back(use->id);
  }
  return out;
}

std::vector<db::CellUse*> reversed(const Uses& u) {
  std::vector<db::CellUse*> out;
  for (auto it = u.all.rbegin(); it != u.all.rend(); ++it) {
    out.push_back(it->get());
  }
  return out;
}

}  // namespace

TEST_CASE("Use index: insertion is LIFO until a search bins it", "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  CHECK(u.index.order(kOn) == reversed(u));
  CHECK_FALSE(u.index.state_for_test().binned);
  CHECK(u.index.state_for_test().unbinned.size() == 10);
  CHECK(u.index.state_for_test().bbox == db::Rect{0, 0, 106, 103});
}

TEST_CASE("Use index: the rebuild threshold is 10 unbinned uses", "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  u.index.remove(*u[9]);
  // 9 unbinned uses: a partial search finds without binning (bpBins.c:651).
  CHECK(searched(u.index, db::Rect{0, 0, 30, 30}) == std::vector<db::CellUse*>{u[6], u[1], u[0]});
  CHECK_FALSE(u.index.state_for_test().binned);
  u.index.place(*u[9]);
  // 10: the bins are built over [0,0,106,103] with the sizing of
  // bpBinArraySizeIt: 33 x 33 bins, row-major from the bottom left.
  CHECK(searched(u.index, db::Rect{0, 0, 30, 30}) == std::vector<db::CellUse*>{u[0], u[1], u[6]});
  const db::UseIndexState st = u.index.state_for_test();
  CHECK(st.binned);
  CHECK(st.unbinned.empty());
  CHECK(st.bin_area == db::Rect{0, 0, 106, 103});
  // MASK-UO-003 step 16: ca_0 cb_0 ca_3 cb_3 ca_1 cb_1 ca_4 cb_4 ca_2 cb_2.
  CHECK(u.index.order(kOn) == std::vector<db::CellUse*>{u[0], u[1], u[6], u[7], u[2], u[3], u[8], u[9], u[4], u[5]});
}

TEST_CASE("Use index: a search over the bounding box rebuilds when a use touches its edge",
          "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  // Grown by one, the box shrunk by one still surrounds every use: no rebuild.
  CHECK(searched(u.index, db::Rect{-1, -1, 107, 104}).size() == 10);
  CHECK_FALSE(u.index.state_for_test().binned);
  // The box itself, shrunk by one, no longer surrounds ca_0 at (0,0).
  CHECK(searched(u.index, db::Rect{0, 0, 106, 103}).size() == 10);
  CHECK(u.index.state_for_test().binned);
}

TEST_CASE("Use index: exact lookups and whole-plane walks never rebuild", "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  db::CellUse& twin = u.add(db::Rect{40, 40, 44, 44});
  std::vector<db::CellUse*> equal;
  CHECK(u.index.find_equal(db::Rect{40, 40, 44, 44}, [&](db::CellUse& use) {
    equal.push_back(&use);
    return true;
  }));
  CHECK(equal == std::vector<db::CellUse*>{&twin, u[2]});  // newest first in its hash chain
  std::size_t visited = 0;
  CHECK(u.index.enumerate_all([&](db::CellUse&) {
    ++visited;
    return true;
  }));
  CHECK(visited == 11);
  CHECK_FALSE(u.index.state_for_test().binned);
}

TEST_CASE("Use index: equal boxes reverse their lookup order when the hash table grows",
          "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  db::CellUse& a = u.add(db::Rect{0, 0, 4, 4});
  db::CellUse& b = u.add(db::Rect{0, 0, 4, 4});
  for (int i = 0; i < 9; ++i) {
    u.add(db::Rect{10 * (i + 1), 0, 10 * (i + 1) + 4, 4});
  }
  std::vector<db::CellUse*> equal;
  const auto collect = [&](db::CellUse& use) {
    equal.push_back(&use);
    return true;
  };
  CHECK(u.index.find_equal(db::Rect{0, 0, 4, 4}, collect));
  CHECK(equal == std::vector<db::CellUse*>{&b, &a});
  u.add(db::Rect{200, 0, 204, 4});  // the 12th entry: iHashResize to 16 buckets re-adds every chain
  equal.clear();
  CHECK(u.index.find_equal(db::Rect{0, 0, 4, 4}, collect));
  CHECK(equal == std::vector<db::CellUse*>{&a, &b});
}

TEST_CASE("Use index: a second rebuild doubles the bounding box", "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  (void)searched(u.index, db::Rect{0, 0, 30, 30});
  REQUIRE(u.index.state_for_test().bin_area == db::Rect{0, 0, 106, 103});
  // Ten uses outside the bin area go to the in-box list (BPAdd).
  for (int i = 0; i < 10; ++i) {
    u.add(db::Rect{200 + 10 * i, 0, 204 + 10 * i, 4});
  }
  CHECK(u.index.state_for_test().unbinned.size() == 10);
  (void)searched(u.index, db::Rect{0, 0, 30, 30});
  const db::UseIndexState st = u.index.state_for_test();
  CHECK(st.unbinned.empty());
  // The exact box [0,0,294,103] grown by half its size on each side.
  CHECK(st.bin_area == db::Rect{-147, -51, 441, 154});
}

TEST_CASE("Use index: uses as wide as a bin go to the oversized list, walked after the bins",
          "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  // Wider and taller than half the array: bpBinArraySizeIt splits the
  // array in two rows of 52 (107 wide), and this use is oversized.
  db::CellUse& big = u.add(db::Rect{0, 0, 100, 100});
  (void)searched(u.index, db::Rect{0, 0, 30, 30});
  const std::vector<db::CellUse*> order = u.index.order(kOn);
  REQUIRE(order.size() == 11);
  CHECK(order.back() == &big);
  // Row 0 (y < 52) newest first after the build, then row 1.
  CHECK(order.front() == u[0]);
}

TEST_CASE("Use index: removal from the in-box list and from bins", "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  u.index.remove(*u[3]);
  CHECK(u.index.size() == 9);
  u.index.place(*u[3]);
  (void)searched(u.index, db::Rect{0, 0, 30, 30});
  REQUIRE(u.index.state_for_test().binned);
  u.index.remove(*u[0]);
  u.index.remove(*u[0]);  // absent: no-op
  const std::vector<db::CellUse*> order = u.index.order(kOn);
  CHECK(order.size() == 9);
  CHECK(order.front() == u[1]);
}

TEST_CASE("Use index: a bin rejecting 10 uses is sub-binned on the fly, unless the walk stopped first",
          "[cap:gds-write][req:Structure ordering]") {
  const auto build = [](Uses& u) {
    add_ten(u);
    (void)searched(u.index, db::Rect{0, 0, 30, 30});  // bins of 33 x 33
    for (int k = 0; k < 12; ++k) {
      u.add(db::Rect{2 * k, 10, 2 * k + 2, 12});  // bin 0, pushed at its head
    }
  };
  Uses full;
  build(full);
  Uses stopped;
  build(stopped);
  const std::vector<std::string> before = ids(full.index.order(kOn));
  REQUIRE(full.index.state_for_test().max_bin_population == 15);

  // [2,2,2,2] after the shrink: only ca_0 matches in bin 0; 14 rejects.
  CHECK(searched(full.index, db::Rect{1, 1, 3, 3}) == std::vector<db::CellUse*>{full[0]});
  CHECK(full.index.state_for_test().max_bin_population < 15);
  CHECK(ids(full.index.order(kOn)) != before);

  // Magic's lazy walk stops at ca_0, before it leaves bin 0.
  CHECK_FALSE(stopped.index.search(db::Rect{1, 1, 3, 3}, [](db::CellUse&) { return false; }));
  CHECK(stopped.index.state_for_test().max_bin_population == 15);
  CHECK(ids(stopped.index.order(kOn)) == before);
}

TEST_CASE("Use index: magic-compat OFF is the reverse of insertion order, whatever the history",
          "[cap:gds-write][req:Structure ordering]") {
  Uses u;
  add_ten(u);
  db::CellUse& extra = u.add(db::Rect{0, 40, 4, 44});
  (void)searched(u.index, db::Rect{0, 0, 30, 30});
  u.index.place(*u[0]);  // a re-placement keeps the insertion
  REQUIRE(u.index.order(kOn) != reversed(u));
  CHECK(u.index.order(kOff) == reversed(u));
  CHECK(u.index.order(kOff).front() == &extra);
}

TEST_CASE("Canonical order with magic-compat OFF", "[cap:gds-write][req:Structure ordering]") {
  db::CellDef child("ca", 1);
  child.set_bboxes(db::Rect{0, 0, 4, 4}, db::Rect{0, 0, 4, 4});
  db::CellDef top("top", 1);
  std::vector<db::CellUse*> placed;
  for (int i = 0; i < 12; ++i) {
    db::PlaceOptions o;
    o.id = "u" + std::to_string(i + 1);
    o.transform = db::translation(10 * i, 10 * (i % 3));
    auto use = db::place(top, child, o);
    REQUIRE(use.ok());
    placed.push_back(use.value());
  }
  // A DRC-like partial search bins the index first.
  (void)db::uses_overlapping(top, db::Rect{0, 0, 30, 30}, [](db::CellUse&) { return true; });
  REQUIRE(top.use_index().state_for_test().binned);
  const std::vector<db::CellUse*> expected(placed.rbegin(), placed.rend());
  CHECK(db::uses_in_search_order(top, kOff) == expected);  // u12 ... u1
  CHECK(db::uses_in_search_order(top, kOn) != expected);
}

TEST_CASE("Binned order with magic-compat ON", "[cap:gds-write][req:Structure ordering]") {
  db::CellDef ca("ca", 1);
  ca.set_bboxes(db::Rect{0, 0, 4, 4}, db::Rect{0, 0, 4, 4});
  db::CellDef top("top", 1);
  const db::Rect boxes[] = {{0, 0, 4, 4},     {20, 20, 24, 24}, {40, 40, 44, 44}, {60, 60, 64, 64},
                            {80, 80, 84, 84}, {100, 100, 104, 104}, {0, 20, 4, 24}, {20, 40, 24, 44},
                            {40, 60, 44, 64}, {60, 80, 64, 84}};
  std::vector<db::CellUse*> placed;
  for (const db::Rect& r : boxes) {
    db::PlaceOptions o;
    o.transform = db::translation(r.x0, r.y0);
    placed.push_back(db::place(top, ca, o).value());
  }
  (void)db::uses_overlapping(top, db::Rect{0, 0, 30, 30}, [](db::CellUse&) { return true; });
  // Square bins of 32 over [0,0,104,104]. The build pops the in-box list
  // newest first and pushes each use at its bin's head, so bin 0 lists
  // uses 0, 1, 6, bin 4 use 7, bin 5 uses 2, 3, 8, and so on.
  CHECK(db::uses_in_search_order(top, kOn) ==
        std::vector<db::CellUse*>{placed[0], placed[1], placed[6], placed[7], placed[2], placed[3], placed[8],
                                  placed[9], placed[4], placed[5]});
  // The one-argument helper keeps the M1-M4 order (newest first) until
  // task 5.2 moves its callers to the session's compat.
  CHECK(db::uses_in_search_order(top) ==
        std::vector<db::CellUse*>{placed[9], placed[8], placed[7], placed[6], placed[5], placed[4], placed[3],
                                  placed[2], placed[1], placed[0]});
  CHECK(db::uses_in_search_order(top) == db::uses_in_search_order(top, kOff));
}

TEST_CASE("Fresh-session write of a cell whose uses touch its bounding box", "[cap:gds-write][req:Structure ordering]") {
  db::CellDef ca("ca", 1);
  ca.set_bboxes(db::Rect{0, 0, 4, 4}, db::Rect{0, 0, 4, 4});
  db::CellDef top("top", 1);
  for (int i = 0; i < 12; ++i) {
    db::PlaceOptions o;
    o.transform = db::translation(9 * i, 9 * (i % 4));
    REQUIRE(db::place(top, ca, o).ok());
  }
  db::recompute_bbox(top);
  REQUIRE(top.bbox() == db::Rect{0, 0, 103, 31});
  const std::vector<db::CellUse*> unbinned = db::uses_in_search_order(top, kOn);
  // The subtree-loading search of a write (DBCellReadArea over the root's
  // bounding box) is non-surrounding: the use at (0,0) touches the edge.
  db::search_subtree(top, top.bbox());
  CHECK(top.use_index().state_for_test().binned);
  CHECK(db::uses_in_search_order(top, kOn) != unbinned);
}

TEST_CASE("Use index: the subtree-loading search descends into children", "[cap:gds-write][req:Structure ordering]") {
  db::CellDef leaf("leaf", 1);
  leaf.set_bboxes(db::Rect{0, 0, 2, 2}, db::Rect{0, 0, 2, 2});
  db::CellDef mid("mid", 1);
  for (int i = 0; i < 10; ++i) {
    db::PlaceOptions o;
    o.transform = db::translation(3 * i, 0);
    REQUIRE(db::place(mid, leaf, o).ok());
  }
  db::recompute_bbox(mid);
  db::CellDef top("top", 1);
  db::PlaceOptions o;
  o.transform = db::translation(100, 100);
  REQUIRE(db::place(top, mid, o).ok());
  db::recompute_bbox(top);
  // Over part of the top: mid's area [0,0,10,10] does not surround its uses.
  db::search_subtree(top, db::Rect{100, 100, 110, 110});
  CHECK(mid.use_index().state_for_test().binned);
  CHECK_FALSE(top.use_index().state_for_test().binned);  // one use only
}

namespace {

/// A child whose font label makes its extended box (0,0)-(20,10) larger
/// than its bounding box (0,0)-(10,10), used by `u0` in `top` before `u1`
/// of another child; then a label grows the bounding box to the old
/// extended one and the child's boxes are recomputed.
struct ExtendedBoxCase {
  db::CellDef child{"child", 1};
  db::CellDef other{"other", 1};
  db::CellDef top{"top", 1};
  db::CellUse* u0 = nullptr;
  db::CellUse* u1 = nullptr;

  explicit ExtendedBoxCase(bool magic_compat) {
    db::Label font_label;
    font_label.text = "a";
    font_label.rect = db::Rect{0, 0, 10, 10};
    font_label.font = 0;
    font_label.text_bbox = db::Rect{0, 0, 20, 10};
    child.labels().push_back(font_label);
    db::recompute_bbox(child, magic_compat);
    REQUIRE(child.bbox() == db::Rect{0, 0, 10, 10});
    REQUIRE(child.extended_bbox() == db::Rect{0, 0, 20, 10});
    other.set_bboxes(db::Rect{0, 0, 4, 4}, db::Rect{0, 0, 4, 4});
    u0 = db::place(top, child, db::PlaceOptions{}).value();
    db::PlaceOptions o;
    o.transform = db::translation(100, 0);
    u1 = db::place(top, other, o).value();
    REQUIRE(db::uses_in_search_order(top, kOn) == std::vector<db::CellUse*>{u1, u0});
    db::Label area_label;
    area_label.text = "b";
    area_label.rect = db::Rect{0, 0, 20, 10};
    child.labels().push_back(area_label);
    db::recompute_bbox(child, magic_compat);
    REQUIRE(child.bbox() == db::Rect{0, 0, 20, 10});
  }
};

}  // namespace

TEST_CASE("A bounding box grown to the old extended box keeps the uses in place with magic-compat ON",
          "[cap:gds-write][req:Structure ordering]") {
  ExtendedBoxCase c{true};
  // DBReComputeBbox returns before the rip-up (DBcellbox.c:709-713).
  CHECK(db::uses_in_search_order(c.top, kOn) == std::vector<db::CellUse*>{c.u1, c.u0});
  CHECK(c.u0->bbox == db::Rect{0, 0, 20, 10});
  // The index holds the new box: a search over the grown part finds u0.
  CHECK(searched(c.top.use_index(), db::Rect{14, 0, 18, 10}) == std::vector<db::CellUse*>{c.u0});
}

TEST_CASE("A bounding box grown to the old extended box re-places the uses with magic-compat OFF",
          "[cap:gds-write][req:Structure ordering]") {
  ExtendedBoxCase c{false};
  CHECK(db::uses_in_search_order(c.top, kOn) == std::vector<db::CellUse*>{c.u0, c.u1});
  CHECK(searched(c.top.use_index(), db::Rect{14, 0, 18, 10}) == std::vector<db::CellUse*>{c.u0});
}
