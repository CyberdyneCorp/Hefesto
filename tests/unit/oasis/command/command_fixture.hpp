// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the `oasis` command tests (tasks.md 13.1, 13.2): the oasis-read
// session (tests/unit/oasis/read/oasis_read_fixture.hpp: the gds-read test
// technology and cifinput style, OASIS files written by the codec's
// Encoder) with every command registered, a dispatcher over in-memory
// files, and a window on cell `top`. A stub write host records the files
// `oasis write` would write, so the command's naming and host routing are
// tested apart from oasis::write (OASW).
// Owner: OASCMD.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../read/oasis_read_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace oasis_command_test {

using namespace oasis_read_test;  // NOLINT(google-build-using-namespace): the shared fixture
namespace cmd = hefesto::cmd;
namespace platform = hefesto::platform;

struct CommandSession : OasSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  db::CellDef* top = nullptr;
  std::vector<std::string> hosted_writes;  // the labels the stub write host received

  explicit CommandSession(bool magic_compat = true) : OasSession(kStyle, magic_compat) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    top = editor.cells.create("top", editor.technology->plane_count()).value();
    top->flags().available = true;
    editor.edit_cell = top;
    REQUIRE(editor.windows.open(top, hefesto::view::ScreenRect{0, 0, 999, 999}).ok());
  }

  /// Installs a write host that records its label and writes nothing.
  void stub_write_host() {
    editor.mask.write_host = [this](std::string_view label, const hefesto::mask::ProgressWork&) {
      hosted_writes.emplace_back(label);
      return cmd::CommandResult::success();
    };
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  /// The result value of `line` as a string; the command must succeed.
  std::string value(std::string_view line) {
    const cmd::CommandResult r = run(line);
    INFO(std::string{line} + " -> " + (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
    return r.value.to_string();
  }

  /// Writes `bytes` to `path` in the command's file system.
  void put(const std::string& path, const Bytes& bytes) {
    REQUIRE(services.view()
                .files.write_atomic(path, std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()})
                .ok());
  }

  /// Every diagnostic text published since the last call.
  std::string take_printed() {
    std::string text;
    for (const auto& d : printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

/// The code of the last diagnostic of `r`.
inline std::string last_code(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.back().code;
}

/// A file with cell `A` (a rectangle on 68/20) placed in `TOP`; `unit`
/// 1000 (1 nm) meets the units check of read-only reads.
inline Bytes two_cells(double unit = 100.0) {
  Oas f(false, 0, unit);
  f.cell("A").add(rect(68, 20, 0, 0, 10, 10));
  f.cell("TOP").add(place("A", 0, 0));
  return f.bytes();
}

}  // namespace oasis_command_test
