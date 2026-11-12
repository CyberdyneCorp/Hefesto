// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A scripted session for the script/pdk tests: every engine command through
// the dispatcher on the local disk, a startup window on "(UNNAMED)", and the
// project's Tcl interpreter with the engine commands bound.
//
// Binding: script::bind_registry (BINDING, tasks.md 5.1) when it is
// implemented. Until then the fixture binds a stand-in: each registry
// command as `::magic::<name>` and, when Tcl has no command of that name,
// globally; a failed command returns ok with an empty result (Magic's quiet
// non-strict dispatch) unless it raises. Owner: PDKINT.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "reference_data.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/pdk/commands.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::script::test {

namespace fs = std::filesystem;

/// A fresh directory under the system temporary directory, removed at scope end.
class TempDir {
 public:
  TempDir() {
    std::random_device device;
    path_ = fs::temp_directory_path() / ("hefesto-pdkint-" + std::to_string(device()) + std::to_string(device()));
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

 private:
  fs::path path_;
};

inline void write_file(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary};
  out << text;
}

/// Sets (or unsets) a process environment variable for the scope (the
/// interpreter's `env` is synthesized, D-31; this is for host lookups).
class ScopedEnv {
 public:
  ScopedEnv(const char* name, const std::optional<std::string>& value) : name_(name) {
    if (const char* old = std::getenv(name)) {
      old_ = old;
    }
    set(value);
  }
  ~ScopedEnv() { set(old_); }
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

 private:
  void set(const std::optional<std::string>& value) {
    if (value) {
      ::setenv(name_, value->c_str(), 1);
    } else {
      ::unsetenv(name_);
    }
  }
  const char* name_;
  std::optional<std::string> old_;
};

/// The pinned PDK's ciel version directory (the PDK_ROOT its rc file
/// expects), or nullopt when the reference data is not fetched.
inline std::optional<fs::path> pinned_pdk_root(const hefesto::test::PinnedPdk& pdk) {
  const auto variant = hefesto::test::pinned_variant(pdk);
  return variant ? std::optional{variant->parent_path()} : std::nullopt;
}

/// A synthesized `env` holding PDK_ROOT (none when `root` is nullopt).
inline std::map<std::string, std::string, std::less<>> pdk_env(const std::optional<std::string>& root) {
  std::map<std::string, std::string, std::less<>> env;
  if (root) {
    env.emplace("PDK_ROOT", *root);
  }
  return env;
}

struct PdkSession {
  platform::PortableServices services{platform::PortableOptions{{}, {}, false, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script_session{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  /// Diagnostics of the commands the stand-in binding ran.
  std::vector<diag::Diagnostic> console;
  bool stand_in_binding = false;

  /// `environment`: the interpreter's synthesized `env` array (D-31). The
  /// granted roots are the PDK reference data and the temporary directory.
  /// `bind_commands` false: no engine command is bound (tests of hooks the
  /// binding would install as well).
  explicit PdkSession(std::map<std::string, std::string, std::less<>> environment = {}, bool bind_commands = true) {
    REQUIRE(cmd::register_all_commands(registry));
    if (registry.find("ext2spice") == nullptr) {
      REQUIRE(register_pdk_commands(registry));  // until INTEGRATION registers it
    }
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script_session);
    dispatcher->set_editor(&editor);
    open_startup_window();
    SandboxPolicy policy;
    policy.granted_roots = {fs::path{HEFESTO_SOURCE_DIR} / ".cache" / "pdks", fs::temp_directory_path()};
    policy.environment = std::move(environment);
    (void)open_project_interpreter(editor, std::move(policy));
    if (bind_commands) {
      bind();
    }
  }

  [[nodiscard]] Interpreter& tcl() { return *editor.script.interp; }

  /// Evaluates `script`; REQUIREs success and returns the result's string form.
  std::string eval(std::string_view script) {
    const auto result = tcl().evaluate(script);
    INFO(script);
    INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text));
    REQUIRE(result.ok());
    return result.value.to_string();
  }

  [[nodiscard]] bool console_has(std::string_view text) const {
    for (const auto& d : console) {
      if (d.text.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

 private:
  // Magic's startup window on "(UNNAMED)" (app startup's adopt_unnamed_edit_cell).
  void open_startup_window() {
    db::CellDef* unnamed = editor.cells.create("(UNNAMED)", editor.technology->plane_count()).value();
    unnamed->flags().available = true;
    editor.edit_cell = unnamed;
    editor.edit_root = unnamed;
    REQUIRE(editor.windows.open_frame(unnamed).ok());
  }

  void bind() {
    const auto bound = bind_registry(tcl(), registry, *dispatcher);
    if (bound.ok()) {
      return;
    }
    REQUIRE_FALSE(bound.diagnostics.empty());
    REQUIRE(bound.diagnostics.front().code == diag::codes::kCoreNotImplemented);
    stand_in_binding = true;
    std::set<std::string> names;
    for (const auto& client : registry.clients()) {
      for (const cmd::CommandSpec* spec : registry.table(client)) {
        names.insert(spec->name);
      }
    }
    for (const std::string& name : names) {
      const NativeCommand command = [this, name](const cmd::Invocation& inv) {
        cmd::CommandResult result =
            dispatcher->execute(cmd::Invocation{name, inv.args, cmd::Origin::script, std::nullopt});
        console.insert(console.end(), result.diagnostics.begin(), result.diagnostics.end());
        if (!result.ok() && result.raises) {
          return result;
        }
        return cmd::CommandResult::success(result.ok() ? result.value : cmd::Value{});
      };
      tcl().define_command("::magic::" + name, command);
      if (!tcl().has_command("::" + name)) {
        tcl().define_command("::" + name, command);
      }
    }
  }
};

}  // namespace hefesto::script::test
