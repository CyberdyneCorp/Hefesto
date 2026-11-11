// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The GUI manager view models (openspec/changes/m4-scripting/design.md
// D4/D8/D12; capability ui-gui-managers): platform-neutral state behind the
// wrapper menus and the cell, library, technology and DRC managers, the
// text helper and the layer toolbar, and the functions that fill them from
// the session and turn their buttons into commands. No widgets: SHELL
// renders the models (desktop) and every user action is a logged command.
// The model functions run engine commands nested in the calling command,
// as Magic's manager scripts do.
//
// (Magic ref: tcltk/cellmgr.tcl, libmgr.tcl, wrapper.tcl:286-531,
// texthelper.tcl, tools.tcl)
//
// Owner: GUIMGR (design D2); the models are shared interfaces read by SHELL.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::script {
class TagTable;
}

namespace hefesto::gui {

// --- wrapper menus ("Wrapper menus") -------------------------------------------------

/// One item of a wrapper menu.
struct WrapperMenuItem {
  enum class Kind : std::uint8_t { command, check, radio, separator, cascade };
  Kind kind = Kind::command;
  std::string label;
  std::string command;   // the logged command the item issues
  std::string variable;  // check/radio: the option variable (Opts(...))
  std::string accelerator;
  bool enabled = true;

  friend bool operator==(const WrapperMenuItem&, const WrapperMenuItem&) = default;
};

struct WrapperMenu {
  std::string title;  // "File", "Edit", "Cell", "Window", "Layers", "Drc", "Options"
  std::vector<WrapperMenuItem> items;

  friend bool operator==(const WrapperMenu&, const WrapperMenu&) = default;
};

struct WrapperMenuModel {
  std::vector<WrapperMenu> menus;
};

// --- script tags ---------------------------------------------------------------------

/// The Tcl library scripts whose tags the managers install.
enum class GuiScript : std::uint8_t { wrapper, cell_manager, library_manager, drc_manager, text_helper };

/// The `magic::tag add` lines of `script` (command, tag script) in Magic's
/// order (wrapper.tcl:567-583, cellmgr.tcl:13-15, libmgr.tcl:13-17,
/// drcmgr.tcl:13-14, texthelper.tcl:142).
[[nodiscard]] const std::vector<std::pair<std::string, std::string>>& gui_script_tags(GuiScript script);

/// Installs the tags of `script` with `tag add` semantics (appended to an
/// existing tag as `<old> ; <new>`).
void install_gui_tags(script::TagTable& tags, GuiScript script);

// --- cell manager ("Cell manager") -----------------------------------------------------

/// One tree item: id is the `/`-joined definition path from a top cell.
struct CellManagerRow {
  std::string id;        // e.g. "top/inv"
  std::string cell;      // the definition (item text)
  std::string instance;  // the first instance of `cell` (the column); empty for a top cell
  int depth = 0;         // 0 for a top cell

  friend bool operator==(const CellManagerRow&, const CellManagerRow&) = default;
};

struct CellManagerModel {
  std::string window;  // the target window path (Opts(target), "default" = first layout window)
  std::vector<CellManagerRow> rows;  // top cells, each followed by its children
};

/// The tree as `cellmanager update` builds it: `cellname list topcells`,
/// each with its children (`cellname list children`).
[[nodiscard]] CellManagerModel cell_manager_model(cmd::CommandContext& ctx);

/// The children of `row` (opening an item).
[[nodiscard]] std::vector<CellManagerRow> cell_manager_children(cmd::CommandContext& ctx, const CellManagerRow& row);

enum class CellManagerAction : std::uint8_t { load, edit, expand, place, zoom };

/// The commands a button issues for `row` (the instance path is the row's
/// instances joined with `/`, given in `instance_path`).
[[nodiscard]] std::vector<std::vector<std::string>> cell_manager_commands(CellManagerAction action,
                                                                         const CellManagerRow& row,
                                                                         std::string_view instance_path);

// --- library manager ("Library manager"; Q-SCRIPT-27) ----------------------------------

struct LibraryManagerRow {
  std::string directory;   // the directory node, `<dir>/`
  std::string cell;        // the file's root name (item text)
  std::string technology;  // the Technology column

  friend bool operator==(const LibraryManagerRow&, const LibraryManagerRow&) = default;
};

struct LibraryManagerModel {
  std::vector<std::string> directories;  // `<dir>/` of `path search`, then of `path cell` (expanded)
  std::vector<LibraryManagerRow> rows;   // grouped by directory, files sorted
};

/// The technology of a `.mag` file's text: line 1 exactly `magic` and line
/// 2 exactly `tech <name>` (two words), else nullopt.
[[nodiscard]] std::optional<std::string> mag_file_technology(std::string_view text);

/// The tree of `libmanager update`; with `filter` only cells of the
/// current technology (`tech name`).
[[nodiscard]] LibraryManagerModel library_manager_model(cmd::CommandContext& ctx, bool filter = true);

enum class LibraryManagerAction : std::uint8_t { load, place, pick };

[[nodiscard]] std::vector<std::vector<std::string>> library_manager_commands(LibraryManagerAction action,
                                                                            std::string_view cell);

// --- technology manager ("Technology manager") -----------------------------------------

struct TechMenuEntry {
  std::string label;
  std::string command;

  friend bool operator==(const TechMenuEntry&, const TechMenuEntry&) = default;
};

struct TechManagerModel {
  std::string technology;
  std::string version;      // "(version <v>)"
  std::string description;
  std::string microns_per_lambda;  // `%g` of cif scale output × λden/λnum
  std::string lambda_numerator;    // the two entries of internal units per lambda
  std::string lambda_denominator;
  std::vector<std::string> cif_input_styles;
  std::vector<std::string> cif_output_styles;
  std::vector<std::string> extract_styles;
  std::vector<std::string> drc_styles;
  std::string cif_input_style;
  std::string cif_output_style;
  std::string extract_style;
  std::string drc_style;
  /// The tech-name menu: `tech load <tech>` for each `*.tech`/`*.tech27` of
  /// `path sys`, then `source <PDK_ROOT>/<dir>/libs.tech/magic/<dir>.magicrc`
  /// for each entry of PDK_ROOT.
  std::vector<TechMenuEntry> tech_menu;
};

/// The model of `techmanager init|update`. PDK_ROOT is the interpreter's
/// variable, else the path-variable table (environment).
[[nodiscard]] TechManagerModel tech_manager_model(cmd::CommandContext& ctx);

/// Editing the units-per-lambda entries: `scalegrid (lval0·λnum) (lval1·λden)`.
[[nodiscard]] std::vector<std::string> tech_manager_scalegrid(std::int64_t lval0, std::int64_t lval1,
                                                              std::int64_t lambda_num, std::int64_t lambda_den);

// --- DRC manager ("DRC manager and DRC report file"; Q-SCRIPT-26) ----------------------

struct DrcManagerError {
  std::string rule;                // the `drc why` text
  std::vector<std::string> boxes;  // "llx lly urx ury"

  friend bool operator==(const DrcManagerError&, const DrcManagerError&) = default;
};

struct DrcManagerModel {
  std::string cell;
  std::vector<DrcManagerError> errors;
};

// --- text helper ("Text helper"; Q-SCRIPT-31) -------------------------------------------

struct TextHelperModel {
  std::string text;
  std::string font = "FreeSans";
  std::string size = "1";
  std::string justification = "center";
  std::string rotation = "0";
  std::string offset = "0 0";
  std::string layer;            // empty: the default layer
  bool default_layer = true;    // the "default" layer check box (typedflt)
  bool sticky = false;
  bool port = false;            // the port check box (typeport)
  std::string port_index = "0";

  friend bool operator==(const TextHelperModel&, const TextHelperModel&) = default;
};

/// The commands Apply issues for a new label (make_new_label), without the
/// port-entry update (`port last + 1`). Empty when the text is empty.
[[nodiscard]] std::vector<std::vector<std::string>> new_label_commands(const TextHelperModel& model);

/// The commands Apply issues to the selected label (change_label), between
/// `units microns` and the restore; `port_exists` is `port exists`.
[[nodiscard]] std::vector<std::vector<std::string>> change_label_commands(const TextHelperModel& model,
                                                                          bool port_exists);

/// Apply: change the selected label, or create a new one when none is
/// selected; after a new port the port entry advances to `port last + 1`.
[[nodiscard]] cmd::CommandResult text_helper_apply(cmd::CommandContext& ctx, TextHelperModel& model);

/// The fonts the font menu lists (`default` and `setlabel fontlist`).
[[nodiscard]] std::vector<std::string> text_helper_fonts(cmd::CommandContext& ctx);

// --- layer toolbar ("Layer toolbar", "Toolbar order presets file"; Q-SCRIPT-11, 28) ------

struct ToolbarModel {
  std::vector<std::string> layers;  // in toolbar order
  std::vector<std::string> hidden;  // turned off with `see no` (blank icon)
  std::vector<std::string> locked;  // locked layers (pale icon, only see and `u` bound)
  std::string presets_file;         // `<tech>_toolbars.tcl` when it was read, else empty

  friend bool operator==(const ToolbarModel&, const ToolbarModel&) = default;
};

}  // namespace hefesto::gui
