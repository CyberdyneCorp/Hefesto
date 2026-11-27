// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the sets G and D come from the session's current extraction
// style (Magic's ExtCurStyle, lefWrite.c:1268), so `extract style` changes
// what `lef write` reads (openspec/changes/m7-extraction tasks.md 4.7,
// design D20).
//
// Owner: EXTTECH (this file; M7 design D20), on LEFW's fixture.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

// write.tech with a second extract style that declares no device and no
// tie-down type.
std::string two_style_tech() {
  std::string text = tech_text(kLefLines);
  const std::string last = "    substrate pwell well\nend\n";
  const std::size_t at = text.find(last);
  REQUIRE(at != std::string::npos);
  text.replace(at, last.size(),
               "    substrate pwell well\n    style other\n    substrate pwell well\n    areacap nfet,ndiff 1\nend\n");
  return text;
}

TEST_CASE("lef write takes G and D from the selected extraction style",
          "[cap:lef-write][req:Macro source geometry and layer masks]") {
  Session s;
  s.file("/t/two.tech", two_style_tech());
  REQUIRE(s.run("tech load /t/two.tech -noprompt").ok());
  // metal1 pin A over pc onto poly, which runs into a 5 x 6 gate.
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  s.paint(c, "pc", db::Rect{0, 0, 4, 4});
  s.paint(c, "poly", db::Rect{4, 0, 20, 4});
  s.paint(c, "nfet", db::Rect{20, 0, 25, 6});
  s.label(c, Port{"A", "metal1", db::Rect{0, 0, 10, 10}, 0, db::PortClass::input});
  const std::string gate = "    DIRECTION INPUT ;\n    ANTENNAGATEAREA 0.003000 ;\n    PORT\n";

  // The tech load's style lefw: nfet is a MOSFET gate.
  CHECK(s.macro(c).find(gate) != std::string::npos);
  // Style other has no device line: G is empty.
  REQUIRE(s.run("extract style other").ok());
  const std::string other = s.macro(c);
  CHECK(other.find("ANTENNAGATEAREA") == std::string::npos);
  CHECK(other.find("ANTENNADIFFAREA") == std::string::npos);
  REQUIRE(s.run("extract style lefw").ok());
  CHECK(s.macro(c).find(gate) != std::string::npos);
}

}  // namespace
}  // namespace hefesto::lef::write_test
