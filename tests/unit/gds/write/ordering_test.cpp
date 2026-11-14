// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// gds-write "Write preconditions and subtree loading" (Q-MASK-219),
// "Structure ordering" (Q-MASK-204, the index state of the prepare phase),
// "Structure naming" (Q-MASK-209) and the abstract-view rule of
// "Abstract views" (Q-MASK-226) that the traversal applies.
// Owner: GDSW.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "hefesto/db/use_index.hpp"
#include "write_fixture.hpp"

namespace gds_write_test {
namespace {

// `top` with `count` uses u1..uN of a 10x10 cell in a row, placed in order.
db::CellDef& row_of_uses(Fixture& f, int count) {
  db::CellDef& leaf = f.cell("leaf");
  f.paint(leaf, f.m1, db::Rect{0, 0, 10, 10});
  db::CellDef& top = f.cell("top");
  for (int i = 1; i <= count; ++i) {
    f.place(top, leaf, db::translation(20 * (i - 1), 0), {}, "u" + std::to_string(i));
  }
  return top;
}

std::vector<std::string> numbered(int from, int to) {
  std::vector<std::string> out;
  for (int i = from; from <= to ? i <= to : i >= to; from <= to ? ++i : --i) {
    out.push_back("u" + std::to_string(i));
  }
  return out;
}

TEST_CASE("post-order with a shared child", "[cap:gds-write][req:Structure ordering]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& c = f.cell("c");
    f.paint(c, f.m1, db::Rect{0, 0, 2, 2});
    db::CellDef& a = f.cell("a");
    f.place(a, c);
    db::CellDef& b = f.cell("b");
    f.place(b, c);
    db::CellDef& top = f.cell("top");
    f.place(top, a, {}, {}, "a_0");
    f.place(top, b, db::translation(10, 0), {}, "b_0");
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    CHECK(structure_names(recs) == std::vector<std::string>{"c", "b", "a", "top"});
    CHECK(use_names(structure(recs, "top")) == std::vector<std::string>{"b_0", "a_0"});
  }
}

TEST_CASE("cell id numbers are assigned in pre-order of first visit", "[cap:gds-write][req:Structure ordering]") {
  // Illegal names make the ids visible (XXXXX<id>).
  Fixture f;
  db::CellDef& c = f.cell("c\xC3\xA9");
  f.paint(c, f.m1, db::Rect{0, 0, 2, 2});
  db::CellDef& a = f.cell("a\xC3\xA9");
  f.place(a, c);
  db::CellDef& b = f.cell("b\xC3\xA9");
  f.place(b, c);
  db::CellDef& top = f.cell("top\xC3\xA9");
  f.place(top, a);
  f.place(top, b, db::translation(10, 0));
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  // top = 1, b = 2, c = 3, a = 4.
  CHECK(structure_names(recs) == std::vector<std::string>{"XXXXX3", "XXXXX2", "XXXXX4", "XXXXX1"});
  CHECK(recs[2].text() == "XXXXX1");  // LIBNAME
}

TEST_CASE("canonical use order with magic-compat OFF", "[cap:gds-write][req:Structure ordering]") {
  Fixture f(false);
  db::CellDef& top = row_of_uses(f, 12);
  // A search over part of `top` (as a DRC check makes) bins its index.
  (void)db::uses_overlapping(top, db::Rect{0, 0, 30, 10}, [](db::CellUse&) { return true; });
  REQUIRE(top.use_index().state_for_test().binned);
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(use_names(structure(recs, "top")) == numbered(12, 1));
}

TEST_CASE("binned use order with magic-compat ON", "[cap:gds-write][req:Structure ordering]") {
  Fixture f(true);
  db::CellDef& top = row_of_uses(f, 12);
  (void)db::uses_overlapping(top, db::Rect{0, 0, 30, 10}, [](db::CellUse&) { return true; });
  REQUIRE(top.use_index().state_for_test().binned);
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  std::vector<std::string> walk;
  for (const db::CellUse* u : db::uses_in_search_order(top, f.compat)) {
    walk.push_back(u->id);
  }
  const std::vector<std::string> written = use_names(structure(recs, "top"));
  CHECK(written == walk);
  CHECK(written != numbered(12, 1));
}

TEST_CASE("a fresh-session write bins a root whose uses touch its bounding box",
          "[cap:gds-write][req:Structure ordering]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = row_of_uses(f, 12);
    REQUIRE_FALSE(top.use_index().state_for_test().binned);
    REQUIRE(top.use_index().order(f.compat).size() == 12);
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    // The subtree-loading search over top's bounding box is non-surrounding
    // (its shrunk area does not surround the uses at the edges), so it bins.
    CHECK(top.use_index().state_for_test().binned);
    const std::vector<std::string> written = use_names(structure(recs, "top"));
    if (on) {
      std::vector<std::string> walk;
      for (const db::CellUse* u : db::uses_in_search_order(top, f.compat)) {
        walk.push_back(u->id);
      }
      CHECK(written == walk);
      CHECK(written != numbered(12, 1));
    } else {
      CHECK(written == numbered(12, 1));
    }
  }
}

TEST_CASE("fewer than ten uses stay unbinned", "[cap:gds-write][req:Structure ordering]") {
  Fixture f;
  db::CellDef& top = row_of_uses(f, 9);
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK_FALSE(top.use_index().state_for_test().binned);
  CHECK(use_names(structure(recs, "top")) == numbered(9, 1));
}

TEST_CASE("library mode omits the root", "[cap:gds-write][req:Structure ordering]") {
  Fixture f;
  db::CellDef& inv = f.cell("inv");
  f.paint(inv, f.m1, db::Rect{0, 0, 2, 2});
  db::CellDef& top = f.cell("top");
  f.place(top, inv);
  f.options.settings.library = true;
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"inv"});
  CHECK(recs[2].record() == gds::RecordType::libname);
  CHECK(recs[2].text() == "top");
}

TEST_CASE("no output style: nothing is written and no file is opened",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.ed.mask.output_styles = mask::OutputStyleSet{};
    REQUIRE(f.fs.write_atomic("/old.gds", "keep").ok());
    f.last = gds::write(f.ed, top, "/old.gds", f.options);
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("GDS-WRITE-002"));
    CHECK(f.file("/old.gds") == "keep");
    f.last = gds::write(f.ed, top, "/new.gds", f.options);
    CHECK_FALSE(f.fs.exists("/new.gds"));
  }
}

TEST_CASE("a missing subcell with undefined disallowed or allowed",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  Fixture f;
  db::CellDef& missing = f.cell("missing");
  missing.flags().available = false;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, db::Rect{0, 0, 4, 4});
  f.place(top, missing);
  bool asked = false;
  f.options.loader.read = [&asked](db::CellDef&) {
    asked = true;
    return false;
  };
  REQUIRE(f.fs.write_atomic("/out.gds", "old contents").ok());
  const std::string failed = f.write(top);
  CHECK(asked);
  CHECK_FALSE(f.last.ok());
  CHECK(f.has_code("GDS-WRITE-003"));
  REQUIRE_FALSE(f.last.diagnostics.empty());
  CHECK(f.last.diagnostics.back().text ==
        "Failure to read entire subtree of the cell.\nFailed on cell missing.");
  CHECK(failed.empty());  // the file exists and holds no records

  f.options.settings.allow_undefined = true;
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"top"});
  bool referenced = false;
  for (const Rec& r : recs) {
    referenced = referenced || (r.record() == gds::RecordType::sname && r.text() == "missing");
  }
  CHECK(referenced);
}

TEST_CASE("the prepare phase fixes timestamp mismatches after loading",
          "[cap:gds-write][req:Write preconditions and subtree loading]") {
  Fixture f;
  db::CellDef& child = f.cell("child");
  child.flags().available = false;
  db::CellDef& top = f.cell("top");
  f.place(top, child);
  std::vector<std::string> steps;
  f.options.loader.read = [&](db::CellDef& def) {
    steps.push_back("read " + def.name());
    def.flags().available = true;
    return true;
  };
  f.options.loader.fix_mismatches = [&] { steps.push_back("fix"); };
  (void)f.write(top);
  REQUIRE(f.last.ok());
  CHECK(steps == std::vector<std::string>{"read child", "fix"});
}

TEST_CASE("structure names: mapped, replaced and limited", "[cap:gds-write][req:Structure naming]") {
  Fixture f;
  db::CellDef& nand = f.cell("nand-2");
  db::CellDef& cell = f.cell("c\xC3\xA9lula");
  db::CellDef& longname = f.cell(std::string(40, 'q'));
  db::CellDef& top = f.cell("top");
  f.place(top, longname);  // visited third (id 4)
  f.place(top, cell);      // visited second (id 3)
  f.place(top, nand);      // visited first (id 2)
  f.style.options |= mask::style_option::string_limit;
  const std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"nand_2", "XXXXX3", "XXXXX4", "top"});
  std::vector<std::string> snames;
  for (const Rec& r : structure(recs, "top")) {
    if (r.record() == gds::RecordType::sname) {
      snames.push_back(r.text());
    }
  }
  CHECK(snames == std::vector<std::string>{"nand_2", "XXXXX3", "XXXXX4"});
  CHECK(f.has_code("GDS-WRITE-006"));
}

TEST_CASE("colliding mapped names are uniquified", "[cap:gds-write][req:Structure naming]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& dash = f.cell("a-b");
    db::CellDef& under = f.cell("a_b");
    db::CellDef& top = f.cell("top");
    f.place(top, under);
    f.place(top, dash);  // newest first: a-b is visited first (id 2), a_b second (id 3)
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    CHECK(structure_names(recs) == std::vector<std::string>{"a_b", "XXXXX3", "top"});
    std::vector<std::string> snames;
    for (const Rec& r : structure(recs, "top")) {
      if (r.record() == gds::RecordType::sname) {
        snames.push_back(r.text());
      }
    }
    CHECK(snames == std::vector<std::string>{"a_b", "XXXXX3"});
    CHECK(f.has_code("GDS-WRITE-007"));
  }
}

TEST_CASE("a renamed name that is taken too gets a suffix", "[cap:gds-write][req:Structure naming]") {
  Fixture f;
  db::CellDef& taken = f.cell("XXXXX3");
  db::CellDef& dash = f.cell("a-b");
  db::CellDef& under = f.cell("a_b");
  db::CellDef& top = f.cell("top");
  f.place(top, under);  // id 4: a_b collides with a-b, and XXXXX4 is free
  f.place(top, taken);  // id 3
  f.place(top, dash);   // id 2
  std::vector<Rec> recs = decode(f.write(top));
  REQUIRE(f.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"a_b", "XXXXX3", "XXXXX4", "top"});

  Fixture g;
  db::CellDef& d2 = g.cell("a-b");
  db::CellDef& u2 = g.cell("a_b");
  db::CellDef& t2 = g.cell("XXXXX3");
  db::CellDef& top2 = g.cell("top");
  g.place(top2, t2);  // id 4, written last: XXXXX3 is a_b's by then
  g.place(top2, u2);  // id 3: collides, becomes XXXXX3
  g.place(top2, d2);  // id 2
  recs = decode(g.write(top2));
  REQUIRE(g.last.ok());
  CHECK(structure_names(recs) == std::vector<std::string>{"a_b", "XXXXX3", "XXXXX4", "top"});
}

TEST_CASE("an abstract view fails the write unless allowed", "[cap:gds-write][req:Abstract views]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& macro = f.cell("macro");
    f.paint(macro, f.m1, db::Rect{0, 0, 2, 2});
    macro.properties().put("LEFview", std::string{"TRUE"});
    db::CellDef& top = f.cell("top");
    f.place(top, macro);
    const std::string bytes = f.write(top);
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("GDS-WRITE-008"));
    if (on) {
      const std::vector<Rec> recs = decode(bytes);
      CHECK(structure_names(recs).empty());
      CHECK(recs.back().record() == gds::RecordType::endlib);
    } else {
      CHECK_FALSE(f.fs.exists("/out.gds"));
    }
    f.options.settings.allow_abstract = true;
    const std::vector<Rec> recs = decode(f.write(top));
    REQUIRE(f.last.ok());
    CHECK(structure_names(recs) == std::vector<std::string>{"macro", "top"});
    CHECK(f.has_code("GDS-WRITE-009"));
  }
}

}  // namespace
}  // namespace gds_write_test
