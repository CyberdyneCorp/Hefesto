// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session lifecycle of the LEF layer table (capability
// lef-tech-section, requirements "LEF table lifecycle", "Scaling the LEF
// table" and "Filling defaults after grid scaling"; design D5, D17): the
// hooks the technology load and the grid rescales call.
//
// Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lef {

/// Reads every `lef` section of `technology` into a new table while the
/// technology is read (TechLoad's LefTechInit and LefTechLine), with the
/// section's messages to `sink`. `lambda_rules` is the DRC style as read,
/// at the technology's own grid (nullptr: no rules).
[[nodiscard]] LayerTable read_layer_section(const tech::Technology& technology, const drc::RuleSet* lambda_rules,
                                            const compat::CompatSettings& compat, diag::DiagnosticSink& sink);

/// Makes `table` the session's table after a successful technology load,
/// once editor.grid and editor.drc are installed: it replaces the layer
/// table, clears the session's VIA definitions and non-default rules, then
/// TechLoad's post-load LefTechScale to editor.grid and the default fill
/// (magic-compat ON: only when the grid is not 1:1, LefTechSetDefaults;
/// OFF: always).
void install_layer_table(editor::EditorState& editor, LayerTable table, const compat::CompatSettings& compat);

/// The default fill after a grid change (`scalegrid`, finer-grid reads),
/// once editor.drc holds the rescaled rules: nothing with magic-compat ON
/// (Magic never refills), every unset value with it OFF.
void refill_layer_table(editor::EditorState& editor, const compat::CompatSettings& compat);

}  // namespace hefesto::lef
