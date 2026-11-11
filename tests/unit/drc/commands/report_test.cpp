// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `drc count`, `drc find`, `drc why`, `drc ignore`, why-text substitution
// and the list-mode results (drc-commands; openspec/changes/m3-drc
// tasks.md 7.3, 7.5). The recheck under the box is stood in for by
// drc::ScopedRecheck / WhyOptions::recheck (drc_fixture.hpp).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <vector>

#include "drc_fixture.hpp"
#include "hefesto/drc/commands/reporting.hpp"

namespace drc = hefesto::drc;
namespace db = hefesto::db;
namespace cmd = hefesto::cmd;
using drc::test::DrcFixture;
using drc::test::FakeRecheck;

namespace {

/// `top` with m1 over [0,40]^2, shown in a window with the box over it.
db::CellDef* shown_top(DrcFixture& f) {
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  f.show(*top);
  f.box(*top, db::Rect{0, 0, 40, 40});
  return top;
}

/// `n` separate error tiles in a row along y = 0..2.
void error_row(db::CellDef& def, int n) {
  for (int i = 0; i < n; ++i) {
    DrcFixture::error(def, db::Rect{10 * i, 0, 10 * i + 2, 2});
  }
}

/// A fake reporting `why` at every error tile of `def`.
FakeRecheck errors_at_tiles(const db::CellDef& def, const std::vector<drc::WhyIndex>& whys) {
  FakeRecheck fake;
  const auto tiles = drc::error_tiles(def, true);
  for (std::size_t i = 0; i < tiles.size() && i < whys.size(); ++i) {
    fake.errors.push_back({&def, tiles[i], DrcFixture::rule(whys[i])});
  }
  return fake;
}

}  // namespace

// --- drc count ---------------------------------------------------------------------------

TEST_CASE("Count in scripting mode", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  db::CellDef* top = shown_top(f);
  error_row(*top, 3);
  CHECK(f.value("drc list count") == "{top 3}");
  CHECK(f.value("drc list count total") == "3");
  CHECK(f.out("drc count total") == "Total DRC errors found: 3\n");
}

TEST_CASE("Single error message", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  db::CellDef* inv = f.cell("inv");
  f.paint(*inv, db::Rect{0, 0, 40, 40});
  f.show(*inv);
  error_row(*inv, 1);
  CHECK(f.out("drc count").starts_with("Cell inv has just one error tile.\n"));
}

TEST_CASE("Count total line with magic-compat ON", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  f.compat(true);
  db::CellDef* top = shown_top(f);
  error_row(*top, 3);
  CHECK(f.out("drc count") == "Cell top has 3 error tiles.\nTotal DRC errors found: 0\n");
  f.run("drc off");  // nothing follows the per-cell lines
  CHECK(f.out("drc count") == "Cell top has 3 error tiles.\n");
}

TEST_CASE("Count total line with magic-compat OFF", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  f.compat(false);
  db::CellDef* top = shown_top(f);
  error_row(*top, 3);
  CHECK(f.out("drc count") == "Cell top has 3 error tiles.\nTotal DRC errors found: 3\n");
  f.run("drc off");
  CHECK(f.out("drc count") == "Cell top has 3 error tiles.\n");
}

TEST_CASE("drc listall count descends into loaded cells, each once, by name", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  db::CellDef* leaf = f.cell("leaf");
  f.paint(*leaf, db::Rect{0, 0, 10, 10});
  error_row(*leaf, 1);
  db::CellDef* mid = f.cell("amid");
  f.paint(*mid, db::Rect{0, 0, 30, 30});
  DrcFixture::error(*mid, db::Rect{20, 20, 22, 22});
  DrcFixture::error(*mid, db::Rect{25, 25, 27, 27});
  DrcFixture::place(*mid, *leaf, 0, 0);
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 200, 200});
  error_row(*top, 2);
  DrcFixture::place(*top, *mid, 100, 100);
  DrcFixture::place(*top, *leaf, 50, 50);  // reached twice: counted once
  f.show(*top);

  CHECK(f.value("drc listall count") == "{amid 2} {leaf 1} {top 2}");
  CHECK(f.value("drc listall count total") == "5");
  CHECK(f.value("drc list count") == "{top 2}");  // without listall only the root
  CHECK(f.out("drc listall count") == "");        // list mode prints nothing

  // An unloaded cell is counted but not descended into.
  mid->flags().available = false;
  CHECK(f.value("drc listall count") == "{amid 2} {leaf 1} {top 2}");  // leaf still reached from top
  db::CellDef* other = f.cell("other");
  f.paint(*other, db::Rect{0, 0, 10, 10});
  f.show(*other);
  CHECK(f.value("drc listall count").empty());
}

TEST_CASE("count_errors skips a root that is not loaded", "[cap:drc-commands][req:drc count]") {
  DrcFixture f;
  db::CellDef* top = f.cell("top");
  f.paint(*top, db::Rect{0, 0, 40, 40});
  error_row(*top, 2);
  CHECK(drc::count_errors(*top, top->bbox(), true, true).size() == 1);
  top->flags().available = false;
  CHECK(drc::count_errors(*top, top->bbox(), true, true).empty());
}

TEST_CASE("count and find reach error tiles beyond the paint in both modes",
          "[cap:drc-commands][req:drc count][req:drc find]") {
  // Regression: with magic-compat OFF the bounding box covers paint only
  // (Q-MODEL-19), so an error area outside the paint (a width error, for
  // instance) was neither counted nor found. ON, the box covers it as in Magic.
  const bool compat = GENERATE(true, false);
  INFO("magic-compat " << compat);
  DrcFixture f;
  f.compat(compat);
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  DrcFixture::error(*top, db::Rect{50, 0, 52, 2});
  db::recompute_bbox(*top, compat);
  CHECK(top->bbox().x1 == (compat ? 52 : 40));
  CHECK(drc::error_tiles(*top, compat) == std::vector<db::Rect>{{50, 0, 52, 2}});
  CHECK(f.value("drc list count total") == "1");
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{50, 0, 52, 2}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc find 1") == "Error area #1:\nw\n");
  CHECK(f.editor.box == db::Rect{50, 0, 52, 2});
}

// --- drc find -------------------------------------------------------------------------------

TEST_CASE("Explicit index", "[cap:drc-commands][req:drc find]") {
  DrcFixture f;
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  error_row(*top, 4);
  FakeRecheck fake = errors_at_tiles(*top, {6, 6, 6, 6});
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc find 3") == "Error area #3:\nw\n");
  CHECK(f.editor.box == drc::error_tiles(*top, true)[2]);
  CHECK(f.out("drc find") == "Error area #4:\nw\n");  // the next one
  CHECK(f.out("drc find") == "Error area #1:\nw\n");  // wraps
  CHECK(f.out("drc find 0") == "Error area #1:\nw\n");  // max(n, 1)
}

TEST_CASE("Index out of range", "[cap:drc-commands][req:drc find]") {
  DrcFixture f;
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  error_row(*top, 2);
  FakeRecheck fake = errors_at_tiles(*top, {6, 6});
  const drc::ScopedRecheck scoped(fake.fn());
  const db::Rect before = *f.editor.box;
  const auto r = f.run("drc find 5");
  CHECK(DrcFixture::texts(r) == "There aren't that many errors\n");
  CHECK(r.diagnostics.back().no_newline);  // TxPrintf without a line end
  // The message, printed through the Tcl console, replaces the -1 result
  // (golden DRC-FIND-1); the list form keeps it.
  CHECK(r.value.is_none());
  CHECK(f.editor.box == before);
  CHECK(f.value("drc list find 5") == "-1");
  CHECK(f.out("drc list find 5").empty());
}

TEST_CASE("First find with magic-compat ON", "[cap:drc-commands][req:drc find]") {
  DrcFixture f;
  f.compat(true);
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  error_row(*top, 4);
  FakeRecheck fake = errors_at_tiles(*top, {6, 6, 6, 6});
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc find") == "Error area #2:\nw\n");
  CHECK(f.editor.box == drc::error_tiles(*top, true)[1]);
}

TEST_CASE("First find with magic-compat OFF", "[cap:drc-commands][req:drc find]") {
  DrcFixture f;
  f.compat(false);
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  error_row(*top, 4);
  FakeRecheck fake = errors_at_tiles(*top, {6, 6, 6, 6});
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc find") == "Error area #1:\nw\n");
  CHECK(f.editor.box == drc::error_tiles(*top, true)[0]);
}

TEST_CASE("drc find without errors and in list mode", "[cap:drc-commands][req:drc find]") {
  DrcFixture f;
  f.whys({"w", "v"});
  db::CellDef* top = shown_top(f);
  const auto none = f.run("drc find");
  CHECK(DrcFixture::texts(none) == "There are no errors in top.\n");
  CHECK(none.value.is_none());  // the printed message replaces the 0 result
  CHECK(f.value("drc list find") == "0");

  error_row(*top, 1);
  FakeRecheck fake;
  fake.errors = {{top, drc::error_tiles(*top, true)[0], DrcFixture::rule(6)},
                 {top, drc::error_tiles(*top, true)[0], DrcFixture::rule(7)}};
  const drc::ScopedRecheck scoped(fake.fn());
  const auto listed = f.run("drc list find");
  CHECK(DrcFixture::texts(listed).empty());  // no `Error area` line in list mode
  CHECK(listed.value == cmd::Value{cmd::Value::List{"w", "v"}});
}

TEST_CASE("drc find text skips areas without a matching why and wraps (both modes)", "[cap:drc-commands][req:drc find]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    DrcFixture f;
    f.compat(compat);
    f.whys({"m1 spacing", "m2 width", "m3 spacing"});  // 6, 7, 8 (the last)
    db::CellDef* top = shown_top(f);
    error_row(*top, 3);
    FakeRecheck fake = errors_at_tiles(*top, {6, 7, 7});
    const drc::ScopedRecheck scoped(fake.fn());
    f.editor.drc.find_counter = 0;
    CHECK(f.out("drc find spacing") == "Error area #1:\nm1 spacing\n");
    CHECK(f.out("drc find spacing") == "Error area #2:\nError area #3:\nError area #1:\nm1 spacing\n");
    // No area matches: the search stops silently where it began.
    CHECK(f.out("drc find nomatch") == "Error area #2:\nError area #3:\nError area #1:\n");
    CHECK(f.run("drc find nomatch").value.is_none());
  }
}

TEST_CASE("drc find text cannot match the last why text with magic-compat ON (Q-DRC-17)", "[cap:drc-commands][req:drc find]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    DrcFixture f;
    f.compat(compat);
    f.whys({"m1 width", "m3 spacing"});  // 6, 7 (the last)
    db::CellDef* top = shown_top(f);
    error_row(*top, 1);
    FakeRecheck fake = errors_at_tiles(*top, {7});
    const drc::ScopedRecheck scoped(fake.fn());
    f.editor.drc.find_counter = 0;
    CHECK(f.out("drc find spacing") == (compat ? "Error area #1:\n" : "Error area #1:\nm3 spacing\n"));
  }
}

// --- drc why ----------------------------------------------------------------------------------

TEST_CASE("Why under the box", "[cap:drc-commands][req:drc why]") {
  DrcFixture f;
  f.whys({"Poly width < 0.15um"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 10}, DrcFixture::rule(6)}, {top, db::Rect{5, 0, 6, 10}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc why") == "Poly width < 0.15um\n");
  REQUIRE(fake.calls.size() == 1);
  CHECK(fake.calls[0] == std::make_pair(static_cast<const db::CellDef*>(top), db::Rect{0, 0, 40, 40}));
}

TEST_CASE("Touching box does not count", "[cap:drc-commands][req:drc why]") {
  DrcFixture f;
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  f.box(*top, db::Rect{12, 0, 20, 10});
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{10, 0, 12, 10}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc why") == "No errors found.\n");
  CHECK(f.out("drc listall why") == "No errors found.\n");
  CHECK(f.value("drc listall why").empty());
}

TEST_CASE("drc why lists distinct texts in discovery order", "[cap:drc-commands][req:drc why]") {
  DrcFixture f;
  f.whys({"a", "b"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 1}, DrcFixture::rule(7)},
                 {top, db::Rect{2, 0, 3, 1}, DrcFixture::rule(6)},
                 {top, db::Rect{4, 0, 5, 1}, DrcFixture::rule(7)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc why") == "b\na\n");
  CHECK(f.value("drc list why") == "b a");
  CHECK(f.out("drc list why").empty());
  CHECK(f.value("drc listall why") == "b {{0 0 1 1} {4 0 5 1}} a {{2 0 3 1}}");
}

TEST_CASE("drc why resolves errors copied from subcells", "[cap:drc-commands][req:drc why]") {
  DrcFixture f;
  f.whys({"child w"});
  db::CellDef* child = f.cell("child");
  f.paint(*child, db::Rect{0, 0, 10, 10});
  db::CellDef* top = shown_top(f);
  DrcFixture::place(*top, *child, 100, 0);
  f.box(*top, db::Rect{99, -1, 103, 3});
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{100, 0, 102, 2}, DrcFixture::rule(drc::why::kInSubcell)},
                 {child, db::Rect{0, 0, 2, 2}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc why") == "child w\n");
  // The subcell was rechecked over the error rectangle in its coordinates.
  CHECK(fake.calls.back() == std::make_pair(static_cast<const db::CellDef*>(child), db::Rect{0, 0, 2, 2}));
  // listall reports the subcell error in root coordinates.
  CHECK(f.value("drc listall why") == "{child w} {{100 0 102 2}}");

  // Nothing found below: the why-4 text itself.
  fake.errors.pop_back();
  CHECK(f.out("drc why") == "See error definition in the subcell\n");
}

TEST_CASE("why_errors skips ignored rules except in listall mode", "[cap:drc-commands][req:drc why]") {
  DrcFixture f;
  f.whys({"a", "b"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 1}, DrcFixture::rule(6)}, {top, db::Rect{2, 0, 3, 1}, DrcFixture::rule(7)}};
  f.editor.drc.ignore.add(6);
  drc::WhyOptions options;
  options.recheck = fake.fn();
  const auto groups = drc::why_errors(f.editor, *top, db::Rect{0, 0, 40, 40}, options);
  REQUIRE(groups.size() == 1);
  CHECK(groups[0] == drc::WhyGroup{"b", 7, {db::Rect{2, 0, 3, 1}}});
  options.all = true;
  CHECK(drc::why_errors(f.editor, *top, db::Rect{0, 0, 40, 40}, options).size() == 2);
}

// --- Why-text substitution ------------------------------------------------------------------

namespace {

/// 0.005 um per internal unit: scalefactor 1, expander 2.
cmd::CoordinateUnits half_centimicron() {
  cmd::CoordinateUnits u;
  u.cif_scale = std::array<std::int64_t, 2>{1, 2};
  return u;
}

std::string substitute(std::string_view why, const drc::Rule& base, const cmd::UnitsState& units = {},
                       const cmd::CoordinateUnits& scale = half_centimicron()) {
  drc::RuleSetBuilder b;
  drc::Rule rule = base;
  rule.why = b.why(why);
  return drc::why_text(*b.build(), rule, units, scale);
}

}  // namespace

TEST_CASE("Micron substitution", "[cap:drc-commands][req:Why-text substitution]") {
  CHECK(substitute("poly width < %d", DrcFixture::rule(0, 30)) == "poly width < 0.15um");
  CHECK(substitute("corner %c", DrcFixture::rule(0, 30, 25)) == "corner 0.125um");
  CHECK(substitute("area %a", DrcFixture::rule(0, 0, 25)) == "area 0.000625um^2");
  CHECK(substitute("no escapes", DrcFixture::rule(0, 30)) == "no escapes");
}

TEST_CASE("Unknown escape copied literally", "[cap:drc-commands][req:Why-text substitution]") {
  CHECK(substitute("m1 %d wide, 100%x done %", DrcFixture::rule(0, 30)) == "m1 0.15um wide, 100%x done %");
  CHECK(substitute("%%d and %d%", DrcFixture::rule(0, 30)) == "%%d and 0.15um%");
  // Q-DRC-16 is fixed in both magic-compat settings.
  for (const bool compat : {true, false}) {
    DrcFixture f;
    f.compat(compat);
    drc::RuleSetBuilder b;
    drc::Rule rule = DrcFixture::rule(b.why("x %d %q"), 3);
    f.editor.drc.rules = b.build();
    CHECK(drc::why_text(f.editor, rule) == "x 3um %q");  // no output style: 1 um per unit
  }
}

TEST_CASE("why texts in every unit mode", "[cap:drc-commands][req:Why-text substitution]") {
  const drc::Rule r = DrcFixture::rule(0, 30, 25);
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::microns, true}) == "0.15um 0.000625um^2");
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::microns, false}) == "0.15 0.000625");
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::internal, true}) == "30i 25i^2");
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::internal, false}) == "30 25");
  cmd::CoordinateUnits lambda = half_centimicron();
  lambda.internal_per = 2;
  lambda.lambda_per = 1;
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::lambda, true}, lambda) == "15l 6.25l^2");
  cmd::CoordinateUnits grid = half_centimicron();
  grid.grid = std::array<std::int64_t, 4>{0, 0, 10, 5};
  CHECK(substitute("%d %a", r, cmd::UnitsState{cmd::InputUnit::user, true}, grid) == "3gx 0.5gx*gy");
  // Without a window grid user units print internal integers.
  CHECK(substitute("%d", r, cmd::UnitsState{cmd::InputUnit::user, true}) == "30");
}

TEST_CASE("substituted values are cut to 19 characters", "[cap:drc-commands][req:Why-text substitution]") {
  cmd::CoordinateUnits huge;
  huge.internal_per = 1;
  huge.lambda_per = 1000000000;
  const std::string text = substitute("[%d]", DrcFixture::rule(0, 2000000000), cmd::UnitsState{cmd::InputUnit::lambda, true}, huge);
  CHECK(text.size() == 2 + 19);
  CHECK(text.find('l') == std::string::npos);
}

TEST_CASE("mask-layer rule values are converted from mask units", "[cap:drc-commands][req:Why-text substitution]") {
  drc::Rule r = DrcFixture::rule(0, 50);
  r.flags = drc::flags::kCifRule;
  cmd::CoordinateUnits u;
  u.cif_scale = std::array<std::int64_t, 2>{5, 1};  // 0.05 um per internal unit, 5 mask units per internal unit
  CHECK(substitute("cif %d", r, {}, u) == "cif 0.5um");
  CHECK(substitute("cif %d", r, cmd::UnitsState{cmd::InputUnit::internal, true}, u) == "cif 10i");
  CHECK(substitute("cif %d", r, cmd::UnitsState{cmd::InputUnit::user, true}, u) == "cif 50");
}

// --- drc ignore -----------------------------------------------------------------------------------

TEST_CASE("Ignore a rule class", "[cap:drc-commands][req:drc ignore]") {
  DrcFixture f;
  f.whys({"m1 spacing", "m2 spacing", "m3 width"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 1}, DrcFixture::rule(6)}, {top, db::Rect{2, 0, 3, 1}, DrcFixture::rule(7)}};
  const drc::ScopedRecheck scoped(fake.fn());
  CHECK(f.out("drc ignore") == "(none)\n");
  CHECK(f.out("drc ignore spacing").empty());
  CHECK(f.out("drc ignore") == "m2 spacing\nm1 spacing\n");
  CHECK(f.out("drc why") == "No errors found.\n");
  CHECK(f.value("drc listall why") == "{m1 spacing} {{0 0 1 1}} {m2 spacing} {{2 0 3 1}}");  // listall ignores nothing
  CHECK(f.out("drc ignore NONE").empty());
  CHECK(f.out("drc ignore") == "(none)\n");
  CHECK(f.out("drc why") == "m1 spacing\nm2 spacing\n");
}

TEST_CASE("drc ignore adds a repeated text again, as Magic's list does", "[cap:drc-commands][req:drc ignore]") {
  DrcFixture f;
  f.whys({"m1 spacing", "m2 width"});
  f.run("drc ignore spacing");
  f.run("drc ignore spacing");
  CHECK(f.out("drc ignore") == "m1 spacing\nm1 spacing\n");
}

TEST_CASE("Last why-text with magic-compat ON", "[cap:drc-commands][req:drc ignore]") {
  DrcFixture f;
  f.compat(true);
  f.whys({"m1 width", "m3 spacing"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 1}, DrcFixture::rule(7)}};
  const drc::ScopedRecheck scoped(fake.fn());
  f.run("drc ignore spacing");
  CHECK(f.out("drc ignore") == "(none)\n");
  CHECK(f.out("drc why") == "m3 spacing\n");
}

TEST_CASE("Last why-text with magic-compat OFF", "[cap:drc-commands][req:drc ignore]") {
  DrcFixture f;
  f.compat(false);
  f.whys({"m1 width", "m3 spacing"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{0, 0, 1, 1}, DrcFixture::rule(7)}};
  const drc::ScopedRecheck scoped(fake.fn());
  f.run("drc ignore spacing");
  CHECK(f.out("drc ignore") == "m3 spacing\n");
  CHECK(f.out("drc why") == "No errors found.\n");
}

TEST_CASE("matching_whys scans the predefined texts too", "[cap:drc-commands][req:drc ignore]") {
  drc::WhyTable why;
  why.intern("x");
  const drc::IgnoreList found = drc::matching_whys(why, "subcell", true);
  CHECK(found.items() == std::vector<drc::WhyIndex>{drc::why::kInSubcell, drc::why::kSubcellOverlap});
}

// --- Scripting results ----------------------------------------------------------------------------

TEST_CASE("listall why structure", "[cap:drc-commands][req:Scripting results on iPadOS and non-Tcl hosts]") {
  DrcFixture f;
  f.whys({"w"});
  db::CellDef* top = shown_top(f);
  FakeRecheck fake;
  fake.errors = {{top, db::Rect{1, 0, 2, 10}, DrcFixture::rule(6)}, {top, db::Rect{6, 0, 7, 10}, DrcFixture::rule(6)}};
  const drc::ScopedRecheck scoped(fake.fn());
  const auto r = f.run("drc listall why");
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "w {{1 0 2 10} {6 0 7 10}}");
  using L = cmd::Value::List;
  CHECK(r.value == cmd::Value{L{"w", L{L{1, 0, 2, 10}, L{6, 0, 7, 10}}}});
  CHECK(DrcFixture::texts(r).empty());
}

TEST_CASE("list results are nested values of strings and integers",
          "[cap:drc-commands][req:Scripting results on iPadOS and non-Tcl hosts]") {
  DrcFixture f;
  db::CellDef* top = shown_top(f);
  error_row(*top, 2);
  using L = cmd::Value::List;
  // One {cell count} pair per cell. Built step by step: `L{L{...}}` may pick
  // the copy constructor instead of a one-element list (compilers differ).
  L pairs;
  pairs.emplace_back(L{"top", 2});
  CHECK(f.run("drc list count").value == cmd::Value{pairs});
  CHECK(f.value("drc list count") == "{top 2}");
  CHECK(f.run("drc list count total").value == cmd::Value{2});
  f.editor.drc.styles = drc::DrcStyleList{{"a b", "c"}};
  CHECK(f.run("drc listall style").value == cmd::Value{L{"a b", "c"}});
  CHECK(f.value("drc listall style") == "{a b} c");
}
