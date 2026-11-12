// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Technology loading, resolution and scaling (capability
// tech-loading-and-scaling): file-name resolution, the readability
// pre-check, full and partial loads, post-load adjustments, built-in
// technologies and the grid-limit check.
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::tech {

struct LoadOptions {
  bool magic_compat = true;                 // the project's magic-compat setting
  /// "File search rules": the system library path, searched after ".".
  std::vector<platform::Path> search_path;
  /// The system library path as the session holds it (`path sys`, with its
  /// `$CAD_ROOT` words unexpanded), printed after "Could not find file" as
  /// Magic prints SysLibPath; empty: "." and `search_path`.
  std::string search_path_text;
  /// Suffix appended to `include` names: ".tech", or ".tech27" when the file
  /// was found through the legacy fallback (resolve_tech_file sets it).
  std::string include_suffix = ".tech";
};

/// "Technology file name resolution": `X` verbatim when its base name has a
/// non-`.tech` extension, then `X.tech`, then `X.tech27`, each through the
/// file search rules. Fails with TECH-FILE-001. `include_suffix`, when
/// given, receives the include suffix of this load (Q-TECH-1).
[[nodiscard]] diag::Outcome<platform::Path> resolve_tech_file(platform::FileSystem& fs, std::string_view name,
                                                              const LoadOptions& options,
                                                              std::string* include_suffix = nullptr);

/// "Readability pre-check": the file opens and its first logical line is
/// exactly `tech`. Fails with TECH-FILE-002.
[[nodiscard]] diag::Status precheck_tech_file(platform::FileSystem& fs, const platform::Path& path);

/// "Full technology load" of an already resolved file: read, apply every
/// section, finalize. Diagnostics go to `sink`; a failed load returns
/// TECH-LOAD-001 and the caller keeps the previous technology (Q-TECH-10:
/// loads are atomic). Post-load adjustments are separate
/// (apply_post_load), because they change session state.
[[nodiscard]] diag::Outcome<std::shared_ptr<const Technology>> load_technology(platform::FileSystem& fs,
                                                                               const platform::Path& path,
                                                                               const LoadOptions& options,
                                                                               diag::DiagnosticSink& sink);

/// The same from text already in memory; `file_name` labels diagnostics and
/// becomes Technology::file_name(), `base_dir` is where `include` looks last.
[[nodiscard]] diag::Outcome<std::shared_ptr<const Technology>> load_technology_text(
    std::string_view text, const platform::Path& file_name, platform::FileSystem& fs, const LoadOptions& options,
    diag::DiagnosticSink& sink);

/// "Partial section reload": re-reads `sections` of `current`'s file from
/// disk and keeps every other section of `current`. No post-load steps.
[[nodiscard]] diag::Outcome<std::shared_ptr<const Technology>> reload_sections(
    platform::FileSystem& fs, const Technology& current, const std::vector<std::string>& sections,
    const LoadOptions& options, diag::DiagnosticSink& sink);

/// "Post-load adjustments" steps 1 and 2 for a successful full load: the
/// minimum-grid rule of the CIF output style, the `Scaled tech values`
/// report and the grid-limit warning. Returns the session's new grid
/// scale. (No M1 table is scale-dependent: the cifoutput/drc/extract/...
/// sections are kept raw and are scaled by their owners in later
/// milestones.)
[[nodiscard]] GridScale apply_post_load(const Technology& tech, GridScale grid, diag::DiagnosticSink& sink);

/// "Built-in technologies": `minimum`, used when none is loaded. Never
/// fails; file_name() is empty.
[[nodiscard]] std::shared_ptr<const Technology> builtin_technology();

/// The text of the built-in `minimum` technology.
[[nodiscard]] std::string_view builtin_technology_text() noexcept;

/// "Grid limit check" for a scale change by n/d against the technology's
/// CIF output style; passes when there is none. Fails with TECH-GRID-002.
/// `current` is the session's grid scaling: Magic scales the CIF output
/// style with every `scalegrid` and with the tech load's rescale
/// (CIFTechOutputScale), so the limit applies to the scaled style.
[[nodiscard]] diag::Status check_grid_limit(const Technology& tech, std::int64_t n, std::int64_t d,
                                            const GridScale& current = GridScale{});

/// Reduces a fraction (utils/fraction.c): gcd divided out. A non-positive
/// part yields 1:1.
[[nodiscard]] GridScale reduce(GridScale scale) noexcept;

}  // namespace hefesto::tech
