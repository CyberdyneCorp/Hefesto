// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// undo::UndoLog (layout-undo "Event log and command grouping", "Undo and
// redo semantics", "History limit", "Non-recorded operations"). Owner: UNDO.
// Ported in behavior from Magic VLSI 8.3.694 (fc5daa9e) utils/undo.c
// (UndoNewEvent, UndoNext, UndoBackward, UndoForward, undoFreeHead,
// undoMemTruncate). Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).

#include "hefesto/undo/undo_log.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <system_error>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/diag_codes.hpp"

namespace hefesto::undo {

namespace {

constexpr std::string_view kDelimiter = "(delimiter)";

db::Rect unite(const db::Rect& a, const db::Rect& b) noexcept {
  if (a.is_null()) {
    return b;
  }
  if (b.is_null()) {
    return a;
  }
  return db::Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

}  // namespace

UndoLog::UndoLog() = default;
UndoLog::~UndoLog() = default;

void UndoLog::record(std::unique_ptr<Event> event) {
  if (!recording() || event == nullptr) {
    return;
  }
  discard_undone();
  open_.push_back(std::move(event));
}

Event* UndoLog::open_back() noexcept {
  return open_.empty() ? nullptr : open_.back().get();
}

bool UndoLog::close_step(std::string_view label) {
  if (suspended() || open_.empty()) {
    return false;
  }
  discard_undone();
  steps_.push_back(std::move(open_));
  labels_.emplace_back(label);
  open_.clear();
  position_ = steps_.size();
  apply_limit();
  return true;
}

void UndoLog::discard_undone() {
  // undoMemTruncate: the undone steps go when something new is recorded.
  while (steps_.size() > position_) {
    steps_.pop_back();
    labels_.pop_back();
  }
}

void UndoLog::apply_limit() {
  // undoFreeHead: keep the `limit_` most recent steps.
  while (steps_.size() > limit_) {
    steps_.pop_front();
    labels_.pop_front();
    if (position_ > 0) {
      --position_;
    }
  }
}

diag::Outcome<ReplayResult> UndoLog::undo(editor::EditorState& editor, std::size_t steps, bool magic_compat) {
  if (suspended()) {
    // UndoBackward (MAGIC_WRAPPER): refuses while recording is disabled.
    return diag::make(codes::kUndoDisabled, "Attempted undo with undo disabled. . . abort function.");
  }
  (void)close_step();
  ReplayResult result;
  if (steps == 0 || position_ == 0) {
    return result;
  }
  begin_replay(editor);
  while (result.steps < steps && position_ > 0) {
    --position_;
    replay(editor, steps_[position_], true);
    ++result.steps;
  }
  end_replay(editor, magic_compat);
  return result;
}

ReplayResult UndoLog::redo(editor::EditorState& editor, std::size_t steps, bool magic_compat) {
  ReplayResult result;
  if (steps == 0 || position_ >= steps_.size()) {
    return result;
  }
  begin_replay(editor);
  while (result.steps < steps && position_ < steps_.size()) {
    replay(editor, steps_[position_], false);
    ++position_;
    ++result.steps;
  }
  end_replay(editor, magic_compat);
  return result;
}

void UndoLog::replay(editor::EditorState& editor, Step& step, bool backward) {
  if (backward) {
    for (auto it = step.rbegin(); it != step.rend(); ++it) {
      (*it)->undo(editor);
    }
  } else {
    for (auto& event : step) {
      event->redo(editor);
    }
  }
}

void UndoLog::begin_replay(editor::EditorState& editor) {
  replaying_ = true;
  live_.clear();
  editor.cells.for_each([this](db::CellDef& def) { live_.insert(&def); });
  changed_.clear();
}

void UndoLog::end_replay(editor::EditorState& editor, bool magic_compat) {
  // After a replay the changed cells' bounding boxes are recomputed and the
  // changed areas redisplayed (dbUndoCloseCell, DBReComputeBbox,
  // DBWAreaChanged). The events queue their DRC areas themselves as they
  // replay (drc::check_this, db_recorder.cpp).
  std::vector<Changed> merged;
  for (const Changed& c : changed_) {
    auto it = std::find_if(merged.begin(), merged.end(), [&](const Changed& m) { return m.def == c.def; });
    if (it == merged.end()) {
      merged.push_back(c);
    } else {
      it->area = unite(it->area, c.area);
    }
  }
  for (const Changed& c : merged) {
    if (live_.count(c.def) == 0) {
      continue;
    }
    db::recompute_bbox(*c.def, magic_compat);
    if (!c.area.is_null()) {
      editor.windows.area_changed(*c.def, c.area, std::nullopt);
    }
  }
  changed_.clear();
  live_.clear();
  replaying_ = false;
}

bool UndoLog::is_live(const db::CellDef* def) const {
  if (def == nullptr) {
    return false;
  }
  return !replaying_ || live_.count(def) != 0;
}

void UndoLog::note_changed(db::CellDef& def, const db::Rect& area) {
  changed_.push_back(Changed{&def, area});
}

std::string UndoLog::print(std::int64_t count, bool before) const {
  // The entries on one side of the current position, nearest first.
  std::vector<std::string> entries;
  const auto add_step = [&](const Step& step, bool reversed) {
    if (reversed) {
      for (auto it = step.rbegin(); it != step.rend(); ++it) {
        entries.push_back((*it)->describe());
      }
    } else {
      for (const auto& event : step) {
        entries.push_back(event->describe());
      }
    }
  };
  if (before) {
    add_step(open_, true);
    for (std::size_t i = position_; i > 0; --i) {
      entries.emplace_back(kDelimiter);
      add_step(steps_[i - 1], true);
    }
  } else {
    for (std::size_t i = position_; i < steps_.size(); ++i) {
      add_step(steps_[i], false);
      entries.emplace_back(kDelimiter);
    }
  }
  const std::size_t shown =
      count > 0 ? std::min(entries.size(), static_cast<std::size_t>(count)) : entries.size();
  std::string out;
  for (std::size_t i = 0; i < shown; ++i) {
    out += entries[i];
    out += '\n';
  }
  return out;
}

void UndoLog::suspend() noexcept {
  ++suspend_depth_;
}

void UndoLog::resume() noexcept {
  if (suspend_depth_ > 0) {
    --suspend_depth_;
  }
}

bool UndoLog::set_limit(std::size_t steps) noexcept {
  if (steps < kDefaultHistoryLimit) {
    return false;
  }
  limit_ = steps;
  return true;
}

std::string UndoLog::step_label(std::size_t index) const {
  if (index >= steps_.size()) {
    return {};
  }
  if (!labels_[index].empty()) {
    return labels_[index];
  }
  const Step& step = steps_[index];
  return step.empty() ? std::string{} : step.back()->describe();
}

void UndoLog::clear() {
  steps_.clear();
  labels_.clear();
  open_.clear();
  position_ = 0;
}

void UndoLog::cell_deleted(const db::CellDef& def) {
  if (replaying_) {
    return;
  }
  const auto refers = [&](const Step& step) {
    return std::any_of(step.begin(), step.end(), [&](const auto& e) { return e->references(def); });
  };
  if (refers(open_) || std::any_of(steps_.begin(), steps_.end(), refers)) {
    clear();
  }
}

namespace {

std::string label_word(const std::string& word) {
  const bool spaced = word.empty() || word.find_first_of(" \t\n") != std::string::npos;
  return spaced ? '"' + word + '"' : word;
}

}  // namespace

std::string command_label(std::string_view name, const std::vector<std::string>& args) {
  std::vector<std::string> words;
  if (name != "*bypass") {
    words.emplace_back(name);
  }
  for (const std::string& arg : args) {
    words.push_back(words.empty() ? arg : label_word(arg));
  }
  std::string label;
  for (const std::string& word : words) {
    if (!label.empty()) {
      label += ' ';
    }
    label += word;
  }
  if (label.size() > kMaxStepLabelLength) {
    // Room for the 3-byte ellipsis, cut at a UTF-8 character boundary.
    std::size_t cut = kMaxStepLabelLength - 3;
    while (cut > 0 && (static_cast<unsigned char>(label[cut]) & 0xC0U) == 0x80U) {
      --cut;
    }
    label = label.substr(0, cut) + "\u2026";
  }
  return label;
}

std::vector<HistoryEntry> history(const UndoLog& log, std::size_t max_each) {
  std::vector<HistoryEntry> out;
  const std::size_t position = log.position();
  const std::size_t redoable = std::min(log.step_count() - position, max_each);
  for (std::size_t n = redoable; n > 0; --n) {
    out.push_back(HistoryEntry{log.step_label(position + n - 1), "redo " + std::to_string(n), true});
  }
  const std::size_t undoable = std::min(position, max_each);
  for (std::size_t n = 1; n <= undoable; ++n) {
    out.push_back(HistoryEntry{log.step_label(position - n), "undo " + std::to_string(n), false});
  }
  return out;
}

diag::Status set_history_limit(UndoLog& log, std::string_view value) {
  std::size_t steps = 0;
  const char* first = value.data();
  const char* last = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(first, last, steps);
  const bool parsed = ec == std::errc{} && ptr == last && first != last;
  if (!parsed || !log.set_limit(steps)) {
    return diag::make(codes::kHistoryLimit, std::string{kHistoryLimitSetting} + " must be an integer of at least " +
                                                std::to_string(kDefaultHistoryLimit) + " (got \"" +
                                                std::string{value} + "\"); it stays " +
                                                std::to_string(log.limit()));
  }
  return diag::Done{};
}

}  // namespace hefesto::undo
