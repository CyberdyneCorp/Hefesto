// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands of the TECH area, registered by cmd::register_all_commands():
//   tech <option> ...   capability tech-command (name, filename, version,
//                       lambda, planes, layers, locked/unlocked/revert, drc,
//                       load, help)
//   scalegrid a b       capability tech-loading-and-scaling, "scalegrid command"
// Handlers reach the session through CommandContext::editor().
//
// Also the session-level load used by `tech load` and by startup
// (app-startup), so both apply the same pre-check, post-load adjustments
// and layout check.
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/services.hpp"

namespace hefesto::db {
class CellLibrary;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::tech {

/// Adds the TECH commands to `registry`. Returns false if a name was taken.
bool register_tech_commands(cmd::CommandRegistry& registry);

struct SessionLoadOptions {
  bool magic_compat = true;
  /// "Readability pre-check" (`tech load`, `-T`, startup default); off for
  /// automatic loads triggered by reading a cell.
  bool precheck = true;
};

/// A full technology load into the session: resolution ("." then the
/// platform PDK store), optional pre-check, the atomic load, "Post-load
/// adjustments" (grid) and "Effect on existing layout" (warnings). On
/// success the session's technology, grid and locked types are replaced;
/// on failure nothing changes. Errors: TECH-FILE-001/002, TECH-LOAD-001.
[[nodiscard]] diag::Status load_session_technology(editor::EditorState& editor,
                                                   platform::PlatformServices& services, std::string_view name,
                                                   const SessionLoadOptions& options, diag::DiagnosticSink& sink);

/// "Effect on existing layout" paint check: a non-internal cell has a tile
/// on a plane in 3..plane_limit-1.
[[nodiscard]] bool layout_has_paint(const db::CellLibrary& cells, std::size_t plane_limit);

/// The rescaling of "scalegrid command" step 6: every coordinate of every
/// available cell multiplied by n/d (floor, infinities kept). Returns
/// whether any value did not scale exactly.
bool scale_layout(editor::EditorState& editor, std::int64_t n, std::int64_t d);

/// DBScaleValue: floor(v*n/d) in 64-bit arithmetic; |v| >= 2^30-6 is kept.
[[nodiscard]] std::int64_t scale_value(std::int64_t v, std::int64_t n, std::int64_t d) noexcept;

}  // namespace hefesto::tech
