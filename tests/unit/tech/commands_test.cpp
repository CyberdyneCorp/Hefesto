// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capabilities tech-command (the `tech` command) and
// tech-loading-and-scaling (session loads, scalegrid, grid limit,
// post-load adjustments, effect on existing layout) through the dispatcher.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <variant>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/commands.hpp"
#include "tech_fixture.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
using tech::test::tiny;
using tech::test::with;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {"/pdk"}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(tech::register_tech_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    return dispatcher->execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }

  void load_tiny() {
    file("/t/tiny.tech", tiny());
    const auto r = run("tech", {"load", "/t/tiny", "-noprompt"});
    REQUIRE(r.ok());
  }

  db::CellDef* painted_cell(tech::PlaneId plane, tech::TileType type, const db::Rect& area) {
    auto def = editor.cells.create("c" + std::to_string(editor.cells.size()), editor.technology->plane_count());
    REQUIRE(def.ok());
    db::PaintRow row{};
    row.fill(type);
    REQUIRE(def.value()->plane(plane).paint(area, row).ok());
    def.value()->flags().available = true;
    return def.value();
  }
};

std::string text(const cmd::CommandResult& r) {
  return r.value.to_string();
}

bool says(const cmd::CommandResult& r, std::string_view needle) {
  for (const auto& d : r.diagnostics) {
    if (d.text.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("tech options by unique prefix", "[cap:tech-command][req:Option parsing]") {
  Session s;
  s.load_tiny();
  CHECK(s.run("tech", {"lock", "poly"}).ok());
  CHECK_FALSE(s.editor.technology->active_types().test(10));
  auto r = s.run("tech", {"la"});
  CHECK_FALSE(r.ok());
  CHECK(says(r, "Ambiguous techinfo option: \"la\""));
  CHECK(says(r, "Type \":tech help\" for help."));
  r = s.run("tech", {"foo"});
  CHECK(says(r, "\"foo\" isn't a valid tech option."));
  CHECK(s.run("tech", {"::magic::name"}).ok());
  CHECK(text(s.run("tech", {"magic::name"})) == "tiny");
  CHECK(says(s.run("tech", {"layers", "a", "b"}), "wrong number of arguments to command \"tech\""));
}

TEST_CASE("tech help lists twelve options", "[cap:tech-command][req:tech help]") {
  Session s;
  for (const auto& r : {s.run("tech"), s.run("tech", {"help"})}) {
    const std::string out = text(r);
    CHECK(out.starts_with("Tech commands have the form \"tech option\",\nwhere option is one of:\n"));
    CHECK(std::count(out.begin(), out.end(), '\n') == 2 + 12 + 1);  // "load" help spans two lines
  }
}

TEST_CASE("tech name, filename and version", "[cap:tech-command][req:tech name, filename and version]") {
  Session s;
  CHECK(text(s.run("tech", {"name"})) == "minimum");
  CHECK(text(s.run("tech", {"filename"})).empty());
  s.load_tiny();
  CHECK(text(s.run("tech", {"name"})) == "tiny");
  CHECK(text(s.run("tech", {"filename"})) == "/t/tiny.tech");
  CHECK(text(s.run("tech", {"version"})) == "1.0 {Tiny demo process}");
}

// Regression (TECH-G03): Magic appends only the description as a list element,
// so a multi-word version (words joined by LF) stays unquoted.
TEST_CASE("tech version with a multi-word version", "[cap:tech-command][req:tech name, filename and version]") {
  Session s;
  s.file("/t/v.tech", with(with(tiny(), "    version 1.0\n", "    version 1.0 beta\n"), "\"Tiny demo process\"",
                           "\"a b\""));
  REQUIRE(s.run("tech", {"load", "/t/v", "-noprompt"}).ok());
  CHECK(text(s.run("tech", {"version"})) == "1.0\nbeta {a b}");
  s.project.compat.magic_compat = false;
  CHECK(text(s.run("tech", {"version"})) == "{1.0\nbeta} {a b}");
}

TEST_CASE("tech lambda reports {L I} and sets without rescaling", "[cap:tech-command][req:tech lambda]") {
  Session s;
  s.load_tiny();
  CHECK(text(s.run("tech", {"lambda"})) == "1 1");
  REQUIRE(s.run("scalegrid", {"1", "2"}).ok());
  CHECK(text(s.run("tech", {"lambda"})) == "1 2");
  CHECK(text(s.run("tech", {"lambda", "3", "2"})) == "2 3");
  CHECK(text(s.run("tech", {"lambda", "4"})) == "1 4");
  CHECK(text(s.run("tech", {"lambda", "6", "x"})) == "1 6");  // D not an integer: L unchanged
  s.project.compat.magic_compat = false;
  CHECK(s.run("tech", {"lambda", "2"}).diagnostics.at(0).code == "TECH-CMD-004");
}

TEST_CASE("tech planes lists every plane", "[cap:tech-command][req:tech planes]") {
  Session s;
  s.load_tiny();
  CHECK(text(s.run("tech", {"planes"})) ==
        "router designRuleCheck designRuleError mhint fhint rhint well active metal1 metal2");
}

TEST_CASE("tech layers formatted and list forms", "[cap:tech-command][req:tech layers]") {
  Session s;
  s.load_tiny();
  const std::string formatted = text(s.run("tech", {"layers"}));
  CHECK(formatted.starts_with("Layer names are:\n    nw or nwell\n    p or poly or polysilicon\n"));
  CHECK(formatted.find("    m2c or m2contact or via\n") != std::string::npos);
  CHECK(formatted.ends_with("    mag or magnet\n    f or fence\n    r or rotate\n"));
  CHECK(formatted.find("pc+m2c") == std::string::npos);
  CHECK(text(s.run("tech", {"layers", "*"})) ==
        "nwell polysilicon ndiffusion ntransistor metal1 metal2 polycontact ndcontact m2contact pwell magnet "
        "fence rotate");
  CHECK(text(s.run("tech", {"layers", "*m1"})) == "metal1 polycontact ndcontact m2contact");
}

TEST_CASE("tech locked, unlocked and revert", "[cap:tech-command][req:tech locked, unlocked and revert]") {
  Session s;
  s.load_tiny();
  CHECK(text(s.run("tech", {"locked"})) == "pwell");
  REQUIRE(s.run("tech", {"locked", "metal1"}).ok());
  CHECK(text(s.run("tech", {"locked"})) == "metal1 pwell");
  CHECK(s.editor.locked_types.test(13));

  REQUIRE(s.run("tech", {"locked", "pc"}).ok());
  const auto& t = *s.editor.technology;
  CHECK(t.erase_result(8, 15, 13) == 15);
  CHECK_FALSE(t.active_types().test(19));

  REQUIRE(s.run("tech", {"unlocked", "pwell"}).ok());
  CHECK_FALSE(s.editor.technology->active_types().test(0));  // restricted to 6..255
  CHECK(s.editor.technology->active_types().test(18));
  REQUIRE(s.run("tech", {"revert"}).ok());
  CHECK(text(s.run("tech", {"locked"})) == "pwell");
  CHECK(s.editor.technology->erase_result(8, 15, 13) == 0);

  REQUIRE(s.run("tech", {"unlocked", "pwell"}).ok());
  REQUIRE(s.run("tech", {"revert", "*"}).ok());  // Q-TECH-25: re-locks
  CHECK(text(s.run("tech", {"locked"})) == "pwell");
  REQUIRE(s.run("tech", {"locked", "*"}).ok());
  CHECK(text(s.run("tech", {"unlocked"})) == "magnet fence rotate");
  REQUIRE(s.run("tech", {"revert", "metal1"}).ok());
  CHECK(text(s.run("tech", {"unlocked"})) == "metal1 magnet fence rotate");
}

TEST_CASE("tech drc checks its arguments", "[cap:tech-command][req:tech drc]") {
  Session s;
  s.load_tiny();
  // Regression (golden TECH-G44): CmdTech's wrongNumArgs prints the first
  // line with TxPrintf (stdout) and only the usage hint with TxError.
  const auto wrong = s.run("tech", {"drc", "width"});
  REQUIRE(wrong.diagnostics.size() == 2);
  CHECK(wrong.diagnostics[0].severity == hefesto::diag::Severity::info);
  CHECK(wrong.diagnostics[0].text == "wrong number of arguments to command \"tech\"");
  CHECK(wrong.diagnostics[1].text == "  Type \":tech help\" for help.");
  CHECK(says(s.run("tech", {"drc", "width", "nosuch"}), "No such layer nosuch"));
  // TECH-CMD-005 is retired: the query answers from the compiled rule set
  // (tests/unit/drc/rules/query_test.cpp checks the values).
  CHECK(s.run("tech", {"drc", "width", "metal1"}).ok());
}

TEST_CASE("tech load options, confirmation, pre-check and failure", "[cap:tech-command][req:tech load]") {
  Session s;
  s.file("/t/tiny.tech", tiny());
  auto r = s.run("tech", {"load", "/t/tiny", "-quiet"});
  CHECK(says(r, "Usage: tech load <filename> [-noprompt] [-override]"));
  CHECK(s.editor.technology->name() == "minimum");
  REQUIRE(s.run("tech", {"load", "/t/tiny"}).ok());  // nothing painted: no confirmation needed
  (void)s.painted_cell(8, 13, db::Rect{0, 0, 4, 4});
  r = s.run("tech", {"load", "/t/tiny"});
  CHECK(r.diagnostics.at(0).code == "TECH-CMD-003");  // non-interactive: declined
  CHECK(s.run("tech", {"load", "/t/tiny", "-noprompt"}).ok());
  CHECK(s.run("tech", {"load", "/t/tiny", "-override", "-noprompt"}).ok());
}

TEST_CASE("the readability pre-check refuses files not starting with tech",
          "[cap:tech-loading-and-scaling][req:Readability pre-check]") {
  Session s;
  s.load_tiny();
  std::string text = tiny();
  const std::string head = "tech\n    format 35\n    tiny\nend\n";
  text = with(text, head, "") + head;
  s.file("/t/late.tech", text);
  const auto r = s.run("tech", {"load", "/t/late", "-noprompt"});
  CHECK(says(r, "Technology file does not exist or is not readable."));
  CHECK(r.value.to_string() == "Technology file does not exist or is not readable\n");  // Tcl result: no period
  CHECK(s.editor.technology->file_name() == "/t/tiny.tech");
  CHECK(says(s.run("tech", {"load", "/t/nothere"}), "Technology file does not exist or is not readable."));
}

TEST_CASE("a failed load keeps the previous technology", "[cap:tech-loading-and-scaling][req:Full technology load]") {
  Session s;
  s.load_tiny();
  s.file("/t/broken.tech", with(tiny(), "connect\n", "connectx\n"));
  const auto r = s.dispatcher->execute(cmd::Invocation{"tech", {"load", "/t/broken", "-noprompt"}, cmd::Origin::script, std::nullopt});
  CHECK_FALSE(r.ok());
  CHECK(says(r, "Section \"connect\" was missing from /t/broken.tech."));
  CHECK(says(r, "Error in loading technology file"));
  CHECK(r.value.to_string() == "Error in loading technology file\n");  // Tcl result
  CHECK(s.editor.technology->name() == "tiny");  // Q-TECH-10: atomic
  CHECK(s.editor.technology->file_name() == "/t/tiny.tech");
}

// Regression (m3-drc tasks.md 4.10, TECH-G44): a negative rule distance makes
// DRCTechAddRule return FALSE, so TechLoad lists drc among the sections with
// errors and the load fails with the previous technology kept.
TEST_CASE("a negative drc rule distance fails tech load and keeps the previous technology",
          "[cap:tech-loading-and-scaling][req:Full technology load]") {
  Session s;
  s.load_tiny();
  s.file("/t/negative.tech", with(tiny(), "drc\nend", "drc\n    width m1 -2 \"w\"\nend"));
  const auto r = s.run("tech", {"load", "/t/negative", "-noprompt"});
  CHECK_FALSE(r.ok());
  CHECK(says(r, "The following sections of /t/negative.tech contained errors:\n    drc"));
  CHECK(r.value.to_string() == "Error in loading technology file\n");
  CHECK(s.editor.technology->file_name() == "/t/tiny.tech");

  hefesto::diag::CollectingSink sink;
  auto services_view = s.services.view();
  const auto status = tech::load_session_technology(s.editor, services_view, "/t/negative.tech", {}, sink);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "TECH-LOAD-001");
  CHECK(s.editor.technology->file_name() == "/t/tiny.tech");
}

TEST_CASE("post-load scaling report at a doubled grid","[cap:tech-loading-and-scaling][req:Post-load adjustments]") {
  Session s;
  REQUIRE(s.run("scalegrid", {"1", "2"}).ok());
  s.file("/t/tiny.tech", tiny());
  const auto r = s.run("tech", {"load", "/t/tiny", "-noprompt"});
  REQUIRE(r.ok());
  CHECK(says(r, "Scaled tech values by 2 / 1 to match internal grid scaling"));
  CHECK(s.editor.grid == tech::GridScale{2, 1});
  // set-minimum-grid: 1 lambda = scalefactor / gridlimit internal units.
  s.file("/t/min.tech", with(tiny(), "cifoutput\nend", "cifoutput\nstyle s\nscalefactor 10\ngridlimit 5\n"
                                                      "options set-minimum-grid\nend"));
  REQUIRE(s.run("tech", {"load", "/t/min", "-noprompt"}).ok());
  CHECK(text(s.run("tech", {"lambda"})) == "1 2");
}

TEST_CASE("existing layout is kept and flagged after a load", "[cap:tech-loading-and-scaling][req:Effect on existing layout]") {
  Session s;
  s.load_tiny();
  (void)s.painted_cell(9, 14, db::Rect{0, 0, 2, 2});
  s.file("/t/three.tech", tech::test::skeleton("well\nactive\nmetal1\n", "metal1 m1\n"));
  const auto r = s.run("tech", {"load", "/t/three", "-noprompt"});
  REQUIRE(r.ok());
  CHECK(says(r, "Warning:  Number of planes has changed.  Existing layout may be invalid."));
}

TEST_CASE("a load with a different plane count resizes every cell's planes",
          "[cap:tech-loading-and-scaling][req:Effect on existing layout]") {
  Session s;
  s.load_tiny();
  auto* def = s.painted_cell(6, 13, db::Rect{0, 0, 2, 2});
  s.file("/t/three.tech", tech::test::skeleton("well\nactive\nmetal1\n", "metal1 m1\n"));
  REQUIRE(s.run("tech", {"load", "/t/three", "-noprompt"}).ok());
  CHECK(def->plane_count() == s.editor.technology->plane_count());
  CHECK_FALSE(def->plane(6).empty());  // planes below both counts keep their paint
  REQUIRE(s.run("tech", {"load", "/t/tiny", "-noprompt"}).ok());
  CHECK(def->plane_count() == s.editor.technology->plane_count());
  CHECK(def->plane(static_cast<tech::PlaneId>(def->plane_count() - 1)).empty());  // a new plane starts empty
}

TEST_CASE("scalegrid scales the box once with magic-compat off",
          "[cap:tech-loading-and-scaling][req:scalegrid command]") {
  Session s;
  s.project.compat.magic_compat = false;
  s.load_tiny();
  s.editor.box = db::Rect{0, 0, 3, 5};
  REQUIRE(s.run("scalegrid", {"1", "2"}).ok());
  CHECK(*s.editor.box == db::Rect{0, 0, 6, 10});
}

// Regression (8.1 close-out): metric units used the output style as read,
// ignoring `scalegrid` (Magic rescales it with CIFTechOutputScale).
TEST_CASE("metric coordinates follow scalegrid", "[cap:cmd-coordinates-and-units][req:Unit suffixes]") {
  Session s;
  REQUIRE(hefesto::db::register_db_commands(s.registry));
  s.file("/t/m.tech", with(tiny(), "cifoutput\nend", "cifoutput\nstyle s\nscalefactor 10 nanometers\nend"));
  REQUIRE(s.run("tech", {"load", "/t/m", "-noprompt"}).ok());
  s.editor.edit_cell = s.painted_cell(6, 13, db::Rect{0, 0, 1, 1});
  // Centimicrons: divisors 1.0 and 0.5 are exact in single precision.
  REQUIRE(s.run("box", {"0", "0", "100cu", "6cu"}).ok());
  CHECK(*s.editor.box == db::Rect{0, 0, 100, 6});  // 10 nm per internal unit
  REQUIRE(s.run("scalegrid", {"1", "2"}).ok());
  REQUIRE(s.run("box", {"0", "0", "100cu", "6cu"}).ok());
  CHECK(*s.editor.box == db::Rect{0, 0, 200, 12});  // 5 nm per internal unit
}

TEST_CASE("a lookup after a load reports TechPrintLine's end-of-file location",
          "[cap:tech-file-format][req:Error reporting and recovery]") {
  Session s;
  s.load_tiny();
  const auto r = s.run("tech", {"layers", "nosuchlayer"});
  REQUIRE_FALSE(r.diagnostics.empty());
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&r.diagnostics.front().location);
  REQUIRE(where != nullptr);
  CHECK(where->file == "/t/tiny.tech");
  const std::string text_of_file = tiny();
  REQUIRE(text_of_file.ends_with('\n'));
  // The read at end of file counts too (utils/tech.c:1009): last line + 1.
  CHECK(where->line == static_cast<std::uint32_t>(std::count(text_of_file.begin(), text_of_file.end(), '\n') + 1));
  REQUIRE(r.diagnostics.front().related.has_value());
  CHECK(r.diagnostics.front().related->name == "(none)");
}

TEST_CASE("internal grid scale stays reduced", "[cap:tech-loading-and-scaling][req:Internal grid scale]") {
  Session s;
  const auto r = s.run("scalegrid", {"2", "4"});
  CHECK(text(r).empty());  // the scale is printed (TxPrintf), not returned
  CHECK(says(r, "2 Magic internal units = 1 Lambda"));
  CHECK(s.editor.grid == tech::GridScale{2, 1});
  CHECK(tech::reduce(tech::GridScale{6, 4}) == tech::GridScale{3, 2});
}

TEST_CASE("scalegrid rescales box and layout", "[cap:tech-loading-and-scaling][req:scalegrid command]") {
  Session s;
  s.load_tiny();
  s.editor.box = db::Rect{0, 0, 3, 5};
  auto* def = s.painted_cell(8, 13, db::Rect{-3, 0, 1, 2});
  auto r = s.run("scalegrid", {"1", "2"});
  REQUIRE(r.ok());
  CHECK(says(r, "2 Magic internal units = 1 Lambda"));
  // magic-compat: DBScaleEverything and CmdScalegrid both rescale the box,
  // so it grows by (b/a)^2 as in Magic (TECH-G36).
  CHECK(*s.editor.box == db::Rect{0, 0, 12, 20});
  CHECK(def->plane(8).bounding_box() == db::Rect{-6, 0, 2, 4});
  REQUIRE(s.editor.windows.open(def, {0, 0, 400, 400}).ok());
  s.editor.windows.current()->move_to(db::Rect{-10, -10, 30, 30});
  const db::Rect before = s.editor.windows.current()->mapping().surface;
  r = s.run("scalegrid", {"2", "1"});
  // WindScale: the window keeps showing the same layout, at the new scale.
  const db::Rect after = s.editor.windows.current()->mapping().surface;
  CHECK(after.width() < before.width());
  CHECK(after.x0 <= tech::scale_value(before.x0, 1, 2));  // the scaled area is still in view
  CHECK(after.x1 >= tech::scale_value(before.x1, 1, 2));
  CHECK(says(r, "1 Magic internal unit = 1 Lambda"));
  CHECK(*s.editor.box == db::Rect{0, 0, 3, 5});
  REQUIRE(s.run("scalegrid", {"2", "1"}).ok());  // -3 -> floor(-1.5) = -2
  CHECK(def->plane(8).bounding_box().x0 == -2);
  CHECK(says(s.run("scalegrid", {"1:3"}), "3 Magic internal units = 2 Lambda"));
  CHECK(says(s.run("scalegrid", {"3/3"}), "3 Magic internal units = 2 Lambda"));
  for (const auto& args : std::vector<std::vector<std::string>>{{"0", "2"}, {"2"}, {"a", "b"}, {"1", "2", "3"}}) {
    CHECK(s.run("scalegrid", args).diagnostics.at(0).text ==
          "Usage:  scalegrid a b, where a and b are strictly positive integers");
  }
  CHECK(tech::scale_value(-3, 1, 2) == -2);
  CHECK(tech::scale_value(1073741818, 2, 1) == 1073741818);  // an infinity marker
}

TEST_CASE("the grid limit check uses the CIF output style", "[cap:tech-loading-and-scaling][req:Grid limit check]") {
  Session s;
  s.file("/t/g.tech", with(tiny(), "cifoutput\nend", "cifoutput\nstyle s\nscalefactor 5\ngridlimit 5\nend"));
  REQUIRE(s.run("tech", {"load", "/t/g", "-noprompt"}).ok());
  CHECK(s.run("scalegrid", {"1", "2"}).ok());
  const auto r = s.run("scalegrid", {"1", "20"});
  CHECK(says(r, "Grid scaling is finer than limit set by the process!"));
  CHECK(tech::check_grid_limit(*tech::builtin_technology(), 1, 1000).ok());  // no output style
}

TEST_CASE("technology loads from the PDK store", "[cap:tech-loading-and-scaling][req:Automatic load from a cell's technology]") {
  Session s;
  s.file("/pdk/tiny/libs.tech/magic/tiny.tech", tiny());
  hefesto::diag::CollectingSink sink;
  tech::SessionLoadOptions options;
  options.precheck = false;  // automatic loads skip the pre-check
  auto services_view = s.services.view();
  const auto status = tech::load_session_technology(s.editor, services_view, "tiny", options, sink);
  REQUIRE(status.ok());
  CHECK(s.editor.technology->name() == "tiny");
  CHECK(s.editor.technology->file_name() == "/pdk/tiny/libs.tech/magic/tiny.tech");
  const auto missing = tech::load_session_technology(s.editor, services_view, "nothere", options, sink);
  CHECK(missing.error().code == "TECH-FILE-001");
}

// Regression (UI-G-067): `tech load` reloads the new technology's display
// styles from where the startup looked, so its dstyle long names resolve.
TEST_CASE("tech load reloads the display styles of the new styletype",
          "[cap:display-style-files][req:Style loading triggered by styletype]") {
  const std::string magic = std::string{HEFESTO_MAGIC_REF_DIR};
  const std::string dstyle = hefesto::tech::test::read_file(magic + "/scmos/mos.OpenGL.dstyle");
  if (dstyle.empty()) {
    SKIP("Magic sources (build/ref/magic) not available");
  }
  Session s;
  s.file("/sys/mos.OpenGL.dstyle", dstyle);
  s.file("/sys/mos.OpenGL.std.cmap", hefesto::tech::test::read_file(magic + "/scmos/mos.OpenGL.std.cmap"));
  s.file("/sys/color.glyphs", hefesto::tech::test::read_file(magic + "/graphics/color.glyphs"));
  s.editor.style_dirs = {"/sys"};
  CHECK_FALSE(s.editor.styles.has_value());
  s.load_tiny();  // styletype mos
  REQUIRE(s.editor.styles.has_value());
  s.editor.style_dirs = {"/nowhere"};
  s.load_tiny();  // missing files keep the styles and do not fail the load (Q-TECH-24)
  CHECK(s.editor.styles.has_value());
}
