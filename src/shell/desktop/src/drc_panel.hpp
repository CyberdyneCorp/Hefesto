// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DRC results browser of the main window (DESIGN board
// docs/design/mockups/DRC-Browser.dc.html, approved 2026-11-10;
// openspec/changes/m3-drc/design.md D11, tasks.md 9.3):
//
//   list dock (left of the canvas, in place of Layers while shown)
//     DRC title · Style ▾ · Re-check · Fix… (M11, disabled)
//     summary "■ 12 error tiles · 23 areas · 5 why-texts · 1 ignored"
//     empty / running / stale notice
//     filter · virtual Raven Table grouped by why-text (Why-text | Areas)
//     Sign-off (M13, disabled)
//   stepper over the canvas, top right (the main window adds it to the
//   canvas overlay): ◂ Prev  #3 of 12  Next ▸
//   inspector dock (right, in place of the Inspector while shown)
//     DRC ERROR AREA #k OF n, why-texts, Cell / Area / Size / Style,
//     Zoom to · Why here, "Ignore rules whose text contains", Later milestones
//
// Every control runs a core command through ScreenContext::run (logged,
// replayable); the rows come from DrcBrowserModel (drc_models.hpp).
// Raven-free: built from the raven_ui wrappers.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/shell/drc_models.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

/// What the panel shows besides the rows (refreshed after every command).
struct DrcPanelState {
  DrcStatusView status;
  DrcBrowserState browser = DrcBrowserState::not_checked;
  DrcStepper stepper;
  std::size_t error_tiles = 0;
  DrcInspectorView inspector;
  std::string cell;
  std::string style;
  std::vector<std::string> styles;
  bool busy = false;
  bool has_root = false;
  std::string ignore_hint;  // the rule tag the Ignore field is prefilled with
  std::optional<db::Rect> box;
};

class DrcPanel {
 public:
  explicit DrcPanel(ScreenContext& ctx);
  [[nodiscard]] ui::Widget& list_widget() noexcept { return list_root_; }
  [[nodiscard]] ui::Widget& inspector_widget() noexcept { return inspector_root_; }
  /// Prev / #k of n / Next, shown over the canvas while the panel is.
  [[nodiscard]] ui::Widget& stepper_widget() noexcept { return stepper_; }
  [[nodiscard]] DrcBrowserModel& model() noexcept { return model_; }

  /// Repaints with `state`; the table's rows are reset only when the model
  /// changed since the last refresh (`rows_changed`), so the selection and
  /// the scroll position survive commands.
  void refresh(const DrcPanelState& state, bool rows_changed);
  void repaint();
  /// F6: the keyboard focus to the table.
  void focus() { table_->focus(); }

 private:
  void build_list();
  void build_inspector();
  void build_stepper();
  /// `on_canvas`: painted with the canvas tokens (dark in both themes).
  std::unique_ptr<ui::Painted> make_button(std::string label, std::string tooltip, std::function<bool()> enabled,
                                           std::function<void(float, float)> click, bool on_canvas = false);
  void reset_rows();
  void on_row_selected(int row);
  void on_row_activated(int row);
  void open_style_menu(const ui::Widget& anchor);

  ScreenContext& ctx_;
  DrcBrowserModel model_;
  DrcPanelState state_;
  ui::Box list_root_ = ui::Box::horizontal();
  ui::Box list_column_ = ui::Box::vertical();
  ui::Box inspector_root_ = ui::Box::horizontal();
  ui::Box inspector_column_ = ui::Box::vertical();
  ui::Box stepper_ = ui::Box::horizontal();
  std::vector<std::unique_ptr<ui::Painted>> painted_;
  std::unique_ptr<ui::Painted> summary_;
  std::unique_ptr<ui::Painted> stepper_label_;
  std::unique_ptr<ui::Painted> notice_;
  std::unique_ptr<ui::Painted> inspector_body_;
  std::unique_ptr<ui::TextInput> filter_;
  std::unique_ptr<ui::TextInput> ignore_input_;
  std::string ignore_prefill_;  // what the Ignore field was last prefilled with
  std::unique_ptr<ui::Table> table_;
  ui::Panel list_background_;
  ui::Panel inspector_background_;
};

}  // namespace hefesto::shell
