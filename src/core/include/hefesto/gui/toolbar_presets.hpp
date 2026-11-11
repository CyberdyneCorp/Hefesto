// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layer toolbar and its presets file `<techname>_toolbars.tcl`
// (openspec/changes/m4-scripting/design.md D12; capability ui-gui-managers
// "Layer toolbar", "Toolbar order presets file"; Q-SCRIPT-11, Q-SCRIPT-28).
//
// The presets file is read as data and never evaluated, in both
// magic-compat settings (Q-SCRIPT-11 fix; Magic sources it): only lines
// `set <name> {<layer> ...}` and `set <name> "<layers>"` are read; every
// other statement is reported (GUI-TOOLBAR-001, file and line) and ignored.
// An untrusted reader, fuzzed by fuzz_toolbar_presets.
//
// (Magic ref: tcltk/toolbar.tcl:7-241, tcltk/reorderLayers.tcl:1-316)
//
// Owner: GUIMGR (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/gui/managers.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::gui {

struct ToolbarPreset {
  std::string name;
  std::vector<std::string> layers;
  std::size_t line = 0;  // 1-based line of the `set`

  friend bool operator==(const ToolbarPreset&, const ToolbarPreset&) = default;
};

/// A statement of the file that is not a data line.
struct SkippedStatement {
  std::size_t line = 0;  // 1-based
  std::string text;

  friend bool operator==(const SkippedStatement&, const SkippedStatement&) = default;
};

struct PresetFile {
  /// In file order; a name set twice appears twice (the last one counts).
  std::vector<ToolbarPreset> presets;
  std::vector<SkippedStatement> skipped;

  /// The layers of the last `set name ...`, or nullptr.
  [[nodiscard]] const ToolbarPreset* find(std::string_view name) const;
  /// The preset names in order of first appearance (the reorder dialog list).
  [[nodiscard]] std::vector<std::string> names() const;
};

/// The presets file of technology `tech`: `<tech>_toolbars.tcl`.
[[nodiscard]] std::string toolbar_presets_file_name(std::string_view tech);

/// Parses the presets file as data (never evaluated). Blank lines and `#`
/// comment lines are neither presets nor reported.
[[nodiscard]] PresetFile parse_toolbar_presets(std::string_view text);

/// The GUI-TOOLBAR-001 diagnostics of `file`'s skipped statements (located
/// at file and line).
[[nodiscard]] std::vector<diag::Diagnostic> skipped_diagnostics(const PresetFile& file, std::string_view file_name);

/// "Toolbar order presets file" save: `text` with every match of the
/// per-line regex `set current_toolbar \{.*\}` replaced by
/// `set current_toolbar {<layers>}` (Magic's `regsub -line -all`).
[[nodiscard]] std::string rewrite_current_toolbar(std::string_view text, const std::vector<std::string>& layers);

/// The file text `set current_toolbar {<layers>}` for a toolbar without a
/// presets file.
[[nodiscard]] std::string write_current_toolbar(const ToolbarModel& toolbar);

/// Options of maketoolbar (`Opts(hidespecial)`, `Opts(hidelocked)`).
struct ToolbarOptions {
  bool hide_special = false;
  bool hide_locked = false;
};

/// `maketoolbar` ("Layer toolbar" steps 1-2): `errors labels subcell`
/// (unless hide_special) and `tech layer *` (or `tech unlocked`), replaced by
/// the file's `current_toolbar` when `./<tech>_toolbars.tcl` (in the
/// session's working directory) sets it. Skipped statements of the file are
/// emitted through `ctx`. Locked layers are listed in `locked`.
[[nodiscard]] ToolbarModel build_toolbar(cmd::CommandContext& ctx, const ToolbarOptions& options);

/// The reorder dialog's preset list: the names the file sets, or
/// `default_toolbar` when the session has no presets file.
[[nodiscard]] std::vector<std::string> toolbar_preset_names(cmd::CommandContext& ctx);

/// The reorder dialog's Load: the layers of `preset` read from the presets
/// file; the file's `current_toolbar` lines are rewritten to them. Without
/// a file the order applies only in memory (the returned layers). Fails
/// with GUI-TOOLBAR-002 naming `<tech>_toolbars.tcl`.
[[nodiscard]] diag::Outcome<std::vector<std::string>> load_toolbar_preset(cmd::CommandContext& ctx,
                                                                        std::string_view preset);

/// A pointer or key action on a toolbar row.
enum class ToolbarAction : std::uint8_t {
  button1,         // see <layer>
  button3,         // see no <layer>
  button2,         // paint <layer> (unlocked)
  shift_button2,   // erase <layer> (unlocked)
  key_p,           // paint
  key_e,           // erase
  key_s,           // select more area <layer>
  key_shift_s,     // select less area <layer>
  key_l,           // tech lock <layer>
  key_u,           // tech unlock <layer> (locked rows only)
};

/// The command a row action issues to layout window `window` (e.g.
/// `.layout1.pane.top.magic paint poly`), or nullopt when the row does not
/// bind the action (locked rows bind only see and `u`).
[[nodiscard]] std::optional<std::vector<std::string>> toolbar_row_command(std::string_view window,
                                                                          std::string_view layer, bool locked,
                                                                          ToolbarAction action);

/// The title-bar message when the pointer enters a row.
[[nodiscard]] std::string toolbar_row_message(std::string_view layer, bool locked);

/// `see` updates the row icons: `see <layers>` shows, `see no <layers>`
/// hides; the aliases `none`, `allSame`, `*`, `$` and `connect` are ignored.
void toolbar_apply_see(ToolbarModel& toolbar, const std::vector<std::string>& see_args);

}  // namespace hefesto::gui
