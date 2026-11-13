// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free model of the text helper utility window
// (docs/design/mockups/M4-Managers.dc.html tile 4, approved 2026-11-11;
// capability ui-gui-managers "Text helper", Q-SCRIPT-31). It edits a
// gui::TextHelperModel (GUIMGR) and turns Apply into the command lines
// Magic's texthelper issues (gui::new_label_commands for a new label, the
// `units microns` … `setlabel` … sequence of gui::change_label_commands for a
// selected one), which the shell runs through the Dispatcher, so they are
// logged. The board's 3×3 justification grid plus "center" and "default"
// replaces Magic's justification menu; the preview line shows what Apply
// will issue.
//
// Owner: SHELL (openspec/changes/m4-scripting/design.md D2, task 10.4).

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/gui/managers.hpp"

namespace hefesto::shell {

/// The justification choices in grid order (row by row: NW N NE / W c E /
/// SW S SE), then "center" and "default". The grid's middle cell is
/// "center" as well (Magic has no other centered justification).
[[nodiscard]] const std::array<std::string_view, 11>& justification_choices();

/// What the selection holds for the helper (Magic's update_texthelper).
enum class TextHelperTarget : unsigned char { new_label, selected_label, selected_labels };

class TextHelperView {
 public:
  TextHelperView() = default;

  [[nodiscard]] const gui::TextHelperModel& model() const noexcept { return model_; }
  gui::TextHelperModel& model() noexcept { return model_; }
  [[nodiscard]] TextHelperTarget target() const noexcept { return target_; }
  /// "New label", "Selected label" or "Selected labels" (the title).
  [[nodiscard]] std::string title() const;

  /// update_texthelper with no editable label selected: Magic's defaults
  /// (empty text, FreeSans, size 1, center, rotation 0, offset 0 0) and the
  /// port entry at `port last + 1`.
  void show_new_label(long long port_last);
  /// update_texthelper with `count` selected labels whose common
  /// attributes are `fields` (read with `setlabel …` in microns).
  void show_selection(const gui::TextHelperModel& fields, std::size_t count);

  /// The justification grid: selects one of justification_choices().
  void set_justification(std::string_view just);
  [[nodiscard]] bool justification_is(std::string_view just) const noexcept { return model_.justification == just; }
  /// Which buttons of the grid (justification_choices() order) show the
  /// current justification as chosen: every slot holding it, so "center"
  /// marks the grid's middle square and the "center" button below it.
  [[nodiscard]] std::array<bool, 11> justification_marks() const noexcept;

  /// The command lines Apply (or Okay) issues: for a new label
  /// gui::new_label_commands; for selected labels `units microns`, the
  /// `setlabel`/`port` lines of gui::change_label_commands, then `units
  /// <previous units>`. Empty when there is nothing to do (no text).
  [[nodiscard]] std::vector<std::string> apply_lines(bool port_exists, std::string_view previous_units) const;
  /// The board's preview line: the apply lines joined with " · ".
  [[nodiscard]] std::string preview(bool port_exists = false, std::string_view previous_units = "lambda") const;
  /// After Apply created a port: the port entry advances to `port last + 1`.
  void after_apply(std::optional<long long> port_last);

 private:
  gui::TextHelperModel model_;
  TextHelperTarget target_ = TextHelperTarget::new_label;
};

}  // namespace hefesto::shell
