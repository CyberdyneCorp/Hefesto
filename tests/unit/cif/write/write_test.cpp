// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Whole CIF writes (cif::write, prepare_write, emit_write): capability
// cif-format-io "CIF output header", "CIF output symbol numbering and
// order", "CIF output symbol definition format", "CIF output labels", "CIF
// output calls and arrays", "CIF output preconditions and flat output",
// "cif read, write and flat options"; mask-hierarchical-generation
// "Hierarchy and array output switches" and "Flat CIF output" for the CIF
// writer.
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "../cif_fixture.hpp"
#include "hefesto/mask/gen/progress.hpp"

namespace {

using namespace cif_test;  // NOLINT(google-build-using-namespace): the fixture's aliases

bool contains(const std::vector<std::string>& lines, const std::string& line) {
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}

std::string joined(const std::vector<std::string>& lines) {
  std::string out;
  for (const std::string& l : lines) {
    out += l + "\n";
  }
  return out;
}

std::size_t index_of(const std::vector<std::string>& lines, const std::string& line) {
  const auto it = std::find(lines.begin(), lines.end(), line);
  REQUIRE(it != lines.end());
  return static_cast<std::size_t>(it - lines.begin());
}

}  // namespace

TEST_CASE("the preamble of a write", "[cap:cif-format-io][req:CIF output header]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.paint(top, "metal1", db::Rect{0, 0, 4, 2});
    cif::CifWriteOptions o = f.options();
    o.user = "ada";
    o.machine = "box1";
    const std::vector<std::string> lines = lines_of(f.write_with(top, o));
    REQUIRE(lines.size() > 10);
    CHECK(lines[0] == "( @@user : ada );");
    CHECK(lines[1] == "( @@machine : box1 );");
    CHECK(lines[2] == "( @@source : ? );");
    CHECK(lines[3].starts_with("( @@tool : Hefesto "));
    CHECK(lines[4].starts_with("( @@compiled : "));
    CHECK(lines[5] == "( @@technology : masktest );");
    CHECK(lines[8 - (on ? 0 : 1)] == "( @@style : gds );");
    CHECK(lines[9 - (on ? 0 : 1)] == "( @@date : Tue Nov 14 22:13:20 2023 );");
    // masktest has no version section: Magic shows the start-up technology's
    // (DBTechVersion is never cleared); with magic-compat OFF "unknown".
    if (on) {
      CHECK(lines[6] == "( @@version : 0.0 );");
      CHECK(lines[7] == "( @@techdesc : Minimum technology file structure );");
    } else {
      CHECK(lines[6] == "( @@version : unknown );");
      CHECK(lines[7].starts_with("( @@style"));
    }
  }
}

TEST_CASE("the source line is the root's file without .mag", "[cap:cif-format-io][req:CIF output header]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  top.set_file_path("input/top.mag");
  CHECK(lines_of(f.write(top))[2] == "( @@source : input/top );");
}

TEST_CASE("box formatting and the file end", "[cap:cif-format-io][req:CIF output symbol definition format]") {
  Fixture f;
  db::CellDef& top = f.cell("inv");
  f.paint(top, "metal1", db::Rect{0, 0, 4, 2});  // (0,0)-(40,20) in output units, R = 10
  const std::vector<std::string> b = body(f.write(top));
  CHECK(b == std::vector<std::string>{"DS 1 10 2;", "9 inv;", "L CMF;", "    B 8 4 4 2;", "DF;", "C 1;", "End"});
  CHECK(f.last.ok());
}

TEST_CASE("a triangle is a P line and empty layers have no L line",
          "[cap:cif-format-io][req:CIF output symbol definition format]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, "metal2", db::Rect{0, 0, 1, 1}, db::TriangleSpec{db::Diagonal::ne_sw, true});
  const std::vector<std::string> b = body(f.write(top));
  CHECK(contains(b, "L CMS;"));
  CHECK(contains(b, "    P 0 0 2 0 2 2;"));
  CHECK_FALSE(contains(b, "L CMF;"));
}

TEST_CASE("the (UNNAMED) cell and the path prefix", "[cap:cif-format-io][req:CIF output symbol definition format]") {
  Fixture f;
  db::CellDef& unnamed = f.cell("");
  REQUIRE(unnamed.name() == "(UNNAMED)");
  CHECK(contains(body(f.write(unnamed)), "9 UNNAMED;"));
  f.ed.mask.cif.prefix = "/lib";
  db::CellDef& inv = f.cell("inv");
  CHECK(contains(body(f.write(inv)), "9 /lib/inv;"));
  CHECK(contains(body(f.write(unnamed)), "9 UNNAMED;"));
}

TEST_CASE("symbols are numbered and written in Magic's stack order",
          "[cap:cif-format-io][req:CIF output symbol numbering and order]") {
  // magic-compat OFF: the canonical newest-first order, so placing b then a
  // enumerates a, b.
  Fixture f(false);
  db::CellDef& top = f.cell("top");
  db::CellDef& a = f.cell("a");
  db::CellDef& b = f.cell("b");
  db::CellDef& c = f.cell("c");
  f.paint(c, "metal1", db::Rect{0, 0, 1, 1});
  f.place(a, c);
  f.place(top, b, db::Transform{1, 0, 10, 0, 1, 0});
  f.place(top, a);
  const std::vector<std::string> lines = body(f.write(top));
  // top = 1, a = 2, b = 3; b (3) is written, then a (2), which numbers c = 4.
  const std::size_t d1 = index_of(lines, "DS 1 10 2;");
  const std::size_t d3 = index_of(lines, "DS 3 10 2;");
  const std::size_t d2 = index_of(lines, "DS 2 10 2;");
  const std::size_t d4 = index_of(lines, "DS 4 10 2;");
  CHECK(d1 < d3);
  CHECK(d3 < d2);
  CHECK(d2 < d4);
  CHECK(lines[d3 + 1] == "9 b;");
  CHECK(lines[d2 + 1] == "9 a;");
  CHECK(lines[d4 + 1] == "9 c;");
  CHECK(lines[lines.size() - 2] == "C 1;");
  CHECK(lines.back() == "End");
  // The calls of top in enumeration order: a (2) before b (3).
  const std::size_t call_a = index_of(lines, "C 2 R 1 0 T 0 0;");
  const std::size_t call_b = index_of(lines, "C 3 R 1 0 T 20 0;");
  CHECK(call_a < call_b);
}

TEST_CASE("a cell that is shared is written once", "[cap:cif-format-io][req:CIF output symbol numbering and order]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    db::CellDef& leaf = f.cell("leaf");
    db::CellDef& mid = f.cell("mid");
    f.place(mid, leaf);
    f.place(top, leaf);
    f.place(top, mid);
    const std::vector<std::string> lines = body(f.write(top));
    CHECK(std::count(lines.begin(), lines.end(), "9 leaf;") == 1);
    CHECK(std::count(lines.begin(), lines.end(), "9 mid;") == 1);
    CHECK(std::count_if(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("DS "); }) == 3);
  }
}

TEST_CASE("labels with and without an output label layer, and area labels",
          "[cap:cif-format-io][req:CIF output labels]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, "metal1", db::Rect{0, 0, 8, 2});
  f.label(top, "out", db::Rect{4, 1, 4, 1}, "metal1");      // metal1 labels go on CMF
  f.label(top, "n", db::Rect{0, 0, 2, 2}, "ndiffusion");    // no label layer
  std::vector<std::string> b = body(f.write(top));
  CHECK(contains(b, "94 out 8 2 CMF;"));
  CHECK(contains(b, "94 n 2 2;"));
  CHECK(index_of(b, "94 out 8 2 CMF;") < index_of(b, "94 n 2 2;"));  // the label list order
  f.ed.mask.cif.arealabels = true;
  b = body(f.write(top));
  CHECK(contains(b, "95 out 0 0 8 2 CMF;"));
  CHECK(contains(b, "95 n 4 4 2 2;"));
}

TEST_CASE("label text with a space in both modes (Q-MASK-322)", "[cap:cif-format-io][req:CIF output labels]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    f.label(top, "out 1", db::Rect{4, 1, 4, 1}, "metal1");
    const std::vector<std::string> b = body(f.write(top));
    CHECK(contains(b, on ? "94 out 1 8 2 CMF;" : "94 out_1 8 2 CMF;"));
    REQUIRE(f.has_code("CIF-WRITE-006"));
    const auto warned = std::find_if(f.last.diagnostics.begin(), f.last.diagnostics.end(),
                                     [](const diag::Diagnostic& d) { return d.code == "CIF-WRITE-006"; });
    CHECK(warned->native_only == on);
    if (!on) {
      CHECK(warned->text == "Warning: characters changed in string 'out 1'; modified string is 'out_1'");
    }
    // The layout's own label is never changed.
    CHECK(top.labels().front().text == "out 1");
  }
}

TEST_CASE("arrays with ids and cif idcell no", "[cap:cif-format-io][req:CIF output calls and arrays]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  db::CellDef& leaf = f.cell("leaf");
  f.paint(leaf, "metal1", db::Rect{0, 0, 1, 1});
  f.place(top, leaf, db::Transform{}, db::ArrayInfo{0, 0, 0, 2, 0, 5}, std::string{"a"});
  std::vector<std::string> b = body(f.write(top));
  CHECK(contains(b, "91 a(0);"));
  CHECK(contains(b, "91 a(2);"));
  CHECK(contains(b, "C 2 R 1 0 T 0 0;"));
  CHECK(contains(b, "C 2 R 1 0 T 0 10;"));
  CHECK(contains(b, "C 2 R 1 0 T 0 20;"));  // ty = (10 * 2 * 10) / 10
  f.ed.mask.cif.idcell = false;
  b = body(f.write(top));
  CHECK_FALSE(contains(b, "91 a(0);"));
  CHECK(contains(b, "C 2 R 1 0 T 0 20;"));
}

TEST_CASE("flat output: __CIF2__ with magic-compat ON, the root's name and labels OFF",
          "[cap:cif-format-io][req:CIF output preconditions and flat output]"
          "[cap:mask-hierarchical-generation][req:Flat CIF output]") {
  for (const bool on : {true, false}) {
    Fixture f(on);
    db::CellDef& top = f.cell("top");
    db::CellDef& leaf = f.cell("leaf");
    f.paint(leaf, "metal1", db::Rect{0, 0, 2, 2});
    f.label(leaf, "pin", db::Rect{1, 1, 1, 1}, "metal1");
    f.place(top, leaf);
    f.place(top, leaf, db::Transform{1, 0, 10, 0, 1, 0});
    const std::vector<std::string> b = body(f.write(top, true));
    INFO(joined(b));
    CHECK(std::count_if(b.begin(), b.end(), [](const std::string& l) { return l.starts_with("DS "); }) == 1);
    CHECK(b[0] == "DS 1 10 2;");
    CHECK(b[1] == (on ? "9 __CIF2__;" : "9 top;"));
    CHECK(std::none_of(b.begin(), b.end(), [](const std::string& l) { return l.starts_with("C 2"); }));
    CHECK(contains(b, "    B 4 4 2 2;"));
    CHECK(contains(b, "    B 4 4 22 2;"));
    // With magic-compat OFF the labels `flatten` copies, named by their use path.
    CHECK(contains(b, "94 leaf_0/pin 2 2 CMF;") == !on);
    CHECK(contains(b, "94 leaf_1/pin 22 2 CMF;") == !on);
    CHECK(b[b.size() - 2] == "C 1;");
    // The scratch cell is not in the library.
    CHECK(f.ed.cells.find("__CIF2__") == nullptr);
  }
}

TEST_CASE("a style with reducer 0 is refused and leaves an empty file",
          "[cap:cif-format-io][req:CIF output preconditions and flat output]") {
  for (const bool flat : {false, true}) {
    Fixture f;
    mask::OutputStyle style = *f.ed.mask.output_styles.current_style();
    style.scale.reducer = 0;
    auto shared = std::make_shared<const mask::OutputStyle>(style);
    f.ed.mask.output_styles.styles = {shared};
    f.ed.mask.output_styles.base = {shared};
    f.ed.mask.output_styles.current = 0;
    db::CellDef& top = f.cell("top");
    REQUIRE(f.fs.write_atomic("/out.cif", "old").ok());
    CHECK(f.write(top, flat).empty());
    CHECK(f.fs.exists("/out.cif"));
    CHECK_FALSE(f.last.ok());
    CHECK(f.has_code("CIF-WRITE-003"));
    CHECK(f.texts().back() == "I/O error in writing file out.\nFile may be incompletely written.");
  }
}

TEST_CASE("without an output style nothing is written", "[cap:cif-format-io][req:CIF output preconditions and flat output]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.ed.mask.output_styles = mask::OutputStyleSet{};
  CHECK(f.write(top).empty());
  CHECK_FALSE(f.fs.exists("/out.cif"));
  CHECK(f.has_code("CIF-WRITE-001"));
}

TEST_CASE("a subtree that cannot be read fails the write with an empty file",
          "[cap:cif-format-io][req:CIF output symbol numbering and order]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  db::CellDef& missing = f.cell("missing");
  f.place(top, missing);
  missing.flags().available = false;  // no loader can read it
  CHECK(f.write(top).empty());
  CHECK(f.fs.exists("/out.cif"));
  CHECK(f.has_code("CIF-WRITE-002"));
  CHECK(f.texts().front() == "Failure to read in entire subtree of the cell.\nFailed on cell missing.");
  // cif flat reads what it can and goes on.
  CHECK_FALSE(f.write(top, true).empty());
  CHECK(f.last.ok());
}

TEST_CASE("the loader reads unavailable cells before the write",
          "[cap:cif-format-io][req:CIF output symbol numbering and order]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  db::CellDef& leaf = f.cell("leaf");
  f.place(top, leaf);
  leaf.flags().available = false;
  cif::CifWriteOptions o = f.options();
  int reads = 0;
  int fixes = 0;
  o.loader.read = [&](db::CellDef& def) {
    ++reads;
    def.flags().available = true;
    return true;
  };
  o.loader.fix_mismatches = [&] { ++fixes; };
  CHECK(contains(body(f.write_with(top, o)), "9 leaf;"));
  CHECK(reads == 1);
  CHECK(fixes == 1);
}

TEST_CASE("a cancelled write leaves no file", "[cap:cif-format-io][req:CIF output preconditions and flat output]") {
  Fixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, "metal1", db::Rect{0, 0, 4, 2});
  std::atomic<bool> cancel{true};
  mask::Progress progress;
  progress.cancel = &cancel;
  REQUIRE(f.fs.write_atomic("/out.cif", "previous").ok());
  f.last = cif::write(f.ed, top, "/out.cif", f.options(), &progress);
  CHECK_FALSE(f.last.ok());
  CHECK(f.has_code("CIF-WRITE-007"));
  CHECK(f.file("/out.cif") == "previous");
}

TEST_CASE("the writer never changes the layout", "[cap:cif-format-io][req:CIF output symbol numbering and order]") {
  for (const bool flat : {false, true}) {
    Fixture f;
    db::CellDef& top = f.cell("top");
    db::CellDef& leaf = f.cell("leaf");
    f.paint(leaf, "metal1", db::Rect{0, 0, 2, 2});
    f.place(top, leaf);
    top.flags().modified = false;
    leaf.flags().modified = false;
    const std::size_t cells = f.ed.cells.size();
    REQUIRE(f.write(top, flat).size() > 0);
    CHECK(f.ed.cells.size() == cells);
    CHECK_FALSE(top.flags().modified);
    CHECK_FALSE(leaf.flags().modified);
  }
}

TEST_CASE("output file names (Q-MASK-320)", "[cap:cif-format-io][req:cif read, write and flat options]") {
  const compat::CompatSettings on{true};
  const compat::CompatSettings off{false};
  CHECK(cif::output_file_name("chip", on) == "chip.cif");
  CHECK(cif::output_file_name("chip.cif", on) == "chip.cif.cif");
  CHECK(cif::output_file_name("chip", off) == "chip.cif");
  CHECK(cif::output_file_name("chip.cif", off) == "chip.cif");
}
