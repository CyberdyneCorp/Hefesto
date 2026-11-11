// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Console tab as the project's Tcl console (docs/design/mockups/
// M4-Console.dc.html, approved 2026-11-11; task 10.5): a header (filter All /
// stderr, ● running time, Interrupt, Clear, Save…), the transcript as a
// virtual Raven Table (10,000 lines cost no widget each; JetBrains Mono),
// the multi-line input (Raven's text editor) with its prompt and hint line,
// and the non-modal Interrupt card shown over the canvas after 2 s. The
// keys are decided by the window's key hook (on_key): Return runs a
// complete command, Shift-Return inserts a line, ↑/↓ walk the history
// shared with the command bar, Ctrl-C interrupts a running command.
// Raven-free: built from the raven_ui wrappers over tcl_console.hpp.
//
// Owner: SHELL (SHELL-B).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "hefesto/shell/script_consent_model.hpp"
#include "hefesto/shell/tcl_console.hpp"
#include "raven_ui.hpp"
#include "screen_context.hpp"

namespace hefesto::shell {

class ConsolePanel {
 public:
  /// `save` asks where to save the transcript (the native save dialog);
  /// nullopt: cancelled.
  using SavePath = std::function<std::optional<std::string>()>;

  ConsolePanel(ScreenContext& ctx, TclConsole& console, SavePath save_path);
  ~ConsolePanel();
  ConsolePanel(const ConsolePanel&) = delete;
  ConsolePanel& operator=(const ConsolePanel&) = delete;

  /// The Console tab's page.
  [[nodiscard]] ui::Widget& widget() noexcept { return root_; }
  /// The Interrupt card, added over the canvas by the window.
  [[nodiscard]] ui::Widget& card_widget() noexcept { return *card_; }

  /// The card of a script that is not the console's (a menu callback, a
  /// PDK file; board M4-Script-Consent tile 7), built from the session's
  /// progress report (TclConsoleSession::progress); `interrupt` stops it.
  using ScriptCard = std::function<std::optional<InterruptCardView>()>;
  void set_script_card(ScriptCard card, std::function<void()> interrupt);

  /// The window's key hook: true when the console used the key.
  bool on_key(int key, std::uint8_t modifiers);
  /// Rebuilds what changed (call between frames and from the run's pump).
  void refresh();
  void repaint();
  /// F6 and `:`: the keyboard focus to the input.
  void focus();
  [[nodiscard]] bool input_focused() const;
  /// The Interrupt card is shown (refresh() decided it).
  [[nodiscard]] bool card_shown() const noexcept { return card_shown_; }

 private:
  void build_header();
  void build_input();
  void sync_input_from_model();
  void save_transcript();
  void reset_rows();
  void paint_script_card(ui::Painter& p, const InterruptCardView& card, const ThemeTokens& t, bool hov, bool foc) const;
  void interrupt_shown();
  [[nodiscard]] bool card_wanted() const;

  ScreenContext& ctx_;
  TclConsole& console_;
  SavePath save_path_;
  ui::Box root_ = ui::Box::vertical();
  std::unique_ptr<ui::Painted> header_;
  std::unique_ptr<ui::Table> transcript_;
  std::unique_ptr<ui::Painted> prompt_;
  std::unique_ptr<ui::TextEditor> input_;
  std::unique_ptr<ui::Painted> hint_;
  std::unique_ptr<ui::Painted> card_;
  std::vector<std::unique_ptr<ui::Painted>> buttons_;
  std::vector<std::size_t> rows_;  // transcript line per table row (filter)
  std::uint64_t shown_revision_ = ~std::uint64_t{0};
  std::size_t shown_lines_ = 0;
  std::size_t shown_dropped_ = 0;
  bool card_shown_ = false;
  ScriptCard script_card_;
  std::function<void()> script_interrupt_;
  bool script_interrupt_requested_ = false;
};

}  // namespace hefesto::shell
