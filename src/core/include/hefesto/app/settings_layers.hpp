// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Filling the settings layers (capability hefesto-settings "Layered
// settings resolution", "Project files", "Global nets and power names",
// "Magic startup-file compatibility", "Preference sync" export/import):
// built-in defaults, PDK and user startup-file settings, user preferences,
// project file and session overrides, and the hand-over of the effective
// compatibility settings to settings::ProjectSettings (the typed store that
// command handlers read through CommandContext::project()).
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/app/project_file.hpp"
#include "hefesto/app/settings_stack.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::app {

// Setting names of "Global nets and power names" in the stack. `globalNets`
// holds the names separated by single spaces, in ascending byte order.
inline constexpr std::string_view kGlobalNets = "globalNets";
inline constexpr std::string_view kVdd = "vdd";
inline constexpr std::string_view kGnd = "gnd";
inline constexpr std::string_view kSub = "sub";
/// The PDK root chosen in the settings ("Install layout compatible with ciel", step 1).
inline constexpr std::string_view kPdkRoot = "pdk-root";

/// Built-in layer: magic-compat on, netgen-compat on, script-strict off,
/// vdd `vdd!`, gnd `gnd!` (Magic's system startup file values), no globals.
void install_builtin_defaults(SettingsStack& stack);

/// Project layer from a project file (typed settings as on/off, other
/// settings verbatim, the set fields of the global nets).
void apply_project(SettingsStack& stack, const ProjectFile& project);

/// Copies the effective magic-compat, netgen-compat and script-strict into
/// the typed project settings.
void apply_effective(const SettingsStack& stack, settings::ProjectSettings& typed);

/// "Magic startup-file compatibility" before the Tcl binding (M4): the plain
/// settings of a startup file, i.e. top-level `set VDD|GND|SUB <word>` and
/// `set globals(<name>) <word>` lines whose value is a literal (no `$`, `[`).
/// Everything else (procedures, conditionals, commands) is script and is
/// not run. Returns setting name/value pairs in file order; globals are
/// folded into one sorted `globalNets` entry.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> startup_file_settings(std::string_view text);

/// User preferences file (export/import format): a flat JSON object of
/// string values, keys in byte order. A missing file is an empty layer;
/// a malformed one fails with APP-SETTINGS-001.
[[nodiscard]] diag::Status load_preferences(platform::FileSystem& fs, const platform::Path& path,
                                            SettingsStack& stack);
[[nodiscard]] diag::Status save_preferences(platform::FileSystem& fs, const platform::Path& path,
                                            const SettingsStack& stack);

}  // namespace hefesto::app
