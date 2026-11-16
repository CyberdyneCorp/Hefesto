// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The file prompts (capability ui-gui-managers "File prompts").

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gui_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/gui/prompts.hpp"

namespace gui = hefesto::gui;
using gui::test::Session;
using hefesto::platform::FilePickerRequest;
using hefesto::platform::ScriptedFilePicker;

namespace {

const std::string kInv = "magic\ntech small\ntimestamp 0\n<< end >>\n";

// A technology whose metal1 is written to and read from GDS layer 49 and
// CIF layer CM1.
constexpr std::string_view kMaskTech =
    "tech\n format 35\n masks\nend\nplanes\n metal1,m1\nend\ntypes\n metal1 metal1,m1\nend\n"
    "contact\nend\nstyles\n styletype mos\n metal1 20\nend\ncompose\nend\nconnect\nend\n"
    "cifoutput\nstyle gdsii\n scalefactor 10\n layer CM1 metal1\n  labels metal1\n  calma 49 0\nend\n"
    "cifinput\nstyle gdsii\n scalefactor 10\n layer metal1 CM1\n  labels CM1\n  calma CM1 49 0\nend\n"
    "drc\nend\nextract\nend\n";

void load_mask_tech(Session& s) {
  s.write("/work/masks.tech", kMaskTech);
  s.ok("tech load /work/masks.tech");
}

// A window on a new cell `name` with metal1 (0,0)-(4,2).
void draw_cell(Session& s, const std::string& name) {
  s.ok("openwindow");
  s.ok("load " + name);
  s.ok("snap internal");
  s.ok("box 0 0 4 2");
  s.ok("paint metal1");
}

// The bytes `gds write` gives for a cell `name` drawn by draw_cell.
std::string written_gds(const std::string& name) {
  Session w;
  load_mask_tech(w);
  draw_cell(w, name);
  w.ok("gds write /work/out");
  return w.read("/work/out.gds");
}

}  // namespace

TEST_CASE("promptload magic loads the chosen file's cell", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  s.write("/work/lib/inv.mag", kInv);
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/lib/inv.mag"});
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "magic"); })
              .ok());
  REQUIRE(picker.requests().size() == 1);
  CHECK(picker.requests()[0].mode == FilePickerRequest::Mode::open);
  CHECK(picker.requests()[0].file_types.front().first == "Magic");
  CHECK(s.editor.cells.find("inv") != nullptr);
  // A cancelled dialog does nothing; so does a missing picker or an unknown type.
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "magic"); })
              .ok());
  CHECK(s.run("promptload magic").ok());
  CHECK(s.run("promptload nothing").ok());
  CHECK(s.run("promptload").raises);
}

TEST_CASE("promptload getcell adds the directory to the search path", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  s.editor.edit_cell = s.cell("top");
  s.write("/work/lib/inv.mag", kInv);
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/lib/inv.mag"});
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "getcell"); })
              .ok());
  const std::string search = s.run("path search").value.to_string();
  CHECK(search.find("/work/lib") != std::string::npos);
  CHECK(s.run("path cell").value.to_string().find("/work/lib/") != std::string::npos);
}

TEST_CASE("the gds, cif and magic prompts pass wrapper.tcl's file types", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  ScriptedFilePicker picker;
  for (const char* type : {"gds", "cif", "magic"}) {
    picker.push_answer(std::nullopt);
    REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, type); })
                .ok());
  }
  picker.push_answer(std::nullopt);
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_save(ctx, &picker, "cif"); }).ok());
  REQUIRE(picker.requests().size() == 4);
  using Type = std::pair<std::string, std::string>;
  // wrapper.tcl's Tk extension lists (an intercepted tk_getOpenFile prints
  // them, SCRIPT-GUI-11); the desktop host turns them into globs.
  CHECK(picker.requests()[0].file_types.front() == Type{"GDS", ".gds .strm .cal"});
  CHECK(picker.requests()[1].file_types.front() == Type{"CIF", ".cif"});
  CHECK(picker.requests()[2].file_types.front() == Type{"Magic", ".mag"});
  CHECK(picker.requests()[3].mode == FilePickerRequest::Mode::save);
  CHECK(picker.requests()[3].file_types.back() == Type{"All files", "*"});
  // A cancelled dialog does nothing (no cif_temp is created).
  CHECK(s.editor.cells.find("cif_temp") == nullptr);
}

TEST_CASE("GDS with a single top cell", "[cap:ui-gui-managers][req:File prompts]") {
  const std::string gds = written_gds("chip");
  Session s;
  load_mask_tech(s);
  s.write("/work/chip.gds", gds);
  REQUIRE(s.run("openwindow").ok());  // a window on (UNNAMED); no cells are loaded
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/chip.gds"});
  const auto r =
      s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "gds"); });
  INFO(gui::test::all_text(r));
  REQUIRE(r.ok());
  REQUIRE(picker.requests().size() == 1);
  CHECK(picker.requests()[0].mode == FilePickerRequest::Mode::open);
  // `gds read /work/chip` (the root name), then `load chip`.
  CHECK(s.run("cellname list window").value.to_string() == "chip");
  const hefesto::db::CellDef* chip = s.editor.cells.find("chip");
  REQUIRE(chip != nullptr);
  CHECK(chip->bbox() == hefesto::db::Rect{0, 0, 4, 2});
}

TEST_CASE("promptsave gds writes the chosen file", "[cap:ui-gui-managers][req:File prompts]") {
  for (const bool compat : {true, false}) {
    // A directory with a dot: the chosen name keeps its extension in both
    // modes (Q-MASK-210 adds none to a name that has one).
    Session s;
    s.project.compat.magic_compat = compat;
    load_mask_tech(s);
    draw_cell(s, "chip");
    REQUIRE(s.services.files().create_directories("/work/v1.0").ok());
    ScriptedFilePicker picker;
    picker.push_answer(std::string{"/work/v1.0/chip.gds"});
    const auto r =
        s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_save(ctx, &picker, "gds"); });
    INFO("magic-compat " << compat << ": " << gui::test::all_text(r));
    REQUIRE(r.ok());
    REQUIRE(picker.requests().size() == 1);
    CHECK(picker.requests()[0].mode == FilePickerRequest::Mode::save);
    CHECK(s.services.files().exists("/work/v1.0/chip.gds"));
    CHECK_FALSE(s.services.files().exists("/work/v1.0/chip"));
  }
}

TEST_CASE("promptsave cif passes the root name", "[cap:ui-gui-managers][req:File prompts]") {
  // Q-MASK-320: `cif write` always appends ".cif" with magic-compat ON, so
  // the picker passes chip, never chip.cif (Magic's GUI writes chip.cif.cif).
  for (const bool compat : {true, false}) {
    Session s;
    s.project.compat.magic_compat = compat;
    load_mask_tech(s);
    draw_cell(s, "chip");
    ScriptedFilePicker picker;
    picker.push_answer(std::string{"/work/chip.cif"});
    const auto r =
        s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_save(ctx, &picker, "cif"); });
    INFO("magic-compat " << compat << ": " << gui::test::all_text(r));
    REQUIRE(r.ok());
    CHECK(s.services.files().exists("/work/chip.cif"));
    CHECK_FALSE(s.services.files().exists("/work/chip.cif.cif"));
  }
}

TEST_CASE("promptload cif reads through cif_temp and loads its first child",
          "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  load_mask_tech(s);
  s.write("/work/inv.cif", "DS 1 1 1;\n9 inv;\nL CM1;\nB 40 20 20 10;\nDF;\nC 1;\nE\n");
  REQUIRE(s.run("openwindow").ok());
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/inv.cif"});
  const auto r =
      s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "cif"); });
  INFO(gui::test::all_text(r));
  REQUIRE(r.ok());
  CHECK(s.run("cellname list window").value.to_string() == "inv");
  CHECK(s.editor.cells.find("cif_temp") == nullptr);  // deleted after the read
  CHECK(s.editor.cells.find("inv") != nullptr);
}

TEST_CASE("promptsave names an unnamed cell, then writes all", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  REQUIRE(s.run("openwindow").ok());  // a window on (UNNAMED)
  s.ok("snap internal");
  s.ok("box 0 0 4 4");
  s.ok("paint metal1");
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/saved.mag"});
  const auto saved =
      s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_save(ctx, &picker, "force"); });
  INFO(gui::test::all_text(saved));
  REQUIRE(saved.ok());
  REQUIRE(picker.requests().size() == 1);
  CHECK(picker.requests()[0].mode == FilePickerRequest::Mode::save);
  CHECK(picker.requests()[0].title == "Save cell (UNNAMED) as:");
  // The chosen directory is the working directory: `save saved.mag` (relative).
  CHECK(s.editor.cells.find("saved") != nullptr);
  CHECK(s.services.files().exists("saved.mag"));
  // Nothing is unnamed now: no dialog.
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_save(ctx, &picker, "magic"); })
              .ok());
  CHECK(picker.requests().size() == 1);
  // saveallandquit quits without a prompt when nothing is modified.
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::save_all_and_quit(ctx, &picker); }).ok());
  CHECK(s.script.quit_requested);
}

TEST_CASE("prompt_dialog new and save", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  REQUIRE(s.run("openwindow").ok());
  std::string initial_new;
  std::string initial_save;
  REQUIRE(s.in_command([&](hefesto::cmd::CommandContext& ctx) {
               initial_new = gui::prompt_dialog_initial(ctx, "new");
               initial_save = gui::prompt_dialog_initial(ctx, "save");
               return hefesto::cmd::CommandResult::success();
             })
              .ok());
  CHECK(initial_new == "(UNNAMED)");
  CHECK(initial_save == "(UNNAMED)");
  CHECK(gui::prompt_dialog_command("new", "inv") == std::vector<std::string>{"load", "inv"});
  CHECK(gui::prompt_dialog_command("save", "inv") == std::vector<std::string>{"save", "inv"});
}
