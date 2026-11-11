// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// wiring-tool "wiring technology section": the reader of the raw `wiring`
// section (seven- and nine-field contacts, scalefactor, line errors, grid
// scaling) on the test technology, Magic's scmos.tech and the pinned
// sky130A.tech, and the DRC default width the wire commands use.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/wiring/diag_codes.hpp"
#include "hefesto/wiring/wiring.hpp"
#include "reference_data.hpp"
#include "wire_fixture.hpp"

namespace wiring = hefesto::wiring;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;
using wiring::test::type_of;

#define SECTION_REQ "[cap:wiring-tool][req:wiring technology section]"

namespace {

tech::RawLine line(std::vector<std::string> tokens, std::uint32_t number = 7) {
  return tech::RawLine{std::move(tokens), "/t/wire.tech", number};
}

struct Parsed {
  wiring::WiringSection section;
  std::vector<diag::Diagnostic> diagnostics;
};

Parsed parse(const tech::Technology& t, std::vector<tech::RawLine> lines, tech::GridScale grid = {}) {
  diag::CollectingSink sink;
  Parsed p;
  p.section = wiring::parse_wiring_lines(lines, t, grid, &sink);
  p.diagnostics = sink.take();
  return p;
}

std::vector<std::string> texts(const Parsed& p) {
  std::vector<std::string> out;
  for (const auto& d : p.diagnostics) {
    out.push_back(d.text);
  }
  return out;
}

}  // namespace

TEST_CASE("a seven-field contact line defines the contact", SECTION_REQ) {
  const auto t = wiring::test::wire_tech();
  const Parsed p = parse(*t, {line({"contact", "m2contact", "4", "metal1", "0", "metal2", "0"})});
  CHECK(p.diagnostics.empty());
  REQUIRE(p.section.contacts.size() == 1);
  const wiring::WiringContact expected{type_of(*t, "m2contact"), 4, type_of(*t, "metal1"), 0, 0,
                                       type_of(*t, "metal2"),    0, 0};
  CHECK(p.section.contacts[0] == expected);
  CHECK(p.section.scale_factor == 1);
}

TEST_CASE("the wiring section of a technology is read in reverse file order", SECTION_REQ) {
  const auto t = wiring::test::wire_tech();
  const auto s = wiring::parse_wiring_section(*t);
  REQUIRE(s.ok());
  const auto& c = s.value().contacts;
  REQUIRE(c.size() == 3);
  // contact m3c 2 metal2 1 1 metal3 1 2 (nine fields), the last line, first.
  CHECK(c[0] == wiring::WiringContact{type_of(*t, "m3c"), 2, type_of(*t, "metal2"), 1, 1, type_of(*t, "metal3"), 1, 2});
  CHECK(c[1].type == type_of(*t, "m2c"));
  CHECK(c[2] == wiring::WiringContact{type_of(*t, "pc"), 2, type_of(*t, "poly"), 1, 0, type_of(*t, "metal1"), 1, 0});
  // A technology without the section has an empty table.
  const auto none = wiring::parse_wiring_section(*tech::builtin_technology());
  REQUIRE(none.ok());
  CHECK(none.value().contacts.empty());
}

TEST_CASE("wiring section line errors are reported and the line ignored", SECTION_REQ) {
  const auto t = wiring::test::wire_tech();
  const Parsed p = parse(*t, {
                                 line({"scalefactor"}),
                                 line({"scalefactor", "x"}),
                                 line({"bogus", "1"}),
                                 line({"contact", "m2c", "4"}),
                                 line({"contact", "nope", "4", "metal1", "0", "metal2", "0"}),
                                 line({"contact", "m2c", "4", "met", "0", "zz", "0"}),
                                 line({"contact", "m2c", "x", "metal1", "0", "metal2", "0"}),
                                 line({"contact", "m2c", "4", "metal1", "1.5", "metal2", "0"}),
                                 line({"contact", "m2c", "4", "metal1", "0", "metal2", "y"}),
                                 line({"contact", "m2c", "4", "metal1", "0", "e", "metal2", "0", "1"}),
                                 line({"contact", "m2c", "4", "metal1", "0", "1", "metal2", "0", "-"}),
                             });
  CHECK(p.section.contacts.size() == 1);  // only the last line: "-" passes StrIsInt as in Magic
  CHECK(p.section.contacts[0].extend2 == 0);
  CHECK(texts(p) == std::vector<std::string>{
                        "\"scalefactor\" line must have exactly 2 arguments.",
                        "\"scalefactor\" argument must be an integer.",
                        "Unknown wiring keyword: bogus.  Line ignored.",
                        "\"contact\" lines must have exactly 7 or 9 arguments.",
                        "Unrecognized layer (type) name \"nope\"",
                        "Ambiguous layer (type) name \"met\"",
                        "Unrecognized layer (type) name \"zz\"",
                        "Contact size must be an integer.",
                        "Contact surround distance must be an integer.",
                        "Contact surround distance must be an integer.",
                        "Contact extend distance must be an integer.",
                    });
  for (const auto& d : p.diagnostics) {
    CHECK(d.code == wiring::codes::kTechLine);
    CHECK(d.severity == diag::Severity::warning);
    CHECK(d.location == diag::Location{diag::SourceLocation{"/t/wire.tech", 7}});
  }
}

TEST_CASE("scalefactor divides contact dimensions rounding up; the grid scales them", SECTION_REQ) {
  const auto t = wiring::test::wire_tech();
  Parsed p = parse(*t, {line({"scalefactor", "10"}), line({"contact", "m2c", "170", "metal1", "25", "metal2", "0"}),
                        line({"scalefactor", "5"})});
  CHECK(p.diagnostics.empty());
  CHECK(p.section.scale_factor == 5);  // the last line wins (WireUnits is one global)
  p.section.scale_factor = 10;
  const wiring::WiringContact c = p.section.scaled(p.section.contacts[0]);
  CHECK(c.size == 17);
  CHECK(c.surround1 == 3);
  CHECK(c.surround2 == 0);
  // Grid scaling multiplies, then truncates.
  const Parsed doubled = parse(*t, {line({"contact", "m3c", "5", "metal2", "1", "3", "metal3", "0", "7"})}, {2, 1});
  CHECK(doubled.section.contacts[0] == wiring::WiringContact{type_of(*t, "m3c"), 10, type_of(*t, "metal2"), 2, 6,
                                                             type_of(*t, "metal3"), 0, 14});
  const Parsed halved = parse(*t, {line({"contact", "m3c", "5", "metal2", "1", "3", "metal3", "0", "7"})}, {1, 2});
  CHECK(halved.section.contacts[0] == wiring::WiringContact{type_of(*t, "m3c"), 2, type_of(*t, "metal2"), 0, 1,
                                                            type_of(*t, "metal3"), 0, 3});
}

TEST_CASE("Magic's scmos.tech wiring section", SECTION_REQ) {
  const std::filesystem::path file{HEFESTO_SCMOS_TECH};
  if (!std::filesystem::exists(file)) {
    SKIP("scmos.tech was not generated (needs build/ref/magic)");
  }
  hefesto::platform::LocalFileSystem fs;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(fs, file.string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const auto& t = *loaded.value();
  diag::CollectingSink sink;
  const auto s = wiring::parse_wiring_section(t, {}, &sink);
  REQUIRE(s.ok());
  CHECK(sink.take().empty());
  const auto& c = s.value().contacts;
  REQUIRE(c.size() == 6);
  // contact m3contact 5 metal2 0 metal3 0 is the last line, so first.
  CHECK(c[0] == wiring::WiringContact{type_of(t, "m3contact"), 5, type_of(t, "metal2"), 0, 0, type_of(t, "metal3"), 0,
                                      0});
  CHECK(c[1] == wiring::WiringContact{type_of(t, "m2contact"), 4, type_of(t, "metal1"), 0, 0, type_of(t, "metal2"), 0,
                                      0});
  CHECK(c[5].type == type_of(t, "pdcontact"));
}

TEST_CASE("the pinned sky130A wiring section", SECTION_REQ) {
  const auto file = hefesto::test::pinned_sky130a_tech();
  if (!file) {
    SKIP("sky130A.tech not in .cache/pdks");
  }
  hefesto::platform::LocalFileSystem fs;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology(fs, file->string(), tech::LoadOptions{}, load_sink);
  REQUIRE(loaded.ok());
  const auto& t = *loaded.value();
  diag::CollectingSink sink;
  const auto s = wiring::parse_wiring_section(t, {}, &sink);
  REQUIRE(s.ok());
  for (const auto& d : sink.take()) {
    UNSCOPED_INFO(d.text);
    CHECK(d.code.empty());
  }
  const auto& w = s.value();
  CHECK(w.scale_factor == 10);
  // 11 contact lines since open_pdks c6df28c1 added `contact xpc 170 xpolyres 50 80 li 0 80` (M4 pins).
  REQUIRE(w.contacts.size() == 11);
  // contact v1 260 m1 0 30 m2 0 30, scaled by 10 rounding up.
  const auto v1 = std::find_if(w.contacts.begin(), w.contacts.end(),
                               [&](const wiring::WiringContact& c) { return c.type == type_of(t, "v1"); });
  REQUIRE(v1 != w.contacts.end());
  CHECK(w.scaled(*v1) ==
        wiring::WiringContact{type_of(t, "v1"), 26, type_of(t, "m1"), 0, 3, type_of(t, "m2"), 0, 3});
  // contact v4 1180 m4 0 m5 120 (seven fields).
  const auto v4 = std::find_if(w.contacts.begin(), w.contacts.end(),
                               [&](const wiring::WiringContact& c) { return c.type == type_of(t, "v4"); });
  REQUIRE(v4 != w.contacts.end());
  CHECK(*v4 == wiring::WiringContact{type_of(t, "v4"), 1180, type_of(t, "m4"), 0, 0, type_of(t, "m5"), 120, 0});
  CHECK(w.contacts.back().type == type_of(t, "mcon"));  // the first line, last
}

TEST_CASE("the DRC default width of a layer", "[cap:wiring-tool][req:Wire type selection]") {
  const auto t = wiring::test::wire_tech();
  CHECK(wiring::default_layer_width(*t, type_of(*t, "poly")) == 2);
  CHECK(wiring::default_layer_width(*t, type_of(*t, "metal1")) == 3);
  CHECK(wiring::default_layer_width(*t, type_of(*t, "metal2")) == 4);  // the largest width rule
  CHECK(wiring::default_layer_width(*t, type_of(*t, "metal3")) == 6);
  CHECK(wiring::default_layer_width(*t, type_of(*t, "ndiff")) == 0);
  CHECK(wiring::default_layer_width(*t, type_of(*t, "metal3"), {2, 1}) == 12);
}

TEST_CASE("loading a technology reports its wiring line errors", SECTION_REQ) {
  // The technology reader keeps the section raw; the session load reads it so
  // the WIRE-TECH-001 lines appear at `tech load` time, as in Magic.
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  std::string text = wiring::test::read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "wire.tech");
  const std::string good = "    contact pc 2 poly 1 metal1 1\n";
  const auto at = text.find(good);
  REQUIRE(at != std::string::npos);
  text.insert(at, "    contact nosuch 2 poly 1 metal1 1\n");
  REQUIRE(services.files().write_atomic("/t/wire.tech", text).ok());
  hefesto::editor::EditorState editor;
  diag::CollectingSink sink;
  hefesto::platform::PlatformServices view = services.view();
  REQUIRE(tech::load_session_technology(editor, view, "/t/wire.tech", {}, sink).ok());
  bool reported = false;
  for (const auto& d : sink.take()) {
    reported = reported || d.code == wiring::codes::kTechLine;
  }
  CHECK(reported);
}
