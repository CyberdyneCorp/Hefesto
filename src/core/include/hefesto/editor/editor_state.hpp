// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session state that commands operate on, reachable from every handler
// through cmd::CommandContext::editor(). One per session (CLI run, desktop
// window set, test). Each member's type is owned by its area; this
// aggregate is shared and owned by INTEGRATION
// (openspec/changes/m1-core-viewer/design.md D2/D4;
// openspec/changes/m2-interactive-editing/design.md D2/D4 for the M2
// members; openspec/changes/m3-drc/design.md D2/D4 for the M3 members;
// openspec/changes/m4-scripting/design.md D4 for the M4 member;
// openspec/changes/m8-schematic/design.md D4 for the M8 member;
// openspec/changes/m7-extraction/design.md D5 for the M7 members;
// openspec/changes/m9-lvs-simulation/design.md D4 for the M9 members).
// Areas request new members through their open items instead of editing it.

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
#include "hefesto/db/lock.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/drc/session.hpp"
#include "hefesto/extract/command/session.hpp"
#include "hefesto/extresist/command/session.hpp"
#include "hefesto/lef/session.hpp"
#include "hefesto/lvs/session.hpp"
#include "hefesto/mask/session.hpp"
#include "hefesto/netlist/session.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/sim/session.hpp"
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
#include "hefesto/view3d/client/session.hpp"
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
  /// Where the styles were looked up at startup (the system library path);
  /// `tech load` reloads the new technology's styletype from the same places.
  std::vector<platform::Path> style_dirs;
  display::FontRegistry fonts;
  view::WindowManager windows;
  /// INTEGRATION: the session shows its windows on a display (Magic's
  /// GrDisplayStatus is not left at DISPLAY_SUSPEND by the NULL driver).
  /// Set by app::run_startup for a display other than NULL; then loading a
  /// cell over an untouched "(UNNAMED)" deletes it, as Magic does while its
  /// display runs (Magic ref: dbwind/DBWprocs.c:321-331,608-617).
  bool display_running = false;
  /// With the NULL driver, Magic's GrDisplayStatus left DISPLAY_SUSPEND:
  /// a DEF read's LefEstimate sets it to DISPLAY_IN_PROGRESS on the first
  /// counted section item and to DISPLAY_IDLE on the last one, and nothing
  /// restores it, so later window loads delete an untouched "(UNNAMED)" as
  /// with a running display (magic-compat ON only; Q-LEFDEF-141).
  /// (Magic ref: lef/lefRead.c:87-131, dbwind/DBWprocs.c:321-331)
  bool display_status_active = false;

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

  // ---- M3 (openspec/changes/m3-drc/design.md D4) ----

  // INTEGRATION: the reader/writer lock over `cells` (db/lock.hpp). The
  // dispatcher holds it exclusively for every command; readers (scene
  // building, the DRC worker) hold it shared.
  db::DatabaseLock database_lock;

  // ---- M4 (openspec/changes/m4-scripting/design.md D4) ----

  // TCL / BINDING / PDKINT / PCELL: the project's interpreter and script
  // state (script/project.hpp; each member's owner is named there). One
  // EditorState is one open project, so each open project has its own
  // interpreter (script::open_project_interpreter / close_project_interpreter).
  // The destructor closes the interpreter before any member is destroyed
  // (its drcstate bridge is registered in `drc.events`).
  script::ProjectInterpreter script;

  // INTEGRATION: Magic's TechOverridesDefault. Set by app::run_startup while
  // the startup files run after a successful `-T` load, so their
  // `tech load` is a no-op unless given `-override` (app-startup "Startup
  // technology"; Magic ref: utils/main.c:872-882,1132, commands/CmdTZ.c:555-579).
  bool technology_override = false;

  // INTEGRATION: the front end's file picker (platform/file_picker.hpp; not
  // owned). nullptr: `promptload`/`promptsave` act as a cancelled dialog.
  platform::FilePicker* file_picker = nullptr;

  // ---- M5 (openspec/changes/m5-mask-gds-cif/design.md D4) ----

  // STYLES / INPUT / GDSR / GDSW / CIF / INTEGRATION: the mask session
  // (mask/session.hpp; each member's owner is named there): the output and
  // input styles, the `gds` and `cif` settings, the read warnings and the
  // shared `random` generator.
  mask::MaskSession mask;

  // ---- M6 (openspec/changes/m6-lef-def-oasis/design.md D4) ----

  // LEFTECH / LEFVIA / LEFR / LEFW / DEFW: the LEF/DEF session
  // (lef/session.hpp; each member's owner is named there): the LEF layer
  // table, the VIA and NONDEFAULTRULE tables, `lef datestamp`, `lef
  // nocheck` and the sticky LEF units.
  lef::LefSession lefdef;

  // ---- M7 (openspec/changes/m7-extraction/design.md D5) ----

  // EXTTECH / EXTCMD: the extraction session (extract/command/session.hpp;
  // each member's owner is named there): the style table and current style
  // (rebuilt by editor/extract_hook.hpp on tech load and grid rescale), the
  // `extract` options, overrides and path-length lists.
  extract::ExtractSession extract;
  // SPICE / SIM / EXTFLAT: the session-sticky `ext2spice` and `ext2sim`
  // options and the name-conversion flags (netlist/session.hpp).
  netlist::NetlistSession netlist;
  // RESCMD / RESNET: the `extresist` options and the path-resistance heap
  // (extresist/command/session.hpp).
  extresist::ResistSession extresist;

  // ---- M8 (openspec/changes/m8-schematic/design.md D4) ----

  // EDIT / SYMLIB: the schematic session (schem/session.hpp; each member's
  // owner is named there): the open schematic and symbol documents, the
  // symbol cache, the library search path and the active PDK's `xschemrc`
  // result. Independent of the layout database.
  schem::Session schem;

  // ---- M10 (openspec/changes/m10-3d-view/design.md D4) ----

  // WIND3D / CAMERA / MESH / INSPECT: the 3D session (view3d/client/
  // session.hpp; each member's owner is named there): the at-most-one 3D
  // window with its mode, view state, cut box, visibility sets and mesh.
  view3d::Session view3d;

  // ---- M9 (openspec/changes/m9-lvs-simulation/design.md D4) ----

  // LVSCMD / LVSHIER: the LVS session (lvs/session.hpp; each member's owner
  // is named there): the last result per document, the run in progress and
  // the name maps of its two circuits.
  lvs::Session lvs;
  // SIMENG / SIMDECK / SIMRES / SIMADV: the simulation session
  // (sim/session.hpp): the testbenches, the jobs, the run sets, the result
  // store index and the engine status.
  sim::Session sim;

  // RULES / ENGINE / COMMANDS: the DRC session (drc/session.hpp; each
  // member's owner is named there). Declared last so that it is destroyed
  // first: its worker stops before the cells it reads go away.
  drc::DrcSession drc;
};

}  // namespace hefesto::editor
