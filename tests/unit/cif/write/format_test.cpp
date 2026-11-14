// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The text of each CIF output line (src/core/src/cif/write/format.hpp):
// capability cif-format-io "CIF output header", "CIF output symbol
// definition format", "CIF output labels", "CIF output calls and arrays".
// Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

#include "cif/write/format.hpp"

namespace {

namespace detail = hefesto::cif::detail;
namespace db = hefesto::db;

db::TileView triangle(db::Rect area, db::Diagonal diagonal, bool right_side) {
  db::TileView t;
  t.area = area;
  t.split = true;
  t.diagonal = diagonal;
  t.right_side = right_side;
  return t;
}

std::vector<std::string> calls_of(const db::CellUse& use, std::int64_t number, const detail::OutputNumbers& n,
                                  bool ids) {
  std::vector<std::string> out;
  detail::call_lines(use, number, n, ids, [&out](const std::string& text) {
    out.push_back(text);
    return true;
  });
  return out;
}

}  // namespace

TEST_CASE("the preamble lines, with and without a description",
          "[cap:cif-format-io][req:CIF output header]") {
  detail::Preamble p;
  p.source = "lib/inv";
  p.tool = "Hefesto 0.5.0";
  p.compiled = "Nov 14 2026 05:27:00";
  p.technology = "scmos";
  p.version = "2.0";
  p.style = "lambda=1.0(gen)";
  p.date = "Sat Nov 14 05:33:37 2026";
  const std::string text = detail::preamble_text(p);
  CHECK(text ==
        "( @@user : ? );\n( @@machine : ? );\n( @@source : lib/inv );\n( @@tool : Hefesto 0.5.0 );\n"
        "( @@compiled : Nov 14 2026 05:27:00 );\n( @@technology : scmos );\n( @@version : 2.0 );\n"
        "( @@style : lambda=1.0(gen) );\n( @@date : Sat Nov 14 05:33:37 2026 );\n");
  p.description = "a test";
  CHECK(detail::preamble_text(p).find("( @@version : 2.0 );\n( @@techdesc : a test );\n( @@style") !=
        std::string::npos);
}

TEST_CASE("the date line is ctime() in the given zone", "[cap:cif-format-io][req:CIF output header]") {
  CHECK(detail::ctime_text(1700000000, "UTC0") == "Tue Nov 14 22:13:20 2023");
  CHECK(detail::ctime_text(0, "UTC0") == "Thu Jan  1 00:00:00 1970");
  CHECK(detail::ctime_text(1700000000, "<-03>3") == "Tue Nov 14 19:13:20 2023");
  CHECK(detail::ctime_text(-86400, "UTC0") == "Wed Dec 31 00:00:00 1969");
  CHECK(detail::ctime_text(std::numeric_limits<std::int64_t>::max(), "UTC0") == "?");
}

TEST_CASE("symbol start and name lines", "[cap:cif-format-io][req:CIF output symbol definition format]") {
  CHECK(detail::symbol_start(1, 1, 1) == "DS 1 1 2;\n");
  CHECK(detail::symbol_start(7, 25, 3) == "DS 7 25 6;\n");
  CHECK(detail::name_line("inv", "") == "9 inv;\n");
  CHECK(detail::name_line("inv", "/lib") == "9 /lib/inv;\n");
  CHECK(detail::name_line("(UNNAMED)", "/lib") == "9 UNNAMED;\n");
  CHECK_FALSE(detail::name_line("", "/lib").has_value());
}

TEST_CASE("boxes are doubled and divided by the reducer",
          "[cap:cif-format-io][req:CIF output symbol definition format]") {
  CHECK(detail::box_line(db::Rect{0, 0, 40, 20}, 1) == "    B 80 40 40 20;\n");
  CHECK(detail::box_line(db::Rect{0, 0, 40, 20}, 10) == "    B 8 4 4 2;\n");
  // C division truncates toward zero, also for negative centers.
  CHECK(detail::box_line(db::Rect{-15, -5, 0, 0}, 10) == "    B 3 1 -1 0;\n");
  // 64-bit arithmetic at the edge of the plane.
  const db::Coord big = std::numeric_limits<db::Coord>::max() / 2;
  CHECK(detail::box_line(db::Rect{-big, -big, big, big}, 1) ==
        "    B " + std::to_string(4 * std::int64_t{big}) + " " + std::to_string(4 * std::int64_t{big}) + " 0 0;\n");
}

TEST_CASE("triangles follow GrClipTriangle's vertex order",
          "[cap:cif-format-io][req:CIF output symbol definition format]") {
  const db::Rect r{0, 0, 10, 10};
  // Lower-right half: (xb,yb) (xt,yb) (xt,yt) — the spec's scenario.
  CHECK(detail::triangle_line(triangle(r, db::Diagonal::ne_sw, true), 1) == "    P 0 0 20 0 20 20;\n");
  // Upper-left half: (xt,yt) (xb,yt) (xb,yb).
  CHECK(detail::triangle_line(triangle(r, db::Diagonal::ne_sw, false), 1) == "    P 20 20 0 20 0 0;\n");
  // Lower-left half: (xb,yt) (xb,yb) (xt,yb).
  CHECK(detail::triangle_line(triangle(r, db::Diagonal::nw_se, false), 1) == "    P 0 20 0 0 20 0;\n");
  // Upper-right half: (xt,yb) (xt,yt) (xb,yt).
  CHECK(detail::triangle_line(triangle(r, db::Diagonal::nw_se, true), 1) == "    P 20 0 20 20 0 20;\n");
}

TEST_CASE("cell labels and label-layer tiles", "[cap:cif-format-io][req:CIF output labels]") {
  detail::OutputNumbers n;
  n.scale_factor = 100;
  n.reducer = 1;
  // The spec's point label: (4,1), S = 100, R = 1, layer CMF.
  CHECK(detail::cell_label_line("out", db::Rect{4, 1, 4, 1}, std::string{"CMF"}, n) == "94 out 800 200 CMF;\n");
  CHECK(detail::cell_label_line("out", db::Rect{4, 1, 4, 1}, std::nullopt, n) == "94 out 800 200;\n");
  n.area_labels = true;
  CHECK(detail::cell_label_line("a", db::Rect{0, 0, 2, 3}, std::string{"CMF"}, n) == "95 a 400 600 200 300 CMF;\n");
  CHECK(detail::cell_label_line("a", db::Rect{0, 0, 2, 3}, std::nullopt, n) == "95 a 400 600 200 300;\n");
}

TEST_CASE("label-layer tiles are scaled by S with magic-compat ON and not with it OFF",
          "[cap:cif-format-io][req:CIF output labels]") {
  // Regression: cifWriteLabelFunc multiplies label-layer tiles by S
  // (cif/CIFwrite.c:603-618); the writer used to divide by R only.
  detail::OutputNumbers n;
  n.scale_factor = 100;
  n.reducer = 10;
  n.scale_label_layers = true;
  CHECK(detail::layer_label_line("CPIN", db::Rect{0, 0, 40, 20}, n) == "94 CPIN 400 200;\n");
  n.area_labels = true;
  CHECK(detail::layer_label_line("CPIN", db::Rect{0, 0, 40, 20}, n) == "95 CPIN 800 400 400 200;\n");
  n.scale_label_layers = false;
  CHECK(detail::layer_label_line("CPIN", db::Rect{0, 0, 40, 20}, n) == "95 CPIN 8 4 4 2;\n");
  n.area_labels = false;
  CHECK(detail::layer_label_line("CPIN", db::Rect{0, 0, 40, 20}, n) == "94 CPIN 4 2;\n");
  n.scale_factor = 1;
  n.reducer = 1;
  n.area_labels = true;
  for (const bool scaled : {true, false}) {
    n.scale_label_layers = scaled;
    CHECK(detail::layer_label_line("CPIN", db::Rect{0, 0, 40, 20}, n) == "95 CPIN 80 40 40 20;\n");
  }
  // Extreme values saturate instead of overflowing.
  n.scale_factor = std::numeric_limits<std::int64_t>::max();
  n.scale_label_layers = true;
  CHECK_FALSE(detail::layer_label_line("CPIN", db::Rect{-2000000000, 0, 2000000000, 1}, n).empty());
}

TEST_CASE("label text is verbatim with magic-compat ON and escaped with it OFF",
          "[cap:cif-format-io][req:CIF output labels]") {
  const detail::LabelText on = detail::label_text("out 1;\t", true);
  CHECK(on.text == "out 1;\t");
  CHECK(on.unsafe);
  CHECK_FALSE(on.changed);
  const detail::LabelText off = detail::label_text("out 1;\t\n", false);
  CHECK(off.text == "out_1___");
  CHECK(off.changed);
  const detail::LabelText plain = detail::label_text("vdd!", false);
  CHECK(plain.text == "vdd!");
  CHECK_FALSE(plain.unsafe);
  CHECK_FALSE(plain.changed);
}

TEST_CASE("calls: rotation, mirroring, translation and ids", "[cap:cif-format-io][req:CIF output calls and arrays]") {
  detail::OutputNumbers n;
  n.scale_factor = 100;
  n.reducer = 1;
  db::CellUse use;
  use.id = "u1";
  use.transform = db::Transform{1, 0, 3, 0, 1, 0};
  CHECK(calls_of(use, 2, n, true) == std::vector<std::string>{"91 u1;\nC 2 R 1 0 T 600 0;\n"});
  CHECK(calls_of(use, 2, n, false) == std::vector<std::string>{"C 2 R 1 0 T 600 0;\n"});
  use.id.clear();
  CHECK(calls_of(use, 2, n, true) == std::vector<std::string>{"C 2 R 1 0 T 600 0;\n"});
  // Mirror in x: (-1 0 0; 0 1 0).
  use.transform = db::Transform{-1, 0, 0, 0, 1, 0};
  CHECK(calls_of(use, 2, n, false).front().find(" MX R 1 0 T 0 0") != std::string::npos);
  // Rotation by 90: (0 -1 0; 1 0 0) is R 0 1; (0 1 0; 1 0 0) is mirrored.
  use.transform = db::Transform{0, -1, 0, 1, 0, 0};
  CHECK(calls_of(use, 2, n, false).front() == "C 2 R 0 1 T 0 0;\n");
  use.transform = db::Transform{0, 1, 0, 1, 0, 0};
  CHECK(calls_of(use, 2, n, false).front() == "C 2 MX R 0 -1 T 0 0;\n");
}

TEST_CASE("arrays give one call per element, x outer and y inner",
          "[cap:cif-format-io][req:CIF output calls and arrays]") {
  detail::OutputNumbers n;  // S = 1, R = 1
  db::CellUse use;
  use.id = "a";
  use.array = db::ArrayInfo{0, 0, 0, 2, 0, 5};
  CHECK(calls_of(use, 3, n, true) == std::vector<std::string>{"91 a(0);\nC 3 R 1 0 T 0 0;\n",
                                                               "91 a(1);\nC 3 R 1 0 T 0 10;\n",
                                                               "91 a(2);\nC 3 R 1 0 T 0 20;\n"});
  use.array = db::ArrayInfo{1, 0, 4, 3, 10, 20};  // x descending, y descending: (Y,X)
  const std::vector<std::string> two_d = calls_of(use, 3, n, true);
  REQUIRE(two_d.size() == 4);
  CHECK(two_d[0] == "91 a(4,1);\nC 3 R 1 0 T 0 0;\n");
  CHECK(two_d[1] == "91 a(3,1);\nC 3 R 1 0 T 0 40;\n");
  CHECK(two_d[2] == "91 a(4,0);\nC 3 R 1 0 T 20 0;\n");
  CHECK(two_d[3] == "91 a(3,0);\nC 3 R 1 0 T 20 40;\n");
  use.array = db::ArrayInfo{2, 4, 0, 0, 7, 0};  // only x: (X)
  CHECK(calls_of(use, 3, n, true)[2] == "91 a(4);\nC 3 R 1 0 T 28 0;\n");
}

TEST_CASE("the visitor stops the elements of a huge array", "[cap:cif-format-io][req:CIF output calls and arrays]") {
  detail::OutputNumbers n;
  db::CellUse use;
  use.array = db::ArrayInfo{0, std::numeric_limits<std::int32_t>::max(), 0, std::numeric_limits<std::int32_t>::max(),
                            std::numeric_limits<db::Coord>::max(), std::numeric_limits<db::Coord>::max()};
  std::size_t seen = 0;
  std::string last;
  detail::call_lines(use, 2, n, false, [&](const std::string& text) {
    last = text;
    return ++seen < 3;
  });
  CHECK(seen == 3);
  CHECK(last == "C 2 R 1 0 T 0 " + std::to_string(4 * std::int64_t{std::numeric_limits<db::Coord>::max()}) + ";\n");
}
