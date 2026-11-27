// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `iroute` session state (capability router-maze-iroute "iroute
// availability and dispatch"; openspec/changes/m10-plot-route/design.md D4,
// D12, "Area contracts" IROUTE): the styles after derive_after_load, the
// session copy of the `irouter` style that `iroute` subcommands change
// (with its verbosity) and the `wizard window` setting. Held by
// EditorState::iroute.
//
// refresh_session_styles is Magic's MZAfterTech + IRAfterTech: the session
// calls it after every technology load (tech::load_session_technology) and
// after every rescale that runs DRCTechScale (`scalegrid`, a finer-grid
// `.mag` read, and a CIF or GDS read that redefines the grid), so tech/
// never learns about the router's state. Magic frees each style's explicit
// `spacing` list after the first MZAfterTech, so with magic-compat ON a
// rescale derives the spacings from the DRC rules alone. With it OFF the
// explicit `spacing` and `width` values follow the grid as the DRC
// distances do (multiplied by d/n and rounded up) and are applied again
// (Q-LEGACY-106).
//
// Owner: IROUTE (refresh_session_styles calls MZTECH's derive_after_load).

#pragma once

#include <cstdint>
#include <optional>

#include "hefesto/diag/sink.hpp"
#include "hefesto/route/tech/styles.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::route {

struct IrouteSession {
  /// Every style of the technology after derive_after_load.
  MazeStyles styles;
  /// The session copy of the `irouter` style (or of the derived one), with
  /// the `iroute verbosity` and wizard settings (Magic's irMazeParms);
  /// nullopt when the technology has neither (`iroute` then reports
  /// Magic's missing-style error).
  std::optional<MazeStyle> irouter;
  /// `iroute wizard window`: the reference window id; nullopt = the window
  /// under the pointer (Magic's irRouteWid = -1).
  std::optional<int> wizard_window;
  /// The derived-style notice is printed once per session (D13).
  bool derived_notice_shown = false;

  /// Replaces `styles` and re-copies `irouter` from them.
  void reset_from(MazeStyles derived);
};

/// A rescale as DRCTechScale(n, d) applies it: distances become value * d / n.
struct StyleRescale {
  std::int64_t n = 1;
  std::int64_t d = 1;
};

/// MZAfterTech + IRAfterTech for `editor` after a technology load (see the
/// file comment): the technology's written styles, derived once.
void refresh_session_styles(editor::EditorState& editor, diag::DiagnosticSink& sink);

/// MZAfterTech + IRAfterTech for `editor` after the rescale `scale`, once
/// the DRC rules and the LEF table are rescaled (see the file comment).
void refresh_session_styles(editor::EditorState& editor, diag::DiagnosticSink& sink, StyleRescale scale);

}  // namespace hefesto::route
