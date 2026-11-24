// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The palette, context menu and desktop binding view model (palette.hpp;
// capability schematic-editing "Desktop and touch interaction",
// Q-SCHEM-24).
//
// Owner: EDIT.

#include "hefesto/schem/edit/palette.hpp"

#include <array>
#include <cstdio>

#include "hefesto/schem/edit/edit_state.hpp"

namespace hefesto::schem {

namespace {

constexpr std::array kPalette{EditAction::select,      EditAction::wire,         EditAction::place_symbol,
                              EditAction::move,        EditAction::copy,         EditAction::rotate,
                              EditAction::flip,        EditAction::delete_selection, EditAction::edit_properties,
                              EditAction::descend,     EditAction::ascend,       EditAction::highlight,
                              EditAction::undo,        EditAction::redo,         EditAction::netlist};

constexpr std::array kContextMenu{EditAction::move,           EditAction::copy,       EditAction::rotate,
                                  EditAction::flip,           EditAction::delete_selection,
                                  EditAction::edit_properties, EditAction::descend,   EditAction::highlight};

struct Binding {
  std::string_view key;
  EditAction action;
};

// xschem's default keys for the operations of this capability.
constexpr std::array kBindings{
    Binding{"w", EditAction::wire},       Binding{"m", EditAction::move},
    Binding{"c", EditAction::copy},       Binding{"R", EditAction::rotate},
    Binding{"F", EditAction::flip},       Binding{"e", EditAction::descend},
    Binding{"Ctrl+e", EditAction::ascend}, Binding{"q", EditAction::edit_properties},
    Binding{"k", EditAction::highlight},  Binding{"u", EditAction::undo},
    Binding{"U", EditAction::redo},       Binding{"Insert", EditAction::place_symbol},
    Binding{"n", EditAction::netlist},    Binding{"Delete", EditAction::delete_selection},
};

constexpr std::array<std::string_view, 15> kLabels{
    "Select", "Wire",    "Place symbol", "Move",      "Copy", "Rotate", "Flip",    "Delete",
    "Edit properties", "Descend", "Ascend", "Highlight", "Undo", "Redo",   "Netlist"};

using Commands = std::vector<std::vector<std::string>>;

Commands wire_commands(const ActionInput& input) {
  Commands commands;
  const Point from = snap_point(input.start, input.snap);
  const Point to = snap_point(input.pointer, input.snap);
  for (const WireSegment& s : route_wire(from, to, input.route)) {
    commands.push_back({"wire", coordinate_text(s.p1.x), coordinate_text(s.p1.y), coordinate_text(s.p2.x),
                        coordinate_text(s.p2.y)});
  }
  return commands;
}

// `move`/`copy` by the snapped pointer displacement.
Commands displacement(std::string option, const ActionInput& input) {
  const Point from = snap_point(input.start, input.snap);
  const Point to = snap_point(input.pointer, input.snap);
  return {{std::move(option), coordinate_text(to.x - from.x), coordinate_text(to.y - from.y)}};
}

// `rotate`/`flip` around the snapped pointer.
Commands around_pointer(std::string option, const ActionInput& input) {
  const Point pivot = snap_point(input.pointer, input.snap);
  return {{std::move(option), coordinate_text(pivot.x), coordinate_text(pivot.y)}};
}

Commands place_commands(const ActionInput& input) {
  const Point at = snap_point(input.pointer, input.snap);
  return {{"place", input.symbol, coordinate_text(at.x), coordinate_text(at.y)}};
}

}  // namespace

std::span<const EditAction> palette_actions() noexcept {
  return kPalette;
}

std::span<const EditAction> context_menu_actions() noexcept {
  return kContextMenu;
}

std::optional<EditAction> desktop_binding(std::string_view key) noexcept {
  for (const Binding& binding : kBindings) {
    if (binding.key == key) return binding.action;
  }
  return std::nullopt;
}

std::string_view action_label(EditAction action) noexcept {
  return kLabels.at(static_cast<std::size_t>(action));
}

std::string coordinate_text(double value) {
  std::array<char, 32> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%.16g", value == 0.0 ? 0.0 : value);
  return buffer.data();
}

std::vector<std::vector<std::string>> action_commands(EditAction action, const ActionInput& input) {
  switch (action) {
    case EditAction::wire:
      return wire_commands(input);
    case EditAction::place_symbol:
      return place_commands(input);
    case EditAction::move:
      return displacement("move", input);
    case EditAction::copy:
      return displacement("copy", input);
    case EditAction::rotate:
      return around_pointer("rotate", input);
    case EditAction::flip:
      return around_pointer("flip", input);
    case EditAction::delete_selection:
      return {{"delete"}};
    case EditAction::descend:
      return {{"descend", input.instance}};
    case EditAction::ascend:
      return {{"ascend"}};
    case EditAction::highlight:
      return {{"hilight", "selection"}};
    case EditAction::undo:
      return {{"undo"}};
    case EditAction::redo:
      return {{"redo"}};
    case EditAction::netlist:
      return {{"netlist"}};
    default:  // select, edit_properties: a tool change or a dialog
      return {};
  }
}

}  // namespace hefesto::schem
