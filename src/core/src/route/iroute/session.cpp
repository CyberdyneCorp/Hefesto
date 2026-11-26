// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session hook of the maze router (route/iroute/session.hpp): final
// glue; the work is MZTECH's derive_after_load.
//
// Owner: IROUTE.

#include "hefesto/route/iroute/session.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::route {

void IrouteSession::reset_from(MazeStyles derived) {
  styles = std::move(derived);
  const MazeStyle* style = find_style(styles, "irouter");
  irouter = style == nullptr ? std::nullopt : std::optional<MazeStyle>{*style};
}

void refresh_session_styles(editor::EditorState& editor, diag::DiagnosticSink& sink) {
  MazeStyles styles = editor.technology->maze_styles();
  derive_after_load(styles, *editor.technology, editor.drc.rules.get(), editor.lefdef.layers,
                    editor.drc.compat_settings().magic_compat, sink);
  editor.iroute.reset_from(std::move(styles));
}

}  // namespace hefesto::route
