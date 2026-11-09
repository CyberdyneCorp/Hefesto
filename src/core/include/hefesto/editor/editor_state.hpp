// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session state that M1 commands operate on, reachable from every
// handler through cmd::CommandContext::editor(). One per session (CLI run,
// desktop window set, test). Each member's type is owned by its area; this
// aggregate is shared and owned by INTEGRATION
// (openspec/changes/m1-core-viewer/design.md, D2 and D4). Areas request new
// members through their open items instead of editing it.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "hefesto/app/settings_stack.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::cell {
class FileLockService;
}

namespace hefesto::editor {

/// The `snap` command's unit for typed and printed coordinates.
enum class SnapMode : std::uint8_t { internal, lambda, user };

struct EditorState {
  EditorState();
  ~EditorState();
  EditorState(const EditorState&) = delete;
  EditorState& operator=(const EditorState&) = delete;

  // TECH: the current technology (never null; starts as the built-in one)
  // and the session-wide internal grid scale.
  std::shared_ptr<const tech::Technology> technology;
  tech::GridScale grid;

  // MODEL: every cell definition, the edit cell and the box tool.
  db::CellLibrary cells;
  db::CellDef* edit_cell = nullptr;
  std::optional<db::Rect> box;  // in box_root coordinates; nullopt = no box
  /// The cell the box belongs to (Magic's box root definition); nullptr =
  /// the edit cell (or the window's root when there is no edit cell).
  db::CellDef* box_root = nullptr;
  tech::TypeMask locked_types;  // "Active (locked) layers"
  SnapMode snap = SnapMode::lambda;  // `snap` (Magic's default: lambda)
  /// Id of the edit cell's current instance (set by `getcell`, used by
  /// `instance`/`array` without a name); empty = none.
  std::string current_use;
  /// `property compat` (layout-properties): Magic-compatible property typing.
  bool property_compat = true;

  // CELL: search paths, path variables and the lock service (nullptr =
  // locking disabled).
  cell::SearchPaths search_paths;
  cell::PathVariables path_variables;
  std::unique_ptr<cell::FileLockService> locks;

  // VIEWER: display styles (absent in a headless session), fonts, windows.
  std::optional<display::StyleSet> styles;
  display::FontRegistry fonts;
  view::WindowManager windows;

  // PDK / app: layered settings (the project's typed compat settings stay
  // in settings::ProjectSettings, reachable through CommandContext::project()).
  app::SettingsStack settings;
};

}  // namespace hefesto::editor
