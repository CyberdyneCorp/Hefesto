// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The headless view model of the schematic tool palette, the touch context
// menu and the desktop key bindings (capability schematic-editing "Desktop
// and touch interaction", Q-SCHEM-24 `Resolution: platform`; the iPad
// presentation is M14). Every input path resolves to an EditAction, and an
// action resolves to the same `schematic` commands whichever path chose it,
// so a palette tap, a long-press menu entry and a key issue identical
// commands ("Rotate on iPad", "Wire from the palette on iPad").
//
// Owner: EDIT.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/schem/edit/wires.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

enum class EditAction : std::uint8_t {
  select,
  wire,
  place_symbol,
  move,
  copy,
  rotate,
  flip,
  delete_selection,
  edit_properties,
  descend,
  ascend,
  highlight,
  undo,
  redo,
  netlist,
};

/// The floating palette, in display order: every operation listed by
/// "Desktop and touch interaction" (select, wire, place-symbol, move, copy,
/// rotate, flip, delete, edit-properties, descend, ascend, highlight, undo,
/// redo, netlist).
[[nodiscard]] std::span<const EditAction> palette_actions() noexcept;

/// The long-press context menu: move, copy, rotate, flip, delete, edit
/// properties, descend and highlight.
[[nodiscard]] std::span<const EditAction> context_menu_actions() noexcept;

/// The action of an xschem-style default desktop binding (`w`, `m`, `c`,
/// `R`, `F`, `e`, `Ctrl+e`, `q`, `k`, `u`, `U`, `Insert`, `n`, `Delete`);
/// nullopt for any other key.
[[nodiscard]] std::optional<EditAction> desktop_binding(std::string_view key) noexcept;

/// The palette label of an action ("Wire", "Place symbol", ...).
[[nodiscard]] std::string_view action_label(EditAction action) noexcept;

/// Where an action applies: the pointer (or touch, or Pencil) positions in
/// document coordinates, before snapping.
struct ActionInput {
  Point start;    // where the click, tap or drag started
  Point pointer;  // where it is now (or ended)
  double snap = 10.0;
  WireRoute route = WireRoute::horizontal_first;
  std::string symbol;    // place_symbol: the symbol reference chosen
  std::string instance;  // descend: the instance name
};

/// The `schematic` commands (each the arguments after `schematic`) that
/// `action` issues for `input`; positions are snapped to `input.snap`.
/// `select` and `edit_properties` change the tool or open a dialog and
/// issue nothing by themselves.
[[nodiscard]] std::vector<std::vector<std::string>> action_commands(EditAction action, const ActionInput& input);

/// A coordinate as the commands write it (`%.16g`, as the file format).
[[nodiscard]] std::string coordinate_text(double value);

}  // namespace hefesto::schem
