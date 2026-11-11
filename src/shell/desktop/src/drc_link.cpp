// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/drc_link.hpp"

#include <utility>

#include "hefesto/drc/engine/background.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::shell {

DrcPump::DrcPump(editor::EditorState& editor, Post post, std::function<void(std::size_t)> after_service)
    : post_(std::move(post)),
      after_service_(std::move(after_service)),
      state_(std::make_shared<State>()),
      host_(editor, [this] { notify(); }) {
  state_->service = [this] { return service(); };
}

DrcPump::~DrcPump() {
  // host_ stops the worker first (members go in reverse order); a task
  // still queued finds the state's service cleared.
  state_->service = nullptr;
}

void DrcPump::notify() {
  if (state_->posted.exchange(true)) return;  // one task waits already
  std::weak_ptr<State> weak = state_;
  post_([weak] {
    const std::shared_ptr<State> state = weak.lock();
    if (!state) return;
    state->posted.store(false);
    if (state->service) (void)state->service();
  });
}

std::size_t DrcPump::service() {
  const std::size_t committed = host_.service();
  if (after_service_) after_service_(committed);
  return committed;
}

DrcLink::DrcLink(editor::EditorState& editor) : editor_(editor) {
  editor_.drc.events.add(*this);
  busy_ = editor_.drc.events.state() == drc::CheckerState::busy;
}

DrcLink::~DrcLink() {
  editor_.drc.events.remove(*this);
}

void DrcLink::on_state(drc::CheckerState state) {
  ++serial_;
  const bool was_busy = std::exchange(busy_, state == drc::CheckerState::busy);
  if (busy_ && !was_busy) busy_since_ = clock_();
  if (!busy_ && was_busy) {
    root_checked_ = root_ != nullptr;
    finished_ = std::chrono::duration<double>(clock_() - busy_since_).count();
  }
}

void DrcLink::on_square_committed(const db::CellDef& cell, const db::Rect& area) {
  ++serial_;
  ++committed_;
  if (&cell == root_) root_checked_ = true;
  // DBWAreaChanged for the error plane: every window showing the cell
  // (as its root or through expanded instances) redraws the square; only
  // the root's error tiles are drawn ("DRC error display").
  editor_.windows.area_changed(cell, area, std::nullopt);
}

void DrcLink::on_style_changed(const std::string& /*style*/) {
  ++serial_;
  ++style_changes_;
}

void DrcLink::set_root(const db::CellDef* root) {
  if (root == root_) return;
  root_ = root;
  root_checked_ = false;
  ++serial_;
}

std::optional<double> DrcLink::take_finished_job() {
  return std::exchange(finished_, std::nullopt);
}

DrcLink::Stamp DrcLink::stamp() const noexcept {
  Stamp s;
  if (editor_.drc.worker) {
    s.generation = editor_.drc.worker->generation();
    s.epoch = editor_.drc.worker->epoch();
  }
  s.style_changes = style_changes_;
  return s;
}

}  // namespace hefesto::shell
