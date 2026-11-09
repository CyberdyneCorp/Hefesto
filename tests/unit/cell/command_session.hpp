// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the CELL command tests: an editor session with the
// CELL commands on a dispatcher over in-memory files (tiny technology,
// cell search path /work), scripted like tests/golden/CELL/CASES.md.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "cell_fixture.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cell/crash.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::cell::test {

struct CommandSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit CommandSession(std::unique_ptr<FileLockService> locks = nullptr) {
    REQUIRE(register_cell_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    if (locks) {
      editor.locks = std::move(locks);
    }
    editor.technology = tiny_technology();
    cell_session(editor).host = make_memory_file_host(files(), "/work");
    editor.search_paths.path = {"/work"};
  }

  platform::FileSystem& files() { return services.files(); }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    return dispatcher->execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }

  void write(const std::string& path, const std::string& text) { REQUIRE(files().write_atomic(path, text).ok()); }
};

inline std::string last_text(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text;
}

inline bool has_text(const cmd::CommandResult& r, std::string_view text) {
  for (const auto& d : r.diagnostics) {
    if (d.text == text) {
      return true;
    }
  }
  return false;
}

}  // namespace hefesto::cell::test
