// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A command session for the `extract` tests (tests/unit/extract/command and
// the extract_cell_*_test.cpp files of tests/unit/extract/hier): the
// dispatcher with every command, the EX cases' exttest.tech loaded from a
// directory, and `.mag` text written next to it. In memory by default; a
// session on the local disk works in a fresh directory under the test's
// current directory (symbolic links, read-only directories).
//
// Owner: EXTCMD.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/write/subtree.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"
#include "hefesto/view/window.hpp"

namespace extcmd_test {

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;

inline std::string read_source(const std::string& relative) {
  std::ifstream in(std::filesystem::path{HEFESTO_SOURCE_DIR} / relative, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// A file of EX case `id`'s input directory.
inline std::string case_input(const std::string& id, const std::string& name) {
  return read_source("tests/golden/EXTRACT/cases/" + id + "/input/" + name);
}

/// A file of EX case `id`'s collected expected output.
inline std::string case_expected(const std::string& id, const std::string& name) {
  return read_source("tests/golden/EXTRACT/cases/" + id + "/expected/collected/input/" + name);
}

/// A `.mag` file of the exttest technology with `body` (paint, labels,
/// uses) and timestamp 100.
inline std::string mag(std::string_view body) {
  return "magic\ntech exttest\ntimestamp 100\n" + std::string{body} + "<< end >>\n";
}

/// A use record of `def` (id `id`) translated by (x, y), timestamp 100.
inline std::string use(std::string_view def, std::string_view id, int x = 0, int y = 0) {
  return "use " + std::string{def} + " " + std::string{id} + "\ntimestamp 100\ntransform 1 0 " + std::to_string(x) +
         " 0 1 " + std::to_string(y) + "\nbox 0 0 1 1\n";
}

/// A fresh directory under the current directory (local-disk sessions).
inline std::filesystem::path fresh_directory(std::string_view tag) {
  static std::atomic<int> counter{0};
  const std::filesystem::path dir = std::filesystem::current_path() /
                                    ("extract_cmd_" + std::string{tag} + "_" + std::to_string(counter++));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

struct Session {
  platform::PortableServices services;
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  diag::CollectingSink printed;
  std::string dir;
  std::unique_ptr<cmd::CommandContext> context_;

  /// `root`: the directory the technology and the cells live in (an
  /// absolute path; on the local disk, a fresh directory when empty).
  explicit Session(bool magic_compat = true, bool in_memory = true, std::string root = "/w",
                   const std::string& tech_text = case_input("EX-026", "exttest.tech"))
      : services(platform::PortableOptions{{}, {}, in_memory, true}), dir(std::move(root)) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(printed);
    if (!in_memory && dir.empty()) {
      dir = fresh_directory("disk").string();
    }
    file("exttest.tech", tech_text);
    REQUIRE(run("tech load " + dir + "/exttest -noprompt").ok());
    editor.drc.compat = &project.compat;
    REQUIRE(run("addpath " + dir).ok());
    editor.command_point = cmd::PointerState{};
    (void)printed.take();
  }

  [[nodiscard]] platform::FileSystem& files() { return services.view().files; }

  /// A command context of the session, for the driver's API (its subtree
  /// loader reads unread cells as a command does).
  cmd::CommandContext& context() {
    if (!context_) {
      context_ = std::make_unique<cmd::CommandContext>(services.view(), project, script, printed, registry, &editor);
    }
    return *context_;
  }

  [[nodiscard]] hefesto::gds::SubtreeLoader loader() { return hefesto::gds::command_subtree_loader(context(), editor); }

  /// Writes `text` to `name` in the session's directory.
  void file(const std::string& name, const std::string& text) {
    (void)services.view().files.create_directories(dir);
    REQUIRE(services.view().files.write_atomic(dir + "/" + name, text).ok());
  }

  [[nodiscard]] std::string read(const std::string& name) {
    auto text = services.view().files.read(dir + "/" + name);
    REQUIRE(text.ok());
    return text.value();
  }

  [[nodiscard]] bool exists(const std::string& name) { return services.view().files.exists(dir + "/" + name); }

  /// Runs a `;`-separated line; returns the first failure, else the last
  /// result.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  /// Writes `<cell>.mag` and loads it with a box (the window check).
  void load(const std::string& cell, const std::string& text) {
    file(cell + ".mag", text);
    open(cell);
  }

  /// Loads `cell` (from the search path) into a window with a box.
  void open(const std::string& cell) {
    REQUIRE(run("load " + cell).ok());
    if (editor.windows.current() == nullptr) {
      // The window whose root `extract` takes (windCheckOnlyWindow).
      REQUIRE(editor.windows.open(editor.cells.find(cell), hefesto::view::ScreenRect{0, 0, 99, 99}).ok());
    }
    REQUIRE(run("box 0 0 1 1").ok());
    (void)printed.take();
  }

  /// The file a run reported for `cell` ("Extracting <cell> into <file>:").
  [[nodiscard]] static std::string output_of(const std::string& printed_text, const std::string& cell) {
    const std::string head = "Extracting " + cell + " into ";
    const std::size_t at = printed_text.find(head);
    REQUIRE(at != std::string::npos);
    const std::size_t from = at + head.size();
    return printed_text.substr(from, printed_text.find(":\n", from) - from);
  }

  /// The printed texts since the last call, one per line.
  std::string take_printed() {
    std::string text;
    for (const auto& d : printed.take()) {
      text += d.text + "\n";
    }
    return text;
  }
};

/// The lines of `text` that start with `prefix`.
inline std::vector<std::string> lines_with(const std::string& text, std::string_view prefix) {
  std::vector<std::string> out;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (line.starts_with(prefix)) {
      out.push_back(line);
    }
  }
  return out;
}

}  // namespace extcmd_test
