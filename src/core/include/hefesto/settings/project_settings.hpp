// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Per-project settings store: the compatibility settings, `portable-netlist-paths`
// (capability schematic-netlisting, requirement "Netlist file structure";
// Q-SCHEM-13) and `script-strict` (capability hefesto-diagnostics,
// requirement "Script strict mode switch"). All are independent booleans
// addressed by their spec names (capability hefesto-settings "Project files").

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::settings {

inline constexpr std::string_view kMagicCompat = "magic-compat";
inline constexpr std::string_view kNetgenCompat = "netgen-compat";
inline constexpr std::string_view kXschemCompat = "xschem-compat";
inline constexpr std::string_view kPortableNetlistPaths = "portable-netlist-paths";
inline constexpr std::string_view kScriptStrict = "script-strict";

struct ProjectSettings {
  compat::CompatSettings compat;         // magic-compat, netgen-compat, xschem-compat: ON
  bool portable_netlist_paths = false;   // schematic netlists: portable `** sch_path:` lines
  bool script_strict = false;            // initial strict state of each new interpreter

  friend bool operator==(const ProjectSettings&, const ProjectSettings&) = default;
};

/// Setting names in a fixed order: magic-compat, netgen-compat, xschem-compat,
/// portable-netlist-paths, script-strict.
[[nodiscard]] std::span<const std::string_view> setting_names() noexcept;

/// Accepts on/off, true/false, yes/no, 1/0 (case-sensitive, lower case).
[[nodiscard]] std::optional<bool> parse_bool(std::string_view text) noexcept;

/// Reads a setting by name; SETTINGS-NAME-001 when the name is unknown.
[[nodiscard]] diag::Outcome<bool> get(const ProjectSettings& settings, std::string_view name);

/// Sets one setting by name and leaves the others unchanged.
/// SETTINGS-NAME-001 for an unknown name, SETTINGS-VALUE-001 for a bad value.
[[nodiscard]] diag::Status set(ProjectSettings& settings, std::string_view name, std::string_view value);

/// JSON form stored in the project: {"magic-compat":true,...} with keys in
/// setting_names() order.
[[nodiscard]] std::string to_json(const ProjectSettings& settings);

/// Parses the JSON form. Missing keys keep their defaults; unknown keys and
/// non-boolean values yield SETTINGS-FILE-001.
[[nodiscard]] diag::Outcome<ProjectSettings> from_json(std::string_view text, std::string_view source_name = {});

/// Loads and saves the JSON form through the platform file system.
[[nodiscard]] diag::Outcome<ProjectSettings> load(platform::FileSystem& fs, const std::filesystem::path& path);
[[nodiscard]] diag::Status save(platform::FileSystem& fs, const std::filesystem::path& path,
                                const ProjectSettings& settings);

}  // namespace hefesto::settings
