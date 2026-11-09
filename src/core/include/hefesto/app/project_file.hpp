// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `hefesto.project` file (capability hefesto-settings "Project files";
// PDK pin per hefesto-pdk-manager "Per-project version pinning"). Text with
// a deterministic key order so it diffs well. Extends the M0 project
// settings JSON (settings::ProjectSettings), which it embeds unchanged.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace hefesto::app {

inline constexpr std::string_view kProjectFileName = "hefesto.project";

struct PdkPin {
  std::string family;
  std::string variant;
  std::string version;                  // full 40-hex hash
  std::vector<std::string> libraries;

  friend bool operator==(const PdkPin&, const PdkPin&) = default;
};

/// "Global nets and power names" (hefesto-settings): an unset field leaves
/// the lower settings layer's value in place.
struct GlobalNets {
  std::optional<std::vector<std::string>> global_nets;  // `globalNets`, unique, exact
  std::optional<std::string> vdd;
  std::optional<std::string> gnd;
  std::optional<std::string> sub;

  friend bool operator==(const GlobalNets&, const GlobalNets&) = default;
};

struct ProjectFile {
  std::optional<PdkPin> pdk;
  std::vector<std::string> cell_search_paths;      // relative to the project folder
  settings::ProjectSettings settings;              // magic-compat, netgen-compat, script-strict
  std::map<std::string, std::string> extra_settings;  // other project-layer settings, sorted
  GlobalNets nets;                                 // "Global nets and power names"
  std::vector<std::string> open_documents;
  std::string window_layout;                       // opaque shell state (JSON text; empty = none)

  friend bool operator==(const ProjectFile&, const ProjectFile&) = default;
};

/// The file text: JSON, keys in byte order, two-space indent, final newline,
/// so equal projects give identical bytes ("Project files": deterministic
/// key order). Shape:
///   {"cellSearchPaths":[...],"documents":[...],"format":"hefesto.project",
///    "globalNets":{"globalNets":[...],"gnd":..,"sub":..,"vdd":..},
///    "pdk":{"family","libraries","variant","version"},
///    "settings":{"magic-compat":true,...,"<name>":"<value>"},
///    "version":1,"windowLayout":{...}}
[[nodiscard]] std::string to_text(const ProjectFile& project);
/// Parses the file text; unknown top-level keys are ignored (newer
/// writers), malformed values fail with APP-PROJECT-001.
[[nodiscard]] diag::Outcome<ProjectFile> parse_project(std::string_view text, std::string_view source_name = {});
[[nodiscard]] diag::Outcome<ProjectFile> load_project(platform::FileSystem& fs, const platform::Path& folder);
[[nodiscard]] diag::Status save_project(platform::FileSystem& fs, const platform::Path& folder,
                                        const ProjectFile& project);

}  // namespace hefesto::app
