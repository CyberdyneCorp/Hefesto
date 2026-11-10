// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::BackgroundHost (design D8 "Hosts"); the console prompt switch of
// Magic's DRCContinuous (drc/DRCcontin.c: TxSetPrompt(']') when it starts
// with work, TxSetPrompt('%') when the list is empty).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/engine/host.hpp"

#include <memory>
#include <utility>

#include "hefesto/drc/engine/background.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::drc {

BackgroundHost::BackgroundHost(editor::EditorState& editor, std::function<void()> notify) : editor_(editor) {
  if (!editor_.drc.worker) {
    editor_.drc.worker = std::make_unique<BackgroundChecker>();
  }
  BackgroundChecker& worker = *editor_.drc.worker;
  worker.stop();  // a worker another host left running gets this host's callback
  worker.set_ready_callback(std::move(notify));
  worker.start(editor_);
  worker.wake();  // work left by startup (file reads, checkpaint sections)
}

BackgroundHost::~BackgroundHost() {
  if (editor_.drc.worker) {
    editor_.drc.worker->stop();
    editor_.drc.worker->set_ready_callback({});
  }
}

std::size_t BackgroundHost::service() {
  return editor_.drc.worker ? editor_.drc.worker->commit_ready(editor_) : 0;
}

bool BackgroundHost::busy() const noexcept {
  return editor_.drc.events.state() == CheckerState::busy;
}

}  // namespace hefesto::drc
