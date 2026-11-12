// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit menus of PDK scripts (openspec/changes/m4-scripting/design.md D9;
// capability pcell-toolkit "Toolkit menu hooks"): `magic::add_toolkit_menu`,
// `add_toolkit_button`, `add_toolkit_command` and `add_toolkit_separator`
// for the layout frame F (`.layoutN.pane.top`). The menu bar (SHELL) shows
// them beside the wrapper menus.
//
// Owner: PCELL (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hefesto::pcell {

struct ToolkitItem {
  enum class Kind : std::uint8_t { command, separator };
  Kind kind = Kind::command;
  std::string text;
  std::string command;  // the Tcl script the item runs

  friend bool operator==(const ToolkitItem&, const ToolkitItem&) = default;
};

struct ToolkitMenu {
  std::string frame;    // F, e.g. ".layout1.pane.top"
  std::string library;  // "pdk1", "pdk2" (the menu button is F.titlebar.mbuttons.<library>)
  std::string text;     // the menu title, e.g. "Devices 1"
  std::vector<ToolkitItem> items;

  friend bool operator==(const ToolkitMenu&, const ToolkitMenu&) = default;
};

/// Every toolkit menu, in creation order.
struct ToolkitMenus {
  std::vector<ToolkitMenu> menus;

  friend bool operator==(const ToolkitMenus&, const ToolkitMenus&) = default;
};

}  // namespace hefesto::pcell
