// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session state that commands operate on, reachable from every handler
// through cmd::CommandContext::editor(). One per session (CLI run, desktop
// window set, test). Each member's type is owned by its area; this
// aggregate is shared and owned by INTEGRATION
// (openspec/changes/m1-core-viewer/design.md D2/D4;
// openspec/changes/m2-interactive-editing/design.md D2/D4 for the M2
// members). Areas request new members through their open items instead of
// editing it.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "hefesto/app/settings_stack.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/session.hpp"
#include "hefesto/cmd/pointer.hpp"
#include "hefesto/cmd/units.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/ui/macros.hpp"
#include "hefesto/ui/stacks.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/ui/tools.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/view/color_window.hpp"
#include "hefesto/view/feedback.hpp"
#include "hefesto/view/highlights.hpp"
#include "hefesto/view/tile_overlay.hpp"
#include "hefesto/view/window.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace hefesto::cell {
class FileLockService;
}

namespace hefesto::cmd {
class PromptHost;
}

namespace hefesto::undo {
class DbRecorder;
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
  /// `setlabel -default` values used by `label` (Magic's DefaultLabel);
  /// nullopt until the first `setlabel -default`.
  std::optional<db::LabelDefaults> label_defaults;

  // CELL: search paths, path variables, the lock service (a host
  // mechanism; front ends replace the in-process default) and the
  // per-session CELL state (`locking`, file host, timestamps, crash
  // backups, recovery), which outlives a replacement of `locks`.
  cell::SearchPaths search_paths;
  cell::PathVariables path_variables;
  std::unique_ptr<cell::FileLockService> locks;
  cell::CellSession cell_session;

  // VIEWER: display styles (absent in a headless session), fonts, windows.
  std::optional<display::StyleSet> styles;
  display::FontRegistry fonts;
  view::WindowManager windows;

  // PDK / app: layered settings (the project's typed compat settings stay
  // in settings::ProjectSettings, reachable through CommandContext::project()).
  app::SettingsStack settings;

  // ---- M2 (openspec/changes/m2-interactive-editing/design.md D4) ----

  // EDIT: `units` (replaces the M1 settings-layer key "units").
  cmd::UnitsState units;

  // UNDO: the event log and the recorder installed on `cells`
  // (db::CellLibrary::set_recorder) by the constructor, so every session
  // records. The dispatcher points the recorder at the project's compat
  // settings (cmd::Dispatcher::set_editor).
  undo::UndoLog undo;
  std::unique_ptr<undo::DbRecorder> undo_recorder;

  // SELECT: the selection (internal root-coordinate cell plus provenance).
  select::Selection selection;

  // DISPATCH: macro tables (per tool / client), the tool registry, the
  // box and edit stacks, the `suspendall` nesting depth, the pointer of the
  // command being run (set by the shell before a button or key macro and
  // by `setpoint`; nullopt: no pointer) and the command counter of
  // editor::post_command; `ui` holds the rest of the ui session state
  // (ui/state.hpp, read through ui::state()).
  ui::MacroTables macros;
  ui::ToolRegistry tools;
  ui::BoxStack box_stack;
  ui::EditStack edit_stack;
  int suspend_depth = 0;
  std::optional<cmd::PointerState> command_point;
  std::uint64_t command_count = 0;
  ui::UiState ui;

  // VIEW: feedback areas, the highlight layer, the color map editing state
  // and the `*watch` tile overlays.
  view::FeedbackList feedback;
  view::HighlightLayer highlights;
  view::ColorMapState color_map;
  view::TileOverlays tile_overlays;

  // WIRE: the wiring tool state.
  wiring::WireState wire;

  // HIER: the edit frame. `edit_transform` maps edit-cell coordinates to
  // the coordinates of `edit_root` (Magic's EditToRootTransform and
  // EditRootDef; identity / nullptr while the edit cell is the root). Read
  // through editor::to_edit / editor::to_root (editor/edit_frame.hpp).
  db::Transform edit_transform;
  db::CellDef* edit_root = nullptr;

  // INTEGRATION: the front end's prompt host (cmd/prompt.hpp; not owned).
  // nullptr: every prompt gets its default answer (headless).
  cmd::PromptHost* prompt = nullptr;
};

}  // namespace hefesto::editor
