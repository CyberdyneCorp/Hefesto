// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The file prompts (capability ui-gui-managers "File prompts").

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "gui_fixture.hpp"
#include "hefesto/gui/prompts.hpp"

namespace gui = hefesto::gui;
using gui::test::Session;
using hefesto::platform::FilePickerRequest;
using hefesto::platform::ScriptedFilePicker;

namespace {

const std::string kInv = "magic\ntech small\ntimestamp 0\n<< end >>\n";

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

TEST_CASE("promptload gds asks for GDS files and reads them", "[cap:ui-gui-managers][req:File prompts]") {
  // "GDS with a single top cell" needs `gds read` (gds-read, M5): only the
  // dialog request and the command issued are checked here.
  Session s;
  ScriptedFilePicker picker;
  picker.push_answer(std::string{"/work/chip.gds"});
  const auto r =
      s.in_command([&](hefesto::cmd::CommandContext& ctx) { return gui::prompt_load(ctx, &picker, "gds"); });
  REQUIRE(r.ok());
  REQUIRE(picker.requests().size() == 1);
  CHECK(picker.requests()[0].file_types.front() == std::pair<std::string, std::string>{"GDS", ".gds .strm .cal"});
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
