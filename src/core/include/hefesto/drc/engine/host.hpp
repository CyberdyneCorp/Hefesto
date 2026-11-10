// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hosting the background checker (design D8 "Hosts"): what an interactive
// host (hefesto-cli's interactive prompt, the desktop app's event loop)
// does around drc::BackgroundChecker, in one RAII object.
//
// A host owns one BackgroundHost for as long as it waits for user input
// between commands and does three things:
//   1. constructs it once the editor is set up (after startup): the worker
//      starts and, when the pending list already holds work, busy is
//      published at once (Magic's idle loop finding DRCHasWork);
//   2. arranges for service() to run on its command thread whenever the
//      notify callback fires (the callback runs on the worker thread and
//      must only post: write a byte to a pipe, post an event);
//   3. runs commands through cmd::Dispatcher as usual: the dispatcher
//      breaks the worker before every command and wakes it after
//      (drc::begin_command / end_command, BackgroundChecker::wake).
// prompt_char() is Magic's console prompt (TxSetPrompt): ']' while the
// checker is busy, '%' otherwise. Destroying the host stops the worker; a
// square in progress or waiting for its commit is dropped (its check tiles
// stay), as when Magic quits with work pending.
//
// Batch hosts (scripts, -c commands) never construct one (Magic in batch
// mode never reaches its idle loop inside a script).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstddef>
#include <functional>

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc {

class BackgroundHost {
 public:
  /// Starts editor.drc.worker (created if null) with `notify` as its ready
  /// callback and publishes busy when there is work. The editor must
  /// outlive the host.
  BackgroundHost(editor::EditorState& editor, std::function<void()> notify);
  ~BackgroundHost();
  BackgroundHost(const BackgroundHost&) = delete;
  BackgroundHost& operator=(const BackgroundHost&) = delete;

  /// Commits the finished square, if any (command thread, between
  /// commands); returns the number committed (0 or 1). Publishes idle when
  /// the pending list becomes empty.
  std::size_t service();

  /// True while the checker is busy (magic::drcstate busy was the last
  /// transition).
  [[nodiscard]] bool busy() const noexcept;

  /// Magic's prompt character: ']' while busy, '%' otherwise.
  [[nodiscard]] char prompt_char() const noexcept { return busy() ? kBusyPrompt : kIdlePrompt; }

  static constexpr char kBusyPrompt = ']';
  static constexpr char kIdlePrompt = '%';

 private:
  editor::EditorState& editor_;
};

}  // namespace hefesto::drc
