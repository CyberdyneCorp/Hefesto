// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M4 round-1 integration (openspec/changes/m4-scripting tasks.md 13.1):
// the shared-file changes the workstreams asked for — the keyword
// terminator rule on every dispatched line, the interpreter closed first by
// ~EditorState, the session's file picker, the -T override and the PDK
// command registration.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/binding/keyword_rule.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace editor = hefesto::editor;
namespace script = hefesto::script;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  // Declared after the dispatcher: destroyed first, with its interpreter
  // still bound, as in a front end that never closes the project.
  std::unique_ptr<editor::EditorState> state = std::make_unique<editor::EditorState>();

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(state.get());
  }

  cmd::CommandResult run(std::string_view line, cmd::Origin origin = cmd::Origin::script) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, origin)) {
      last = std::move(r);
    }
    return last;
  }
};

}  // namespace

// BINDING open item (tasks.md 5.8): the drcstate bridge lives in the
// interpreter and is registered in `drc.events`; `drc` is destroyed before
// `script`, so the interpreter must be closed first or deleting it touches
// the freed event list (caught by the macos-asan build).
TEST_CASE("an editor with a bound interpreter is destroyed without closing it first",
          "[cap:scripting-api][req:Post-command bookkeeping]") {
  Session s;
  (void)script::open_project_interpreter(*s.state, script::SandboxPolicy{});
  REQUIRE(script::bind_registry(*s.state->script.interp, s.registry, s.dispatcher).ok());
  REQUIRE(s.state->script.interp->evaluate("drc off; drc on").ok());
  s.dispatcher.set_editor(nullptr);
  s.state.reset();  // ~EditorState closes the interpreter before `drc` goes
  CHECK(s.state == nullptr);
}

// BINDING open item (tasks.md 5.6): lines typed in the command bar or issued
// by the GUI follow the project's keyword rule too, not only script lines.
TEST_CASE("every dispatched line uses the project's keyword terminator rule",
          "[cap:scripting-api][req:Option keyword matching]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s;
    s.project.compat.magic_compat = compat;
    std::optional<cmd::KeywordTerminator> seen;
    REQUIRE_FALSE(s.registry.add(cmd::CommandSpec{"kwprobe", "kwprobe", "records the keyword rule",
                                            [&seen](cmd::CommandContext&, const cmd::Invocation&) {
                                              seen = cmd::keyword_terminator();
                                              return cmd::CommandResult::success();
                                            }})
                      .has_value());
    REQUIRE(s.run("kwprobe", cmd::Origin::gui).ok());
    REQUIRE(seen.has_value());
    CHECK(*seen == (compat ? cmd::KeywordTerminator::space : cmd::KeywordTerminator::whitespace));
    CHECK(cmd::keyword_terminator() == cmd::KeywordTerminator::space);  // the scope ended with the command
  }
}

// GUIMGR open item (tasks.md 9.5): the file prompts use the front end's
// picker from the session.
TEST_CASE("promptload asks the session's file picker", "[cap:ui-gui-managers][req:File prompts]") {
  Session s;
  hefesto::platform::ScriptedFilePicker picker;  // empty queue: the user cancels
  s.state->file_picker = &picker;
  (void)s.run("magic::promptload magic");
  REQUIRE(picker.requests().size() == 1);
  CHECK(picker.requests().front().mode == hefesto::platform::FilePickerRequest::Mode::open);
  s.state->file_picker = nullptr;
  (void)s.run("magic::promptload magic");
  CHECK(picker.requests().size() == 1);  // no picker: a cancelled dialog, nothing asked
}

// PDKINT open item (tasks.md 6.3): `ext2spice scale` is part of the full
// command set every front end registers.
TEST_CASE("the full command set has the PDK generator commands",
          "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  Session s;
  REQUIRE(s.registry.find("ext2spice") != nullptr);
  CHECK(s.run("ext2spice scale off").ok());
}
