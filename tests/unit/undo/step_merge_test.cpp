// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// UndoLog::mark / merge_since: the edits of an interrupted console command
// become one undo step named after it (openspec/changes/m4-scripting
// design.md "Long-running scripts"; board M4-Console tile 8).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace undo = hefesto::undo;
using hefesto::editor::EditorState;

namespace {

// Appends its id to a trace when undone (negative) or redone (positive).
class TraceEvent final : public undo::Event {
 public:
  TraceEvent(std::vector<int>& trace, int id) : trace_(trace), id_(id) {}
  void undo(EditorState&) override { trace_.push_back(-id_); }
  void redo(EditorState&) override { trace_.push_back(id_); }
  [[nodiscard]] std::string describe() const override { return "trace " + std::to_string(id_); }

 private:
  std::vector<int>& trace_;
  int id_;
};

// One command's step: its events, closed with its label (post_command).
void command(EditorState& editor, std::vector<int>& trace, int id) {
  editor.undo.record(std::make_unique<TraceEvent>(trace, id));
  REQUIRE(editor.undo.close_step("paint " + std::to_string(id)));
}

std::size_t undo_once(EditorState& editor) {
  auto r = editor.undo.undo(editor, 1);
  REQUIRE(r.ok());
  return r.value().steps;
}

}  // namespace

TEST_CASE("the edits of an interrupted command undo as one step named after it",
          "[cap:layout-undo][req:Event log and command grouping]") {
  EditorState editor;
  std::vector<int> trace;
  command(editor, trace, 1);  // before the console command
  const undo::StepMark mark = editor.undo.mark();
  for (int id = 2; id <= 5; ++id) command(editor, trace, id);
  // The interrupted command's last edit had not been closed yet.
  editor.undo.record(std::make_unique<TraceEvent>(trace, 6));
  CHECK(editor.undo.merge_since(mark, "ring 100000") == 5);
  CHECK(editor.undo.step_count() == 2);
  CHECK(editor.undo.step_label(0) == "paint 1");
  CHECK(editor.undo.step_label(1) == "ring 100000");
  CHECK(undo_once(editor) == 1);
  CHECK(trace == std::vector<int>{-6, -5, -4, -3, -2});  // newest first, all at once
  CHECK(editor.undo.redo(editor).steps == 1);
  CHECK(trace == std::vector<int>{-6, -5, -4, -3, -2, 2, 3, 4, 5, 6});
}

TEST_CASE("merging the steps of a command with no edits changes nothing",
          "[cap:layout-undo][req:Event log and command grouping]") {
  EditorState editor;
  std::vector<int> trace;
  command(editor, trace, 1);
  const undo::StepMark mark = editor.undo.mark();
  CHECK(editor.undo.merge_since(mark, "puts hi") == 0);
  CHECK(editor.undo.step_count() == 1);
  CHECK(editor.undo.step_label(0) == "paint 1");
  // One step since the mark is renamed after the command.
  const undo::StepMark again = editor.undo.mark();
  command(editor, trace, 2);
  CHECK(editor.undo.merge_since(again, "my_proc") == 1);
  CHECK(editor.undo.step_label(1) == "my_proc");
}

TEST_CASE("a merge keeps redoable steps and respects undo, clear and the limit",
          "[cap:layout-undo][req:Undo and redo semantics]") {
  EditorState editor;
  std::vector<int> trace;
  SECTION("the command undid its own last edit: the rest merge, the undone one stays redoable") {
    const undo::StepMark mark = editor.undo.mark();
    command(editor, trace, 1);
    command(editor, trace, 2);
    command(editor, trace, 3);
    CHECK(undo_once(editor) == 1);
    CHECK(editor.undo.merge_since(mark, "script") == 2);
    CHECK(editor.undo.step_count() == 2);
    CHECK(editor.undo.position() == 1);
    CHECK(editor.undo.step_label(1) == "paint 3");
  }
  SECTION("the command undid past the mark: nothing to merge") {
    command(editor, trace, 1);
    const undo::StepMark mark = editor.undo.mark();
    CHECK(undo_once(editor) == 1);
    CHECK(editor.undo.merge_since(mark, "undo") == 0);
    CHECK(editor.undo.step_label(0) == "paint 1");
  }
  SECTION("the log was cleared meanwhile") {
    const undo::StepMark mark = editor.undo.mark();
    command(editor, trace, 1);
    editor.undo.clear();
    command(editor, trace, 2);
    CHECK(editor.undo.merge_since(mark, "script") == 0);
    CHECK(editor.undo.step_label(0) == "paint 2");
  }
  SECTION("the history limit dropped the oldest steps meanwhile") {
    command(editor, trace, 0);
    const undo::StepMark mark = editor.undo.mark();
    const int count = static_cast<int>(undo::kDefaultHistoryLimit) + 5;
    for (int id = 1; id <= count; ++id) command(editor, trace, id);
    CHECK(editor.undo.step_count() == undo::kDefaultHistoryLimit);
    CHECK(editor.undo.merge_since(mark, "loop") == undo::kDefaultHistoryLimit);
    CHECK(editor.undo.step_count() == 1);
    CHECK(editor.undo.step_label(0) == "loop");
  }
  SECTION("recording suspended: nothing is merged") {
    const undo::StepMark mark = editor.undo.mark();
    command(editor, trace, 1);
    const undo::ScopedSuspend suspend{editor.undo};
    CHECK(editor.undo.merge_since(mark, "script") == 0);
    CHECK(editor.undo.step_label(0) == "paint 1");
  }
}

TEST_CASE("script_label names a step after the typed text",
          "[cap:hefesto-app-ux][req:Undo history]") {
  CHECK(undo::script_label("ring 100000") == "ring 100000");
  CHECK(undo::script_label("  foreach l {m1 m2} {\n\tpaint $l\n}\n") == "foreach l {m1 m2} { paint $l }");
  const std::string long_text(80, 'x');
  const std::string label = undo::script_label(long_text);
  CHECK(label.size() == undo::kMaxStepLabelLength);
  CHECK(label.ends_with("…"));
}
