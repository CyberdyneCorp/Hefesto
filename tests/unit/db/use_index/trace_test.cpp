// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Structure ordering": replays the use-order traces recorded
// from Magic 8.3.694 (tests/data/db/use_index/MASK-UO-*.trace, format in
// that directory's README.md) against db::UseIndex and checks every SREF
// and CIF call order they captured.
//
// The replay drives the database as the probe drove Magic: `getcell` is
// db::place, `expand` one overlap search, `select cell` + `delete` two 1x1
// overlap searches at the use's lower-left corner and db::remove_use, and a
// child's growth db::recompute_bbox. A write makes Magic's searches at
// Magic's points (design D7): the subtree-loading search over the root's
// bounding box (db::search_subtree), then the GDS order is sampled before
// the root's CIFGenSubcells and the CIF calls after it. CIFGenSubcells is
// emulated by the interaction finder's overlap searches over its squares
// (step 50 for the probe style, radius 0): the search of each square and
// then of each found use's halo area. The finder's last pass
// (DBTreeSrCells over the interaction area) is not replayed; it could only
// change the index by sub-binning a bin of 10 or more uses, and the test
// checks that no bin is that full.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"

namespace db = hefesto::db;
namespace fs = std::filesystem;

namespace {

constexpr db::Coord kProbeStep = 50;  // CIFGenSubcells: stepsize 0, radius 0 -> 50

db::Label label(const char* text, const db::Rect& r) {
  db::Label l;
  l.text = text;
  l.rect = r;
  return l;
}

// GeoClip.
db::Rect clip(const db::Rect& r, const db::Rect& area) {
  return db::Rect{std::max(r.x0, area.x0), std::max(r.y0, area.y0), std::min(r.x1, area.x1), std::min(r.y1, area.y1)};
}

db::Rect read_rect(std::istringstream& in) {
  db::Rect r;
  in >> r.x0 >> r.y0 >> r.x1 >> r.y1;
  return r;
}

std::string read_text(const fs::path& path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// The paint of the root that frames the uses: `box` + `paint` pairs of the
/// case script, or the `rect` lines of a `.mag` file it reads.
std::vector<db::Rect> script_frame(const std::string& script) {
  std::vector<db::Rect> out;
  std::istringstream lines(script);
  std::string line;
  db::Rect box;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string word;
    in >> word;
    if (word == "box") {
      box = read_rect(in);
    } else if (word == "paint") {
      out.push_back(box);
    }
  }
  return out;
}

std::vector<db::Rect> mag_frame(const fs::path& mag) {
  std::vector<db::Rect> out;
  std::istringstream lines(read_text(mag));
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream in(line);
    std::string word;
    in >> word;
    if (word == "rect") {
      out.push_back(read_rect(in));
    }
  }
  return out;
}

class Replay {
 public:
  explicit Replay(std::string name) : name_(std::move(name)) {
    child("ca", db::Rect{0, 0, 4, 4});
    child("cb", db::Rect{0, 0, 6, 3});
    const std::string script = read_text(fs::path{HEFESTO_TEST_DATA_DIR} / (name_ + ".tcl"));
    hier_write_ = script.find("cif *hier write disable") == std::string::npos;
    add_frame(script_frame(script));
  }

  void run() {
    std::istringstream lines(read_text(fs::path{HEFESTO_TEST_DATA_DIR} / (name_ + ".trace")));
    std::string line;
    bool ended = false;
    while (std::getline(lines, line)) {
      INFO(name_ << ": " << line);
      ended = step(line) || ended;
    }
    CHECK(ended);
    CHECK(orders_checked_ > 0);
  }

 private:
  void child(const std::string& name, const db::Rect& box) {
    auto def = std::make_unique<db::CellDef>(name, 1);
    def->labels().push_back(label("box", box));
    db::recompute_bbox(*def);
    defs_[name] = std::move(def);
  }

  void add_frame(const std::vector<db::Rect>& rects) {
    for (const db::Rect& r : rects) {
      top_.labels().push_back(label("frame", r));
    }
    db::recompute_bbox(top_);
  }

  // One trace line; true at `end`.
  bool step(const std::string& line) {
    std::istringstream in(line);
    std::string kind;
    int n = 0;
    in >> kind;
    if (kind == "end") {
      return true;
    }
    if (kind == "trace" || kind == "root") {
      return false;
    }
    in >> n;
    dispatch(kind, in);
    return false;
  }

  void dispatch(const std::string& kind, std::istringstream& in) {
    if (kind == "place") {
      place(in);
    } else if (kind == "readmag") {
      readmag(in);
    } else if (kind == "search") {
      search(read_rect(in));
    } else if (kind == "remove") {
      remove(in);
    } else if (kind == "equal") {
      equal(in);
    } else if (kind == "instances") {
      instances(in);
    } else if (kind == "resize") {
      resize(in);
    } else if (kind == "bbox") {
      bbox(in);
    } else if (kind == "write") {
      write(in);
    } else if (kind == "order") {
      order(in);
    } else {
      CHECK(kind != "error");  // childinst, structures, symbols: not use orders
    }
  }

  void place(std::istringstream& in) {
    std::string id;
    std::string def;
    in >> id >> def;
    const db::Rect r = read_rect(in);
    db::PlaceOptions o;
    o.id = id;
    o.transform = db::translation(r.x0, r.y0);
    auto use = db::place(top_, *defs_.at(def), o);
    REQUIRE(use.ok());
    CHECK(use.value()->bbox == r);
    uses_[id] = use.value();
    db::recompute_bbox(top_);
  }

  void readmag(std::istringstream& in) {
    std::string cell;
    std::string path;
    in >> cell >> path;
    add_frame(mag_frame(fs::path{HEFESTO_SOURCE_DIR} / "tests/golden/MASK/cases" / name_ / path));
  }

  void search(const db::Rect& area) const {
    (void)db::uses_overlapping(top_, area, [](db::CellUse&) { return true; });
  }

  void remove(std::istringstream& in) {
    std::string id;
    in >> id;
    const db::Rect r = read_rect(in);
    const db::Rect corner{r.x0, r.y0, r.x0 + 1, r.y0 + 1};
    search(corner);  // DBSeeTypesAll
    search(corner);  // selEnumCFunc1
    REQUIRE(db::remove_use(*uses_.at(id)).ok());
    uses_.erase(id);
    db::recompute_bbox(top_);
  }

  void equal(std::istringstream& in) {
    std::string id;
    std::string def;
    in >> id >> def;
    const db::Rect r = read_rect(in);
    std::string outcome;
    in >> outcome;
    bool duplicate = false;
    (void)top_.use_index().find_equal(r, [&](db::CellUse& u) {
      duplicate = duplicate || u.def == defs_.at(def).get();
      return true;
    });
    CHECK(duplicate);
    CHECK(outcome == "rejected");
  }

  void instances(std::istringstream& in) {
    std::string def;
    in >> def;
    std::vector<std::string> expected;
    for (std::string id; in >> id;) {
      expected.push_back(id);
    }
    std::vector<std::string> got;
    for (const db::CellUse* u : defs_.at(def)->instances()) {
      got.push_back(u->id);
    }
    CHECK(got == expected);
  }

  void resize(std::istringstream& in) {
    std::string def;
    in >> def;
    db::CellDef& child = *defs_.at(def);
    child.labels().push_back(label("grow", read_rect(in)));
    db::recompute_bbox(child);  // DBReComputeBbox: rip up and re-place the uses
  }

  void bbox(std::istringstream& in) {
    std::string id;
    in >> id;
    CHECK(uses_.at(id)->bbox == read_rect(in));
  }

  void write(std::istringstream& in) {
    std::string format;
    in >> format;
    db::search_subtree(top_, top_.bbox());  // DBCellReadArea (CalmaWrite.c:316, CIFwrite.c:137)
    if (format == "gds") {
      sampled_ = ids(db::uses_in_search_order(top_, hefesto::compat::CompatSettings{}));  // CalmaWrite.c:1486
      if (hier_write_) {
        gen_subcells();
      }
    } else {
      gen_subcells();  // the CIF writer ignores CIFhier (Q-MASK-410)
      sampled_ = ids(db::uses_in_search_order(top_, hefesto::compat::CompatSettings{}));  // CIFwrite.c:467
    }
    CHECK(top_.use_index().state_for_test().max_bin_population < 10);
  }

  // CIFGenSubcells' DRCFindInteractions searches over the root's squares.
  void gen_subcells() const {
    const db::Rect total = top_.bbox();
    for (db::Coord y = total.y0; y < total.y1; y += kProbeStep) {
      for (db::Coord x = total.x0; x < total.x1; x += kProbeStep) {
        const db::Rect square{x, y, std::min(x + kProbeStep, total.x1), std::min(y + kProbeStep, total.y1)};
        find_interactions(square);
      }
    }
  }

  void find_interactions(const db::Rect& square) const {
    std::vector<db::Rect> linked;  // dsa_dlu is a stack
    (void)db::uses_overlapping(top_, square, [&](db::CellUse& u) {
      linked.insert(linked.begin(), clip(u.bbox, square));
      return true;
    });
    for (const db::Rect& halo : linked) {
      search(halo);  // drcFindOtherCells
    }
  }

  void order(std::istringstream& in) {
    std::string format;
    std::string cell;
    in >> format >> cell;
    std::vector<std::string> expected;
    for (std::string id; in >> id;) {
      expected.push_back(id);
    }
    CHECK(cell == "top");
    CHECK(sampled_ == expected);
    ++orders_checked_;
  }

  static std::vector<std::string> ids(const std::vector<db::CellUse*>& uses) {
    std::vector<std::string> out;
    for (const db::CellUse* u : uses) {
      out.push_back(u->id);
    }
    return out;
  }

  std::string name_;
  bool hier_write_ = true;
  std::map<std::string, std::unique_ptr<db::CellDef>> defs_;
  db::CellDef top_{"top", 1};
  std::map<std::string, db::CellUse*> uses_;
  std::vector<std::string> sampled_;
  int orders_checked_ = 0;
};

void replay(const std::string& name) {
  Replay r(name);
  r.run();
}

}  // namespace

TEST_CASE("Use-order traces: placements, exact lookups and surrounding searches keep insertion order",
          "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-001");
  replay("MASK-UO-002");
}

TEST_CASE("Use-order traces: the rebuild threshold and the bins", "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-003");
  replay("MASK-UO-004");
}

TEST_CASE("Use-order traces: removals", "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-005");
}

TEST_CASE("Use-order traces: DBReComputeBbox re-places a grown child's uses",
          "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-006");
  replay("MASK-UO-007");
}

TEST_CASE("Use-order traces: .mag read order and a partial search", "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-008");
}

TEST_CASE("Use-order traces: one GDS and one CIF write in a fresh session",
          "[cap:gds-write][req:Structure ordering]") {
  replay("MASK-UO-009");
  replay("MASK-UO-010");
  replay("MASK-UO-011");
  replay("MASK-UO-012");
}
