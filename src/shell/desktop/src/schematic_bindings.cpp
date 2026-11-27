// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic editor's default key bindings (schematic_bindings.hpp).
// The default keys are the ones schematic-editing "Desktop and touch
// interaction" lists. Owner: SHELL.

#include "hefesto/shell/schematic_bindings.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/shell/key_bindings_sheet.hpp"

namespace hefesto::shell {

namespace {

bool is_action(std::string_view text) {
  return std::find(kSchematicActions.begin(), kSchematicActions.end(), text) != kSchematicActions.end();
}

}  // namespace

const std::vector<DefaultBinding>& schematic_default_bindings() {
  static const std::vector<DefaultBinding> bindings = {
      {"w", "wire", "Draw wires"},
      {"m", "move", "Move the selection"},
      {"c", "copy", "Copy the selection"},
      {"R", "schematic rotate", "Rotate the selection"},
      {"F", "schematic flip", "Flip the selection"},
      {"e", "descend", "Descend into the selected instance"},
      {"Control_e", "schematic ascend", "Ascend to the parent schematic"},
      {"q", "properties", "Edit the selection's properties"},
      {"k", "schematic hilight selection", "Highlight the selected nets"},
      {"u", "schematic undo", "Undo"},
      {"U", "schematic redo", "Redo"},
      {"Insert", "place", "Place a symbol from the library"},
      {"n", "schematic netlist", "Netlist the schematic"},
      {"Delete", "schematic delete", "Delete the selection"},
  };
  return bindings;
}

void install_schematic_defaults(ui::MacroTables& tables) {
  for (const DefaultBinding& b : schematic_default_bindings()) {
    auto key = ui::parse_key_name(b.key);
    if (key) tables.define(kSchematicTable, key.value(), std::string{b.text}, std::string{b.help}, false);
  }
}

SchematicKeyMap::SchematicKeyMap() {
  install_schematic_defaults(own_);
}

void SchematicKeyMap::attach(ui::MacroTables& tables) {
  for (const DefaultBinding& b : schematic_default_bindings()) {
    auto key = ui::parse_key_name(b.key);
    if (key && tables.find(kSchematicTable, key.value()) == nullptr) {
      tables.define(kSchematicTable, key.value(), std::string{b.text}, std::string{b.help}, false);
    }
  }
  tables_ = &tables;
}

std::optional<BoundAction> SchematicKeyMap::lookup(const ui::Key& key) const {
  const ui::Macro* macro = tables_->find(kSchematicTable, key);
  if (macro == nullptr || macro->text.empty()) return std::nullopt;
  if (is_action(macro->text)) return BoundAction{macro->text, {}};
  return BoundAction{{}, macro->text};
}

std::optional<BoundAction> SchematicKeyMap::lookup(const ui::KeyEvent& event, bool macos) const {
  const auto key = ui::normalize_key_event(event, macos);
  return key ? lookup(*key) : std::nullopt;
}

std::vector<std::string> SchematicKeyMap::reset_commands() const {
  ui::MacroTables defaults;
  install_schematic_defaults(defaults);
  return binding_changes(*tables_, defaults, kSchematicTable);
}

}  // namespace hefesto::shell
