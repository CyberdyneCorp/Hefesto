// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session lifecycle of the LEF layer table (lef/tech/lifecycle.hpp).
//
// Ported from Magic VLSI 8.3.694 lef/lefTech.c (LefTechInit) and
// utils/tech.c (TechLoad's post-load LefTechScale and LefTechSetDefaults),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFTECH (design D2).

#include "hefesto/lef/tech/lifecycle.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::lef {

LayerTable read_layer_section(const tech::Technology& technology, const drc::RuleSet* lambda_rules,
                              const compat::CompatSettings& compat, diag::DiagnosticSink& sink) {
  // A repeated section is read again into the same table (Q-TECH-7).
  tech::RawSection all{"lef", {}};
  for (const tech::RawSection& raw : technology.raw_sections()) {
    if (raw.name == "lef") {
      all.lines.insert(all.lines.end(), raw.lines.begin(), raw.lines.end());
    }
  }
  return LayerTable::from_section(all, technology, lambda_rules, compat, sink);
}

void install_layer_table(editor::EditorState& editor, LayerTable table, const compat::CompatSettings& compat) {
  LefSession& lefdef = editor.lefdef;
  lefdef.layers = std::move(table);
  lefdef.vias = ViaTable{};
  lefdef.rules = RuleTable{};
  const bool one_to_one = editor.grid.internal == editor.grid.lambda;
  if (!one_to_one) {
    lefdef.layers.scale(static_cast<std::int32_t>(editor.grid.internal),
                        static_cast<std::int32_t>(editor.grid.lambda));
  }
  if (editor.drc.rules != nullptr && editor.technology != nullptr && (!compat.magic_compat || !one_to_one)) {
    lefdef.layers.fill_defaults(*editor.drc.rules, *editor.technology, compat);
  }
}

void refill_layer_table(editor::EditorState& editor, const compat::CompatSettings& compat) {
  if (compat.magic_compat || editor.drc.rules == nullptr || editor.technology == nullptr) {
    return;
  }
  editor.lefdef.layers.fill_defaults(*editor.drc.rules, *editor.technology, compat);
}

}  // namespace hefesto::lef
