// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editor-side extraction-style hook (openspec/changes/m7-extraction
// design.md D7, D20): after a technology is installed in the session, and
// after every grid rescale (`scalegrid`, finer-grid `.mag` reads and mask
// input reads, all through tech::scale_layout), the session's extraction
// style is rebuilt or rescaled here, so tech/ never learns about
// EditorState::extract. The hook calls EXTTECH's builders
// (extract::load_styles, StyleTable::select, ExtStyle::micron_convert and
// ExtStyle::rescale); until EXTTECH fills them (tasks.md 4.6) they answer
// an empty table, so EditorState::extract.current stays null.
//
// Owner: INTEGRATION (the hook and its call sites); EXTTECH (the builders
// it calls).

#pragma once

#include <cstdint>

#include "hefesto/diag/sink.hpp"

namespace hefesto::editor {

struct EditorState;

/// Rebuilds EditorState::extract.styles and .current from the session's
/// technology (its last `extract` section): the first style declared is
/// selected, micron-converted with the current cifoutput style and scaled
/// to the session grid; the `halo`/`stepsize` overrides are cleared
/// (Q-EXTRACT-25). Called by the tech loader after the technology, the
/// grid and the output styles are installed.
void install_extract_styles(EditorState& editor, diag::DiagnosticSink& sink);

/// Rescales the current style and the session overrides by n/d
/// (ExtTechScale). Called by tech::scale_layout, so every DBScaleEverything
/// site rescales the style once.
void rescale_extract_style(EditorState& editor, std::int64_t n, std::int64_t d);

}  // namespace hefesto::editor
