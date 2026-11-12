// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// pcell::FrameworkState: the gencell framework's per-project state,
// editor::EditorState::script.gencell (openspec/changes/m4-scripting/
// design.md D4): the open parameter dialog (Magic's single `.params`
// window) and the toolkit menus PDK scripts registered.
//
// Owner: PCELL (design D2).

#pragma once

#include <optional>
#include <set>
#include <string>

#include "hefesto/pcell/framework/dialog.hpp"
#include "hefesto/pcell/framework/menus.hpp"

namespace hefesto::pcell {

struct FrameworkState {
  /// The parameter dialog; nullopt while closed (Magic: no `.params`).
  std::optional<DialogModel> dialog;
  ToolkitMenus menus;
  /// How many framework procedures are running (Magic's `info level` test
  /// in gencell_update: a `select` tag fired from inside a framework or
  /// device procedure does not refresh the dialog).
  int depth = 0;
  /// Devices whose missing native generator for this PDK version was
  /// reported (PCELL-NATIVE-001, once per device and session).
  std::set<std::string, std::less<>> native_fallbacks;
};

}  // namespace hefesto::pcell
