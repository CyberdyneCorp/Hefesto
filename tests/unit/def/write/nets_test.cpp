// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write: SPECIALNETS and NETS from the `.ext` netlist (capability
// def-write "Net classification", "DEF net naming", "Net record header and
// connections", "Net geometry discovery", "Wire segment derivation",
// "Route record assembly"; tasks.md 8.4; Q-LEFDEF-95, 96, 97, 102, 112).
// The `.ext` files are written by hand in Magic's format.
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/app/settings_layers.hpp"
#include "write_fixture.hpp"

namespace def_write_test {
namespace {

constexpr std::string_view kExtHeader =
    "timestamp 0\nversion 8.3\ntech lefdef\nstyle lefdef\nscale 1000 1000 1\n"
    "resistclasses 1000 500 100 50 30 20\n";

std::string node(std::string_view name, int x, int y, std::string_view layer = "m1") {
  return "node \"" + std::string{name} + "\" 1 6 " + std::to_string(x) + " " + std::to_string(y) + " " +
         std::string{layer} + " 0 0 0 0 0 0 0 0 0 0 0 0\n";
}

std::string port(std::string_view name, int n, const db::Rect& r) {
  return "port \"" + std::string{name} + "\" " + std::to_string(n) + " " + std::to_string(r.x0) + " " +
         std::to_string(r.y0) + " " + std::to_string(r.x1) + " " + std::to_string(r.y1) + " m1\n";
}

/// Writes `top` with `ext` as top.ext and returns the DEF text.
std::string write_with(Session& s, db::CellDef& top, const std::string& ext, def::DefWriteOptions o = {}) {
  s.set_widths();
  s.file("/t/" + top.name() + ".ext", std::string{kExtHeader} + ext);
  const auto result = s.write(top, "", o);
  for (const auto& d : s.messages) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(result.ok());
  return s.read(top.name() + ".def");
}

TEST_CASE("net classification: global, port and labeled nets", "[cap:def-write][req:Net classification]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 190, 200, 200});
  s.paint(top, "metal1", db::Rect{0, 100, 60, 110});
  s.paint(top, "metal1", db::Rect{100, 100, 160, 110});
  s.paint(top, "metal1", db::Rect{0, 50, 60, 60});
  const std::string ext = port("A", 1, db::Rect{0, 100, 60, 110}) + node("vdd!", 0, 190) + node("A", 0, 100) +
                          node("clkint", 100, 100) + node("a_12_n3#", 0, 50);
  const std::string text = write_with(s, top, ext);
  const std::string special = section(text, "SPECIALNETS");
  CHECK(special.starts_with("SPECIALNETS 2 ;\n"));
  CHECK(special.find("   - vdd!\n") != std::string::npos);
  CHECK(special.find("   - clkint\n") != std::string::npos);
  CHECK(section(text, "NETS").starts_with("NETS 1 ;\n   - A ( PIN A )\n"));
  CHECK(text.find("a_12_n3") == std::string::npos);
  CHECK(section(text, "BLOCKAGES")
            .starts_with("BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 500 ) ( 600 600 ) ;\n"));

  const std::string all = write_with(s, top, ext, def::DefWriteOptions{std::nullopt, false, true, {}, nullptr});
  CHECK(section(all, "SPECIALNETS").starts_with("SPECIALNETS 3 ;\n"));
  CHECK(all.find("\nNETS ") == std::string::npos);
}

TEST_CASE("the project's global nets and power names make special nets (Q-LEFDEF-102)",
          "[cap:def-write][req:Net classification]") {
  Session s;
  hefesto::app::install_builtin_defaults(s.editor.settings);
  s.editor.settings.set(hefesto::app::SettingLayer::project, hefesto::app::kGlobalNets, "PWR");
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 60, 10});
  s.paint(top, "metal1", db::Rect{0, 20, 60, 30});
  s.paint(top, "metal1", db::Rect{0, 40, 60, 50});
  const std::string ext = port("PWR", 1, db::Rect{0, 0, 60, 10}) + port("vdd!", 2, db::Rect{0, 20, 60, 30}) +
                          port("Y", 3, db::Rect{0, 40, 60, 50}) + node("PWR", 0, 0) + node("vdd!", 0, 20) +
                          node("Y", 0, 40);
  const std::string text = write_with(s, top, ext);
  CHECK(s.said("Node PWR is defined in the \"globals\" array"));
  CHECK(s.said("Node vdd! matches VDD variable definition!"));
  CHECK(section(text, "SPECIALNETS").starts_with("SPECIALNETS 2 ;\n"));
  CHECK(section(text, "NETS").starts_with("NETS 1 ;\n   - Y ( PIN Y )\n"));
}

TEST_CASE("DEF net names: the last component sanitized", "[cap:def-write][req:DEF net naming]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 60, 3});
  s.paint(top, "metal1", db::Rect{0, 20, 60, 23});
  const std::string text =
      write_with(s, top, node("net-1;%*", 0, 0) + port("q#x!", 1, db::Rect{0, 20, 60, 23}) + node("q#x!", 0, 20));
  CHECK(text.find("   - net_1___\n") != std::string::npos);
  CHECK(text.find("   - qx! ( PIN qx! )\n") != std::string::npos);
}

TEST_CASE("a regular horizontal wire, per mode (Q-LEFDEF-95)", "[cap:def-write][req:Wire segment derivation]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    s.paint(top, "metal1", db::Rect{0, 10, 100, 13});
    const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 10, 100, 13}) + node("A", 0, 10));
    CHECK(section(text, "NETS") == std::string{"NETS 1 ;\n   - A ( PIN A )\n      + ROUTED metal1  "} +
                                       (on ? "( 10 115 ) ( 990 * )" : "( 15 115 ) ( 985 * )") + " ;\nEND NETS\n\n");
  }
}

TEST_CASE("a vertical regular wire", "[cap:def-write][req:Wire segment derivation]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{10, 0, 13, 100});
  const std::string text = write_with(s, top, port("A", 1, db::Rect{10, 0, 13, 100}) + node("A", 10, 0));
  CHECK(section(text, "NETS").find("+ ROUTED metal1  ( 115 10 ) ( * 990 ) ;") != std::string::npos);
}

TEST_CASE("a special wire", "[cap:def-write][req:Wire segment derivation]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 200, 10});
  const std::string text = write_with(s, top, node("vdd!", 0, 0));
  CHECK(section(text, "SPECIALNETS") ==
        "SPECIALNETS 1 ;\n   - vdd!\n      + ROUTED metal1 100  ( 0 50 ) ( 2000 * ) ;\nEND SPECIALNETS\n\n");
}

TEST_CASE("a wide regular wire creates a non-default rule", "[cap:def-write][req:Wire segment derivation]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 100, 8});
  const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 0, 100, 8}) + node("A", 0, 0));
  CHECK(section(text, "NETS").find("+ ROUTED metal1 TAPERRULE metal1_width_80  ( 40 40 ) ( 960 * ) ;") !=
        std::string::npos);
  CHECK(section(text, "NONDEFAULTRULES") ==
        "NONDEFAULTRULES 1 ;\n  - metal1_width_80\n     + LAYER metal1 WIDTH 80 ;\nEND NONDEFAULTRULES\n\n");
}

TEST_CASE("a wire into a via, and an off-centre via", "[cap:def-write][req:Route record assembly]") {
  for (const bool centred : {true, false}) {
    INFO((centred ? "centred" : "off-centre"));
    Session s;
    db::CellDef& top = s.cell("top");
    s.paint(top, "metal1", db::Rect{0, 10, 30, 13});
    const db::Rect via = centred ? db::Rect{30, 9, 36, 14} : db::Rect{30, 10, 36, 16};
    s.paint(top, "via1", via);
    const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 10, 30, 13}) + node("A", 0, 10));
    const std::string expect = centred ? "+ ROUTED metal1  ( 10 115 ) ( 330 * ) via1_60_50 "
                                       : "+ ROUTED metal1  ( 10 115 ) ( 330 * ) ( * 130 ) via1_60_60 ";
    CHECK(section(text, "NETS").find(expect) != std::string::npos);
    CHECK(section(text, "VIAS").find(centred ? "   - via1_60_50\n" : "   - via1_60_60\n") != std::string::npos);
  }
}

TEST_CASE("a net that starts on a via", "[cap:def-write][req:Route record assembly]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "via1", db::Rect{0, 0, 6, 6});
  const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 0, 6, 6}) + node("A", 0, 0));
  CHECK(section(text, "NETS").find("   - A ( PIN A )\n      + ROUTED metal1  ( 30 30 ) via1_60_60  ;") !=
        std::string::npos);
}

TEST_CASE("two disjoint wires of one net", "[cap:def-write][req:Route record assembly]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 10, 100, 13});
  s.paint(top, "metal1", db::Rect{0, 50, 100, 53});
  const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 10, 100, 13}) + node("A", 0, 10) +
                                                  node("A_2", 0, 50) + "merge \"A\" \"A_2\"\n");
  const std::string nets = section(text, "NETS");
  INFO(nets);
  CHECK((nets.find("+ ROUTED metal1  ( 10 115 ) ( 990 * )\n      NEW metal1  ( 10 515 ) ( 990 * ) ;") !=
             std::string::npos ||
         nets.find("+ ROUTED metal1  ( 10 515 ) ( 990 * )\n      NEW metal1  ( 10 115 ) ( 990 * ) ;") !=
             std::string::npos));
}

TEST_CASE("two seeds on one wire, per mode (Q-LEFDEF-112)", "[cap:def-write][req:Net geometry discovery]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    s.paint(top, "metal1", db::Rect{0, 0, 200, 10});
    const std::string text =
        write_with(s, top, node("vdd!", 0, 0) + node("vdd2", 150, 0) + "merge \"vdd!\" \"vdd2\"\n");
    const std::string special = section(text, "SPECIALNETS");
    INFO(special);
    const std::string piece = "( 0 50 ) ( 2000 * )";
    std::size_t count = 0;
    for (std::size_t at = special.find(piece); at != std::string::npos; at = special.find(piece, at + 1)) {
      ++count;
    }
    CHECK(count == (on ? 2U : 1U));
  }
}

TEST_CASE("a net with no paint at its location", "[cap:def-write][req:Net geometry discovery]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{100, 100, 110, 110});
  const std::string text = write_with(s, top, port("A", 1, db::Rect{0, 0, 1, 1}) + node("A", 0, 0));
  CHECK(section(text, "NETS") == "NETS 1 ;\n   - A ( PIN A ) ;\nEND NETS\n\n");
}

TEST_CASE("connections to subcell pins wrap at column 70", "[cap:def-write][req:Net record header and connections]") {
  Session s;
  db::CellDef& leaf = s.cell("leaf", false);
  s.paint(leaf, "metal1", db::Rect{0, 0, 4, 4});
  db::CellDef& top = s.cell("top");
  std::string ext = port("n1", 1, db::Rect{0, 0, 1, 1}) + node("n1", 0, 0);
  std::string uses;
  for (int i = 10; i < 20; ++i) {
    s.place(top, leaf, "u" + std::to_string(i), db::Transform{1, 0, 10 * i, 0, 1, 0});
    uses += "use leaf u" + std::to_string(i) + " 1 0 " + std::to_string(10 * i) + " 0 1 0\n";
    ext += "merge \"n1\" \"u" + std::to_string(i) + "/A\"\n";
  }
  s.file("/t/leaf.ext", std::string{kExtHeader} + port("A", 1, db::Rect{0, 0, 4, 4}) + node("A", 0, 0));
  const std::string text = write_with(s, top, uses + ext);
  const std::string nets = section(text, "NETS");
  INFO(nets);
  for (int i = 10; i < 20; ++i) {
    CHECK(nets.find("( u" + std::to_string(i) + " A )") != std::string::npos);
  }
  // Every line but the record's last stays within 70 columns plus one entry.
  std::size_t start = 0;
  for (std::size_t eol = nets.find('\n'); eol != std::string::npos; eol = nets.find('\n', start)) {
    CHECK(eol - start <= 70 + 11);
    start = eol + 1;
  }
  CHECK(nets.find("\n       ( u") != std::string::npos);
}

}  // namespace
}  // namespace def_write_test
