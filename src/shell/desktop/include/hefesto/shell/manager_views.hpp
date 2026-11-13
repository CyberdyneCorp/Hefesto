// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free view models of the M4 manager panels
// (docs/design/mockups/M4-Managers.dc.html, approved 2026-11-11; capability
// ui-gui-managers "Cell manager", "Library manager", "Technology manager";
// hefesto-app-ux "Functional parity with Magic's GUI managers", "Desktop
// platform integration through Raven" Tables). The core fills the gui::
// models (GUIMGR, hefesto/gui/managers.hpp); these views flatten them into
// the rows of a virtual Raven Table (a tree drawn as indented rows, so a
// 446-cell library costs no widget per row), keep the open/closed state of
// the nodes, and turn every button into the command line Magic's manager
// issues, which the shell runs through the Dispatcher (logged, replayable).
//
// Owner: SHELL (openspec/changes/m4-scripting/design.md D2, task 10.4).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/gui/managers.hpp"

namespace hefesto::shell {

/// One command (words) as a Tcl line: each word quoted as Tcl's `list`
/// quotes an element (`{0 0}`, `{}`), so the project's interpreter and the
/// M0 command reader both read it back unchanged, without substitution.
[[nodiscard]] std::string command_line(const std::vector<std::string>& words);
/// Each command of `commands` as a line, in order.
[[nodiscard]] std::vector<std::string> command_lines(const std::vector<std::vector<std::string>>& commands);

/// The disclosure glyph and indentation of a tree row in a Table cell:
/// two spaces per level, then "▾ " (open), "▸ " (closed) or "  " (leaf).
[[nodiscard]] std::string tree_cell_text(int depth, std::optional<bool> open, std::string_view text);

// --- cell manager (Cells tab of the left dock) -----------------------------------------

/// The Cells tab: the gui::CellManagerModel tree (top cells, then children
/// loaded when a node is opened) as visible Table rows.
class CellsView {
 public:
  /// Children of a row when it is opened (gui::cell_manager_children).
  using ChildrenFn = std::function<std::vector<gui::CellManagerRow>(const gui::CellManagerRow&)>;

  CellsView() = default;
  explicit CellsView(ChildrenFn children) : children_(std::move(children)) {}

  /// Replaces the tree (`cellmanager update`). Open nodes and the selection
  /// are kept by id when they still exist; a top cell with children starts
  /// open, as Magic opens the top items.
  void reset(const gui::CellManagerModel& model);

  [[nodiscard]] const std::string& target() const noexcept { return target_; }
  /// The rows a Table shows (closed subtrees hidden).
  [[nodiscard]] std::size_t visible_count() const noexcept { return visible_.size(); }
  [[nodiscard]] const gui::CellManagerRow& visible_row(std::size_t index) const;
  /// Table cell text: column 0 the cell (indented, with its disclosure),
  /// column 1 the first instance, or "(top cell)" at depth 0.
  [[nodiscard]] std::string cell_text(std::size_t index, int column) const;
  /// Screen-reader text of a row: "<cell>, instance <inst>, level <n>, open|closed".
  [[nodiscard]] std::string accessible_text(std::size_t index) const;

  /// Opens or closes the node at visible `index`; opening loads its children
  /// once. False for a leaf.
  bool toggle(std::size_t index);
  /// Keyboard: Right opens (or moves to the first child), Left closes (or
  /// moves to the parent), as tree views do. Returns the new selection.
  std::optional<std::size_t> key_right(std::size_t index);
  std::optional<std::size_t> key_left(std::size_t index);
  /// A key pressed in the Cells table with row `index` selected (Raven key
  /// code, kMod* bits): unmodified Left and Right run key_left / key_right
  /// and return true (consumed); any other key returns false and is left to
  /// the table (arrows, Return).
  bool tree_key(int key, std::uint8_t modifiers, std::size_t index);

  void select(std::optional<std::size_t> index);
  [[nodiscard]] std::optional<std::size_t> selected() const;

  /// The `/`-joined instance path of the row, as `select cell` takes it
  /// (Magic's findinstance): the instances from below the top cell down.
  [[nodiscard]] std::string instance_path(std::size_t index) const;
  /// The command lines of a button for the selected row (empty: none selected).
  [[nodiscard]] std::vector<std::string> commands(gui::CellManagerAction action) const;

  /// `magic::mgrselect %r` (the `select` tag): highlights the instance the
  /// selection names. `top` is the window's root cell; `path` the selected
  /// instances from the top down with their definitions
  /// (`instance list celldef`). A path of no instance (a 5-element result
  /// in Magic) selects the top cell. Returns Magic's message
  /// "Selected cell is <id> (<sstr>)", or nullopt when nothing matched.
  std::optional<std::string> select_instance(std::string_view top,
                                             const std::vector<std::pair<std::string, std::string>>& path,
                                             std::string_view sstr);

 private:
  struct Node {
    gui::CellManagerRow row;
    bool open = false;
    bool loaded = false;    // children asked for
    bool has_children = true;
  };
  void rebuild();
  [[nodiscard]] std::optional<std::size_t> find(std::string_view id) const;
  [[nodiscard]] std::size_t subtree_end(std::size_t node) const;
  void load_children(std::size_t node);

  ChildrenFn children_;
  std::string target_ = "default";
  std::vector<Node> nodes_;           // pre-order
  std::vector<std::size_t> visible_;  // indices into nodes_
  std::optional<std::string> selected_id_;
};

// --- library manager (Libraries tab of the left dock) ----------------------------------

/// Shows a directory with its path variables unexpanded (board: `$PDKPATH/…`):
/// the longest variable value that is a prefix of `directory` on a path
/// boundary is replaced by `$NAME`. `variables` are (name, value) pairs.
[[nodiscard]] std::string display_directory(std::string_view directory,
                                            const std::vector<std::pair<std::string, std::string>>& variables);

/// The Libraries tab: one node per directory (only the first open, as
/// Magic's libmgr), its `.mag` cells under it.
class LibrariesView {
 public:
  void reset(const gui::LibraryManagerModel& model,
             const std::vector<std::pair<std::string, std::string>>& variables = {});

  [[nodiscard]] std::size_t visible_count() const noexcept { return visible_.size(); }
  /// Column 0: directory node (`<dir>/`, variables unexpanded) or cell;
  /// column 1: the cell's technology.
  [[nodiscard]] std::string cell_text(std::size_t index, int column) const;
  [[nodiscard]] std::string accessible_text(std::size_t index) const;
  [[nodiscard]] bool is_directory(std::size_t index) const;
  /// The cell of a visible row; empty for a directory node.
  [[nodiscard]] std::string cell(std::size_t index) const;
  [[nodiscard]] std::size_t cell_count() const noexcept { return cell_count_; }

  bool toggle(std::size_t index);
  void select(std::optional<std::size_t> index) { selected_ = index; }
  [[nodiscard]] std::optional<std::size_t> selected() const noexcept { return selected_; }
  /// The command lines of a button for the selected cell (empty: none, or a directory).
  [[nodiscard]] std::vector<std::string> commands(gui::LibraryManagerAction action) const;

 private:
  struct Group {
    std::string directory;
    std::string label;
    std::vector<gui::LibraryManagerRow> rows;
    bool open = false;
  };
  struct Visible {
    std::size_t group = 0;
    std::optional<std::size_t> row;  // nullopt: the directory node
  };
  void rebuild();

  std::vector<Group> groups_;
  std::vector<Visible> visible_;
  std::optional<std::size_t> selected_;
  std::size_t cell_count_ = 0;
};

// --- technology manager (right dock) -----------------------------------------------------

/// One labelled line of the technology manager.
struct TechField {
  std::string label;
  std::string value;

  friend bool operator==(const TechField&, const TechField&) = default;
};

/// The style menus of the technology manager.
enum class TechStyle : unsigned char { cif_input, cif_output, extract, drc };

/// The technology manager panel (board tile 3).
class TechView {
 public:
  void reset(gui::TechManagerModel model) { model_ = std::move(model); }
  [[nodiscard]] const gui::TechManagerModel& model() const noexcept { return model_; }

  /// "sky130A (version 1.0.588-…)" and the description.
  [[nodiscard]] std::string title() const;
  /// The read-only lines: microns per lambda, internal units per lambda, the
  /// four styles with their current choice.
  [[nodiscard]] std::vector<TechField> fields() const;
  [[nodiscard]] const std::vector<std::string>& styles(TechStyle style) const;
  [[nodiscard]] const std::string& current(TechStyle style) const;
  /// The command a style menu choice issues: `cif istyle`, `cif ostyle`,
  /// `extract style`, `drc style` with the style.
  [[nodiscard]] static std::string style_command(TechStyle style, std::string_view name);
  /// The command of the technology menu entry `index` (`tech load <t>` or
  /// `source <rc>`; the rc goes through consent, M4-Script-Consent).
  [[nodiscard]] std::optional<std::string> tech_menu_command(std::size_t index) const;
  /// Editing the internal-units-per-lambda entries: `scalegrid a b`, or
  /// nullopt when an entry is not a positive integer or the scale is unknown.
  [[nodiscard]] std::optional<std::string> scalegrid_command(std::string_view numerator,
                                                             std::string_view denominator) const;

 private:
  gui::TechManagerModel model_;
};

}  // namespace hefesto::shell
