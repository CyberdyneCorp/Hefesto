// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session hook of the maze router (route/iroute/session.hpp): final
// glue; the work is MZTECH's derive_after_load. After a rescale, magic-compat
// ON drops the explicit spacings as Magic's MZAfterTech does after its first
// run; OFF carries the session's explicit values over, rescaled
// (Q-LEGACY-106).
//
// Ported from Magic VLSI 8.3.694 mzrouter/mzTech.c (MZAfterTech) and
// irouter/irMain.c (IRAfterTech), Copyright (C) 1988, 1990 Michael H.
// Arnold and the Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: IROUTE.

#include "hefesto/route/iroute/session.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::route {

void IrouteSession::reset_from(MazeStyles derived) {
  styles = std::move(derived);
  const MazeStyle* style = find_style(styles, "irouter");
  irouter = style == nullptr ? std::nullopt : std::optional<MazeStyle>{*style};
}

namespace {

// DRCTechScale's rounding of a distance: up; NIL (-1) and 0 are kept.
int scaled_value(int value, StyleRescale scale) {
  if (value <= 0 || scale.n <= 0) {
    return value;
  }
  const std::int64_t up = (static_cast<std::int64_t>(value) * scale.d + scale.n - 1) / scale.n;
  return static_cast<int>(std::min<std::int64_t>(up, std::numeric_limits<int>::max()));
}

// magic-compat OFF: the explicit values of the session's styles, rescaled,
// onto the written styles they came from (the same list, in file order; a
// derived `irouter` style comes last and has none).
void carry_scaled_statements(MazeStyles& styles, const MazeStyles& previous, StyleRescale scale) {
  const std::size_t count = std::min(styles.styles.size(), previous.styles.size());
  for (std::size_t i = 0; i < count; ++i) {
    MazeStyle& style = styles.styles[i];
    style.spacings = previous.styles[i].spacings;
    for (SpacingStatement& statement : style.spacings) {
      statement.value = scaled_value(statement.value, scale);
    }
    style.widths = previous.styles[i].widths;
    for (WidthStatement& statement : style.widths) {
      statement.width = scaled_value(statement.width, scale);
      statement.length = scaled_value(statement.length, scale);
    }
  }
}

void derive_and_install(editor::EditorState& editor, diag::DiagnosticSink& sink, MazeStyles styles) {
  derive_after_load(styles, *editor.technology, editor.drc.rules.get(), editor.lefdef.layers,
                    editor.drc.compat_settings().magic_compat, sink);
  editor.iroute.reset_from(std::move(styles));
}

}  // namespace

void refresh_session_styles(editor::EditorState& editor, diag::DiagnosticSink& sink) {
  derive_and_install(editor, sink, editor.technology->maze_styles());
}

void refresh_session_styles(editor::EditorState& editor, diag::DiagnosticSink& sink, StyleRescale scale) {
  MazeStyles styles = editor.technology->maze_styles();
  if (editor.drc.compat_settings().magic_compat) {
    // MZAfterTech freed ms_spacingL on its first run (Q-LEGACY-106 ON).
    for (MazeStyle& style : styles.styles) {
      style.spacings.clear();
    }
  } else {
    carry_scaled_statements(styles, editor.iroute.styles, scale);
  }
  derive_and_install(editor, sink, std::move(styles));
}

}  // namespace hefesto::route
