// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic canvas's link to the system clipboard (capability
// schematic-editing "Clipboard"; schematic-command `clipboard` and `paste`;
// openspec/changes/m8-schematic design D15, task 13.2):
//
// - Copy (Command-C on macOS, Ctrl-C elsewhere while no netlist runs) runs
//   `schematic clipboard copy`; when that command returns the copied text,
//   the text goes to the system clipboard too, so another Hefesto window or
//   an xschem session can paste it;
// - Paste (Command-V, Ctrl-V) reads the system clipboard and runs
//   `schematic clipboard set <text>` then `schematic paste <x> <y>` at the
//   pointer, snapped to the 10-unit grid. An empty clipboard, or one larger
//   than kMaxClipboardBytes, runs nothing and gives a message instead.
//
// Every step is a command line run through the window (logged,
// replayable). The system clipboard is a ClipboardHost: Raven's clipboard in
// the app (schematic_view.cpp), a fake in the tests.
//
// Raven-free and unit-tested in tests/unit/shell/schematic_clipboard_test.cpp.
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::shell {

/// The system clipboard as the link sees it.
class ClipboardHost {
 public:
  ClipboardHost() = default;
  ClipboardHost(const ClipboardHost&) = delete;
  ClipboardHost& operator=(const ClipboardHost&) = delete;
  virtual ~ClipboardHost() = default;

  virtual void set_text(const std::string& text) = 0;
  /// The clipboard's text; empty when it holds none.
  [[nodiscard]] virtual std::string text() = 0;
};

/// The largest system clipboard text a paste hands to `schematic clipboard
/// set` (crafted-input limit, both modes): a larger text is refused with a
/// message before any command runs.
inline constexpr std::size_t kMaxClipboardBytes = std::size_t{16} * 1024U * 1024U;

/// What a paste runs, or why it runs nothing.
struct ClipboardPaste {
  std::vector<std::string> lines;  // `schematic clipboard set ...`, `schematic paste x y`
  std::string message;             // set when `lines` is empty
};

class SchematicClipboardLink {
 public:
  explicit SchematicClipboardLink(ClipboardHost& host) noexcept : host_(host) {}

  /// Copy: the command line to run; the next result of it (copied()) goes
  /// to the system clipboard.
  [[nodiscard]] std::string copy_command();
  /// A command's result, as the view observes them: the result of the
  /// copy asked for by copy_command puts its text on the system clipboard
  /// (an empty result, nothing selected, leaves it unchanged). True when
  /// the text was placed.
  bool copied(const cmd::Invocation& invocation, const cmd::CommandResult& result);
  [[nodiscard]] bool copy_pending() const noexcept { return copy_pending_; }
  /// After running the copy: a copy that never reached the dispatcher does
  /// not wait for a later one.
  void end_copy() noexcept { copy_pending_ = false; }

  /// Paste at `at` (schematic units; snapped to the grid).
  [[nodiscard]] ClipboardPaste paste_commands(schem::Point at) const;

 private:
  ClipboardHost& host_;
  bool copy_pending_ = false;
};

}  // namespace hefesto::shell
