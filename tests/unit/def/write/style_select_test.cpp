// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-write: the active types of BLOCKAGES come from the session's current
// extraction style (Magic's ExtCurStyle, defWrite.c:816, 2538), so
// `extract style` changes what `def write` reads (openspec/changes/
// m7-extraction tasks.md 4.7, design D20).
//
// Owner: EXTTECH (this file; M7 design D20), on DEFW's fixture.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

// lefdef.tech with a second extract style, `active`, that mentions the
// obstruction type obsm1 (so obsm1 is electrically active there).
std::string two_style_tech() {
  std::string text = data_file("lefdef.tech");
  const std::string head = "extract\n    style lefdef\n";
  const std::size_t start = text.find(head);
  REQUIRE(start != std::string::npos);
  const std::size_t end = text.find("\nend", start);
  REQUIRE(end != std::string::npos);
  std::string body = text.substr(start + head.size(), end + 1 - start - head.size());
  text.insert(end + 1, "    style active\n" + body + "    areacap obsm1 1\n");
  return text;
}

constexpr std::string_view kBlockage =
    "BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 0 ) ( 200 100 ) ;\nEND BLOCKAGES\n\n";

TEST_CASE("def write takes the active types from the selected extraction style",
          "[cap:def-write][req:BLOCKAGES section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on, two_style_tech());
    db::CellDef& top = s.cell("top");
    s.paint(top, "obsm1", db::Rect{0, 0, 20, 10});
    // The tech load's style lefdef: obsm1 is not electrically active.
    REQUIRE(s.write(top, "").ok());
    CHECK(section(s.read("top.def"), "BLOCKAGES") == kBlockage);
    // Style active mentions obsm1: its paint is no blockage.
    REQUIRE(s.run("extract style active").ok());
    REQUIRE(s.write(top, "").ok());
    CHECK(s.read("top.def").find("BLOCKAGES") == std::string::npos);
    // Back to lefdef: magic-compat ON keeps the types an earlier selection
    // mentioned (allExtractTypes accumulates), OFF does not.
    REQUIRE(s.run("extract style lefdef").ok());
    REQUIRE(s.write(top, "").ok());
    if (on) {
      CHECK(s.read("top.def").find("BLOCKAGES") == std::string::npos);
    } else {
      CHECK(section(s.read("top.def"), "BLOCKAGES") == kBlockage);
    }
  }
}

}  // namespace
}  // namespace def_write_test
