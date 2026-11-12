// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The undo event log (capability layout-undo; design.md D5): one linear log
// of reversible events grouped into steps, one step per user command. The
// database reports primitive changes through undo::DbRecorder
// (db::ChangeRecorder); editor-level events (box, selection, edit cell,
// wiring, color map) and Hefesto's native compound events are recorded by
// their areas with UndoLog::record() (undo/events.hpp has reusable ones).
// editor::post_command closes the step and names it after the command
// (`close_step(command_label(...))`, hefesto-app-ux "Undo history").
// (Magic ref: utils/undo.c, database/DBundo.c, dbwind/DBWundo.c)
//
// Model (Magic's utils/undo.c with its delimiters made explicit):
//   - closed steps, oldest first; the first `position` of them are done,
//     the rest have been undone and can be redone;
//   - the open step: events recorded since the last close_step().
// record() first discards every undone step (the redo-discard rule).
// close_step() turns a non-empty open step into a done step, unless
// recording is suspended (Magic's UndoNext keeps the events "recent" then),
// and drops the oldest steps beyond the history limit.
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::undo {

/// One reversible change. Events of a step are undone newest first and
/// redone oldest first. Replay runs with recording suspended; an event that
/// changes a cell marks it modified (Q-MODEL-12) and reports the area with
/// UndoLog::note_changed() so the log recomputes bounding boxes and
/// redisplays once per step. An event must check UndoLog::is_live() before
/// touching a cell it holds a pointer to.
class Event {
 public:
  Event() = default;
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  virtual ~Event() = default;

  virtual void undo(editor::EditorState& editor) = 0;
  virtual void redo(editor::EditorState& editor) = 0;
  /// One line for `undo print` / `redo print` (the format is informative).
  [[nodiscard]] virtual std::string describe() const = 0;
  /// True when the event holds a pointer to `def` (UndoLog::cell_deleted).
  [[nodiscard]] virtual bool references(const db::CellDef& def) const {
    (void)def;
    return false;
  }
};

/// Magic's undo history limit, in steps ("History limit", Q-MODEL-21).
inline constexpr std::size_t kDefaultHistoryLimit = 1000;

/// Name of the setting that holds the history limit (design.md D5).
inline constexpr std::string_view kHistoryLimitSetting = "undo-history-limit";

/// What undo()/redo() did.
struct ReplayResult {
  std::size_t steps = 0;  // steps replayed; 0: "Nothing more to undo/redo"
};

class UndoLog {
 public:
  UndoLog();
  ~UndoLog();
  UndoLog(const UndoLog&) = delete;
  UndoLog& operator=(const UndoLog&) = delete;

  /// Appends `event` to the open step, discarding every undone step first
  /// (the redo-discard rule). Ignored while suspended or replaying.
  void record(std::unique_ptr<Event> event);

  /// True when record() would keep an event (not suspended, not replaying).
  /// Recorders test it before building an event.
  [[nodiscard]] bool recording() const noexcept { return suspend_depth_ == 0 && !replaying_; }

  /// The newest event of the open step (nullptr when it is empty), so a
  /// recorder can extend it instead of adding one (DbRecorder groups the
  /// consecutive tile changes of one cell into one event).
  [[nodiscard]] Event* open_back() noexcept;

  /// Closes the open step if it recorded something ("Event log and command
  /// grouping"); returns true when a step was closed. Applies the limit.
  /// While recording is suspended nothing is closed (Magic's UndoNext).
  /// `label` names the step after the user command that made it ("paint
  /// metal1", see command_label); empty: the step is named by its newest
  /// event (step_label). The label is Hefesto's: `undo print` is unchanged.
  bool close_step(std::string_view label = {});

  /// The name of closed step `index` (0 = the oldest kept, step_count() - 1
  /// = the newest): its label, or without one its newest event's
  /// description. Empty when `index` is out of range.
  [[nodiscard]] std::string step_label(std::size_t index) const;

  /// Undoes up to `steps` steps (an open step is closed first). Fails with
  /// UNDO-CMD-002 while recording is suspended (Magic refuses then).
  /// `magic_compat` selects the bounding-box rule (Q-MODEL-19) used for
  /// the cells the replay changed.
  [[nodiscard]] diag::Outcome<ReplayResult> undo(editor::EditorState& editor, std::size_t steps = 1,
                                                 bool magic_compat = true);
  /// Redoes up to `steps` undone steps (allowed while suspended, as Magic).
  [[nodiscard]] ReplayResult redo(editor::EditorState& editor, std::size_t steps = 1, bool magic_compat = true);

  /// `undo print N` (`before`): up to `count` entries before the current
  /// position, newest first; `redo print N`: up to `count` entries after
  /// it, oldest first. count <= 0 lists to the end. One entry per line:
  /// an event description or "(delimiter)" for a step boundary.
  [[nodiscard]] std::string print(std::int64_t count, bool before) const;

  /// The nesting counter shared by `undo disable/enable`, `undo 0`/`redo 0`
  /// (magic-compat ON) and internal suspensions ("Non-recorded
  /// operations"; see ScopedSuspend). resume() never goes below zero.
  void suspend() noexcept;
  void resume() noexcept;
  [[nodiscard]] bool suspended() const noexcept { return suspend_depth_ > 0; }
  [[nodiscard]] int suspend_depth() const noexcept { return suspend_depth_; }

  /// The history limit in steps (setting `undo-history-limit`); values
  /// below kDefaultHistoryLimit are rejected (returns false, the limit is
  /// unchanged). Lowering it drops the oldest steps at the next close.
  bool set_limit(std::size_t steps) noexcept;
  [[nodiscard]] std::size_t limit() const noexcept { return limit_; }

  /// Forgets every step, the open one included.
  void clear();

  /// "Deleting the cell whose events are being recorded clears the whole
  /// history": clears the log when an event references `def`. Called
  /// before `def` is destroyed; ignored during a replay (a native event
  /// that deletes a cell it created).
  void cell_deleted(const db::CellDef& def);

  /// Number of closed steps (done and undone) and of done steps.
  [[nodiscard]] std::size_t step_count() const noexcept { return steps_.size(); }
  [[nodiscard]] std::size_t position() const noexcept { return position_; }
  /// Number of events in the open step.
  [[nodiscard]] std::size_t open_event_count() const noexcept { return open_.size(); }

  // ---- for events, during a replay ----

  /// True when `def` is still a definition of the session's library (or no
  /// replay is running). Events skip cells deleted since they were recorded.
  [[nodiscard]] bool is_live(const db::CellDef* def) const;
  /// Reports that a replayed event changed `area` of `def` (an empty area:
  /// only the bounding box needs recomputing).
  void note_changed(db::CellDef& def, const db::Rect& area);
  /// Queues a line a replayed event prints (TxPrintf from an undo client,
  /// e.g. "Edit cell is now ..."); the undo/redo command reports them.
  void note(std::string text) { notes_.push_back(std::move(text)); }
  /// The lines queued by note() since the last call, oldest first.
  [[nodiscard]] std::vector<std::string> take_notes() { return std::exchange(notes_, {}); }

 private:
  using Step = std::vector<std::unique_ptr<Event>>;

  struct Changed {
    db::CellDef* def = nullptr;
    db::Rect area;
  };

  static void replay(editor::EditorState& editor, Step& step, bool backward);
  void begin_replay(editor::EditorState& editor);
  void end_replay(editor::EditorState& editor, bool magic_compat);
  void discard_undone();
  void apply_limit();

  std::deque<Step> steps_;
  std::deque<std::string> labels_;  // one per closed step, parallel to steps_
  std::size_t position_ = 0;
  Step open_;
  int suspend_depth_ = 0;
  std::size_t limit_ = kDefaultHistoryLimit;
  bool replaying_ = false;
  std::unordered_set<const db::CellDef*> live_;
  std::vector<Changed> changed_;
  std::vector<std::string> notes_;
};

/// Suspends recording for its lifetime ("Non-recorded operations": file
/// reads, flatten into a new cell, cell rename and delete, scratch cells).
/// A null log does nothing, so callers without a session can use it.
class ScopedSuspend {
 public:
  explicit ScopedSuspend(UndoLog* log) noexcept : log_(log) {
    if (log_ != nullptr) {
      log_->suspend();
    }
  }
  explicit ScopedSuspend(UndoLog& log) noexcept : ScopedSuspend(&log) {}
  ~ScopedSuspend() {
    if (log_ != nullptr) {
      log_->resume();
    }
  }
  ScopedSuspend(const ScopedSuspend&) = delete;
  ScopedSuspend& operator=(const ScopedSuspend&) = delete;

 private:
  UndoLog* log_;
};

/// The undo-step label of a user command: its name and arguments as typed,
/// separated by spaces ("paint metal1"); a word with white space is quoted
/// with double quotes; a leading `*bypass` is dropped (the pick tool's drag
/// commands are named by the command they bypass). At most
/// kMaxStepLabelLength bytes, cut with "…".
inline constexpr std::size_t kMaxStepLabelLength = 60;
[[nodiscard]] std::string command_label(std::string_view name, const std::vector<std::string>& args);

/// One entry of the undo history (hefesto-app-ux "Undo history"): a step
/// named by its label (UndoLog::step_label) and the command that reaches it
/// (`undo N` / `redo N`).
struct HistoryEntry {
  std::string label;
  std::string command;
  bool redo = false;

  friend bool operator==(const HistoryEntry&, const HistoryEntry&) = default;
};

/// The undo history as the desktop shows it: first the undone steps that
/// can be redone, farthest first (`redo N`), then the steps that can be
/// undone, newest first (`undo N`); at most `max_each` of each.
[[nodiscard]] std::vector<HistoryEntry> history(const UndoLog& log, std::size_t max_each = 8);

/// Applies a value of the `undo-history-limit` setting: a decimal integer
/// >= kDefaultHistoryLimit. Anything else fails with UNDO-LIMIT-001 and
/// keeps the previous limit ("History limit", Q-MODEL-21).
[[nodiscard]] diag::Status set_history_limit(UndoLog& log, std::string_view value);

}  // namespace hefesto::undo
