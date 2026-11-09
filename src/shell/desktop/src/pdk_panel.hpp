// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager panel (capability hefesto-pdk-manager; hefesto-app-ux
// "Main window layout"). It lists the catalog's variants with their install
// state (shape + text), and its buttons run `pdk` commands through the core
// Dispatcher ("Headless and scripted operation": same semantics as
// scripts), so every action is logged and replayable and its result lands
// in the Console. It replaces the Inspector in the right column while open.
// Raven-free: built from the raven_ui wrappers.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hefesto/pdk/manager.hpp"
#include "hefesto/shell/pdk_panel_model.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"
#include "screen_paint.hpp"

namespace hefesto::shell {

class PdkPanel {
 public:
  /// `close` hides the panel (back to the Inspector).
  PdkPanel(ScreenContext& ctx, std::function<void()> close);
  ~PdkPanel();
  PdkPanel(const PdkPanel&) = delete;
  PdkPanel& operator=(const PdkPanel&) = delete;

  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// Re-reads the installed versions under the PDK root.
  void refresh();
  void repaint();

 private:
  void rebuild_rows();
  /// Sizes the header for the root error's wrapped lines and sets its tooltip.
  void layout_header();
  [[nodiscard]] float header_text_width() const;
  void run(PdkAction action);
  std::unique_ptr<ui::Painted> make_button(PdkAction action, paint::ButtonStyle style);
  std::unique_ptr<ui::Painted> make_toggle(std::string label, bool* value);

  ScreenContext& ctx_;
  std::function<void()> close_;
  std::unique_ptr<pdk::InstallHost> host_;
  std::unique_ptr<pdk::Fetcher> fetcher_;
  std::unique_ptr<pdk::PdkManager> manager_;

  ui::Box root_ = ui::Box::horizontal();
  ui::Panel background_;
  ui::Box column_ = ui::Box::vertical();
  ui::ScrollBox list_ = ui::ScrollBox::create();
  ui::Box rows_box_ = ui::Box::vertical();
  std::unique_ptr<ui::TextInput> version_;
  std::vector<std::unique_ptr<ui::Painted>> fixed_;  // header, buttons, toggles
  std::vector<std::unique_ptr<ui::Painted>> row_widgets_;
  std::vector<PdkRow> rows_;
  std::size_t selected_ = 0;
  std::string version_text_;
  std::string root_text_;   // "Root: <path>" (ellipsized when painted; the tooltip is whole)
  std::string root_error_;  // "■ <error>" when the root cannot be listed; wrapped in danger_text
  ui::Painted* header_ = nullptr;  // owned by fixed_
  PdkInstallFlags flags_;
};

}  // namespace hefesto::shell
