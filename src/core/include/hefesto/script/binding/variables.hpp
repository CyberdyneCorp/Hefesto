// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script variables read by the engine (openspec/changes/m4-scripting/
// design.md D6; capability scripting-api "Script variables read by the
// engine"; hefesto-settings "Global nets and power names" (D-14); Q-SCRIPT-5):
//
//   - `VDD`, `GND`, `SUB` and the array `globals` are traced into the
//     project setting "global nets and power names": they read as its
//     effective value, and a script's `set`/`unset` changes the session
//     layer (PDKINT moves the values a startup file set into that file's
//     layer with adopt_engine_variables);
//   - `$NAME` / `${NAME}` path expansion (search paths, file names, use
//     lines) reads the script variable first, then the session's previous
//     resolver (the environment / path-variable table, Q-CELL-17), with
//     CAD_ROOT's compiled default last (cell::PathVariables). With
//     magic-compat ON the first resolved value of each name is cached for
//     the session (Magic's expansion table, utils/path.c); a name that does
//     not resolve is retried on its next use. With it OFF each expansion
//     uses the current value. In both settings a name whose current value
//     differs from its first resolved value is warned about once
//     (SCRIPT-BIND-001).
//
// Owner: BINDING (design D2).

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/app/settings_stack.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

/// The traces on the engine-read variables of one interpreter.
class VariableTraces {
 public:
  /// Seeds `VDD`, `GND`, `SUB` and `globals` from `editor`'s effective
  /// setting, installs their read/write/unset traces, and routes
  /// `editor.path_variables` through the interpreter's variables and the
  /// Q-SCRIPT-5 cache (`editor.script.path_cache`; its warnings go to
  /// `report`). Idempotent per editor: the resolver of the session is wrapped
  /// once.
  [[nodiscard]] static cmd::CommandResult install(Interpreter& interp, editor::EditorState& editor,
                                                  std::function<void(const diag::Diagnostic&)> report = {});
};

/// Moves the values a script set in the session layer for `vdd`, `gnd`,
/// `sub` and `globalNets` into `layer` (hefesto-settings "Seeding from the
/// PDK": a startup file's variables seed the layer of that file). Called by
/// PDKINT after each startup file.
void adopt_engine_variables(app::SettingsStack& settings, app::SettingLayer layer);

/// The path-variable cache (Q-SCRIPT-5) and its warn-once set.
class PathVariableCache {
 public:
  using Resolver = std::function<std::optional<std::string>(std::string_view name)>;
  using Reporter = std::function<void(const diag::Diagnostic&)>;

  [[nodiscard]] std::optional<std::string> lookup(std::string_view name) const;
  void store(std::string_view name, std::string value);
  void clear() noexcept;
  /// True the first time it is called for `name` (the warning is due).
  [[nodiscard]] bool first_warning(std::string_view name);

  /// One expansion of `name` whose current value is `live`: records the
  /// first resolved value, warns once (through the reporter) when `live`
  /// differs from it, and returns the first value (magic-compat ON) or
  /// `live` (OFF). An unresolved name is not cached.
  [[nodiscard]] std::optional<std::string> resolve(std::string_view name, const std::optional<std::string>& live,
                                                   bool magic_compat);

  /// The resolver the session had before the binding wrapped it (the
  /// environment), set once by VariableTraces::install.
  [[nodiscard]] const std::optional<Resolver>& base() const noexcept { return base_; }
  void set_base(Resolver resolver) { base_ = std::move(resolver); }
  void set_reporter(Reporter reporter) { reporter_ = std::move(reporter); }

 private:
  std::map<std::string, std::string, std::less<>> values_;
  std::set<std::string, std::less<>> warned_;
  std::optional<Resolver> base_;
  Reporter reporter_;
};

}  // namespace hefesto::script
