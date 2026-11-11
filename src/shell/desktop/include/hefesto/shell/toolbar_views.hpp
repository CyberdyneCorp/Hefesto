// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free models of the layer toolbar and its reorder dialog
// (docs/design/mockups/M4-Managers.dc.html tiles 5 and 6, approved
// 2026-11-11; capability ui-gui-managers "Layer toolbar", "Toolbar order
// presets file"; Q-SCRIPT-11, Q-SCRIPT-28). The layer palette of the left
// dock doubles as Magic's layer toolbar: its rows follow the toolbar order
// of gui::build_toolbar (GUIMGR), and the toolbar's row bindings
// (gui::toolbar_row_command) map onto the palette's mouse buttons and keys.
// The reorder dialog edits an order in memory; Load and Save go through the
// presets file `<tech>_toolbars.tcl`, read as data and never evaluated.
//
// Owner: SHELL (openspec/changes/m4-scripting/design.md D2, task 10.4).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/gui/managers.hpp"
#include "hefesto/gui/toolbar_presets.hpp"

namespace hefesto::shell {

/// A palette gesture on a layer row (the M2 palette's mouse and keys).
enum class PaletteGesture : std::uint8_t { click, middle, shift_middle, right, key_p, key_e, key_s, key_shift_s,
                                            key_l, key_u };

/// The command line a palette gesture issues on `layer` as Magic's toolbar
/// row binding (click = `see`, middle = `paint`, Shift-middle = `erase`,
/// right = `see no`, keys p/e/s/S/l/u), or nullopt when the row does not
/// bind it (a locked row binds only `see` and `u`). `window` empty: the
/// command goes to the current window (no window path prefix).
[[nodiscard]] std::optional<std::string> palette_command(std::string_view window, std::string_view layer,
                                                         bool locked, PaletteGesture gesture);

/// Orders `names` (the palette's layers) by `toolbar` order: names in the
/// toolbar first, in its order, then the others in their original order.
/// Returns indices into `names`.
[[nodiscard]] std::vector<std::size_t> toolbar_order(const std::vector<std::string>& names,
                                                     const gui::ToolbarModel& toolbar);

/// One row of the reorder dialog: a layer and its toolbar row (1-based;
/// grouped layers share a row).
struct ReorderRow {
  std::string layer;
  int row = 0;

  friend bool operator==(const ReorderRow&, const ReorderRow&) = default;
};

/// The "Reorder layers" dialog (Layers › Reorder layers…).
class ReorderModel {
 public:
  /// `layers`: the current toolbar order; `presets`: the names the presets
  /// file sets (`default_toolbar` without a file).
  ReorderModel(std::vector<std::string> layers, std::vector<std::string> presets);

  /// Every row in order with its toolbar row number.
  [[nodiscard]] std::vector<ReorderRow> rows() const;
  /// The rows whose layer contains `search` (case-insensitive), as Magic's
  /// search entry filters the tree; indices into rows().
  [[nodiscard]] std::vector<std::size_t> filtered(std::string_view search) const;
  [[nodiscard]] const std::vector<std::string>& layers() const noexcept { return layers_; }
  [[nodiscard]] const std::vector<std::string>& presets() const noexcept { return presets_; }

  void set_selection(std::set<std::size_t> rows) { selection_ = std::move(rows); }
  [[nodiscard]] const std::set<std::size_t>& selection() const noexcept { return selection_; }

  /// Move layer: the layer at `from` goes to position `to` (clamped); the
  /// selection follows it. False when `from` is out of range.
  bool move(std::size_t from, std::size_t to);
  /// Group selected: the selected layers join the first one's toolbar row
  /// and become adjacent after it. False with fewer than two selected.
  bool group_selected();
  /// Delete layer: removes the selected layers from the order.
  bool delete_selected();
  /// Load: replaces the order with a preset's layers (gui::load_toolbar_preset).
  void replace(std::vector<std::string> layers);
  /// True when the order differs from the one the dialog opened with.
  [[nodiscard]] bool changed() const noexcept { return layers_ != original_ || !groups_.empty(); }

  /// Save: the presets file text with its `set current_toolbar {…}` lines
  /// rewritten to this order (gui::rewrite_current_toolbar), or the single
  /// line to write when there is no file yet (`text` empty).
  [[nodiscard]] std::string saved_text(std::string_view text) const;

 private:
  std::vector<std::string> layers_;
  std::vector<std::string> original_;
  std::vector<std::string> presets_;
  std::set<std::size_t> selection_;
  /// For each grouped layer: the layer it shares a row with (its group head).
  std::vector<std::pair<std::string, std::string>> groups_;
};

}  // namespace hefesto::shell
