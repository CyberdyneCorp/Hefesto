// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager panel (capability hefesto-pdk-manager; hefesto-app-ux
// "Main window layout"). It lists the catalog's variants with their install
// state (shape + text), and its buttons run `pdk` commands through the core
// Dispatcher (an install runs as `pdk install ... -progress` through a
// background job's Dispatcher, shell/pdk_job.hpp, with a progress bar) ("Headless and scripted operation": same semantics as
// scripts), so every action is logged and replayable and its result lands
// in the Console. It replaces the Inspector in the right column while open.
// Raven-free: built from the raven_ui wrappers.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/pdk/manager.hpp"
#include "hefesto/shell/pdk_job.hpp"
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
  /// Called every frame: follows a running install (progress bar) and, when
  /// it ends, records it in the session (command log, Console, Diagnostics)
  /// and refreshes the list. True while a job runs (keep frames coming).
  bool poll();

 private:
  void rebuild_rows();
  /// Sizes the header for the root error's wrapped lines and sets its tooltip.
  void layout_header();
  [[nodiscard]] float header_text_width() const;
  void run(PdkAction action);
  /// Starts `line` (install) as a background job with progress.
  void start_job(const std::string& line);
  [[nodiscard]] pdk::PdkManager& manager() noexcept { return shared_manager_ != nullptr ? *shared_manager_ : *manager_; }
  std::unique_ptr<ui::Painted> make_button(PdkAction action, paint::ButtonStyle style);
  std::unique_ptr<ui::Painted> make_toggle(std::string label, bool* value);
  // M4 script trust (board M4-Script-Consent tile 5).
  std::unique_ptr<ui::Painted> make_trust_button();
  [[nodiscard]] std::optional<PdkAction> selected_trust_action() const;
  void update_trust_button();
  [[nodiscard]] ScriptTrustState scripts_trusted(const pdk::InstalledVersion& version);

  ScreenContext& ctx_;
  std::function<void()> close_;
  std::unique_ptr<pdk::InstallHost> host_;
  std::unique_ptr<pdk::Fetcher> fetcher_;
  std::unique_ptr<pdk::PdkManager> manager_;
  pdk::PdkManager* shared_manager_ = nullptr;  // the session's (host services), else manager_
  PdkJob job_;
  PdkProgressView progress_view_;  // shown when has_progress_ (no optional: GCC 14 -Wmaybe-uninitialized)
  bool has_progress_ = false;
  bool job_failed_ = false;
  ui::Painted* progress_ = nullptr;  // owned by fixed_

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
  ui::Painted* trust_button_ = nullptr;  // owned by fixed_
  PdkInstallFlags flags_;
};

}  // namespace hefesto::shell
