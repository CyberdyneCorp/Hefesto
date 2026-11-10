// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// undo::UndoLog with test events (capability layout-undo): grouping, the
// redo-discard rule, the history limit, suspension, `print`, history
// clearing on cell deletion, box events and native compound events.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/diag_codes.hpp"
#include "hefesto/undo/events.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace undo = hefesto::undo;
namespace db = hefesto::db;
using hefesto::editor::EditorState;

namespace {

// Appends its id to a trace when undone (negative) or redone (positive).
class TraceEvent final : public undo::Event {
 public:
  TraceEvent(std::vector<int>& trace, int id, const db::CellDef* cell = nullptr)
      : trace_(trace), id_(id), cell_(cell) {}
  void undo(EditorState&) override { trace_.push_back(-id_); }
  void redo(EditorState&) override { trace_.push_back(id_); }
  [[nodiscard]] std::string describe() const override { return "trace " + std::to_string(id_); }
  [[nodiscard]] bool references(const db::CellDef& def) const override { return &def == cell_; }

 private:
  std::vector<int>& trace_;
  int id_;
  const db::CellDef* cell_;
};

// One step of `ids`, closed.
void step(undo::UndoLog& log, std::vector<int>& trace, std::initializer_list<int> ids) {
  for (const int id : ids) {
    log.record(std::make_unique<TraceEvent>(trace, id));
  }
  REQUIRE(log.close_step());
}

std::size_t undo_n(EditorState& editor, std::size_t n) {
  auto r = editor.undo.undo(editor, n);
  REQUIRE(r.ok());
  return r.value().steps;
}

}  // namespace

TEST_CASE("events of one command form one step, replayed in recording order",
          "[cap:layout-undo][req:Event log and command grouping]") {
  EditorState editor;
  std::vector<int> trace;
  step(editor.undo, trace, {1, 2, 3});
  CHECK(editor.undo.step_count() == 1);
  CHECK(undo_n(editor, 1) == 1);
  CHECK(trace == std::vector<int>{-3, -2, -1});
  trace.clear();
  CHECK(editor.undo.redo(editor).steps == 1);
  CHECK(trace == std::vector<int>{1, 2, 3});
}

TEST_CASE("a command without events closes no step", "[cap:layout-undo][req:Event log and command grouping]") {
  EditorState editor;
  std::vector<int> trace;
  step(editor.undo, trace, {1});
  CHECK_FALSE(editor.undo.close_step());  // the information-only command
  CHECK(editor.undo.step_count() == 1);
  CHECK(undo_n(editor, 1) == 1);
  CHECK(trace == std::vector<int>{-1});
}

TEST_CASE("undo closes an open step first", "[cap:layout-undo][req:Event log and command grouping]") {
  EditorState editor;
  std::vector<int> trace;
  editor.undo.record(std::make_unique<TraceEvent>(trace, 7));
  CHECK(editor.undo.open_event_count() == 1);
  CHECK(undo_n(editor, 1) == 1);
  CHECK(trace == std::vector<int>{-7});
}

TEST_CASE("a new event discards every undone step", "[cap:layout-undo][req:Undo and redo semantics]") {
  EditorState editor;
  std::vector<int> trace;
  step(editor.undo, trace, {1});
  step(editor.undo, trace, {2});
  step(editor.undo, trace, {3});
  CHECK(undo_n(editor, 2) == 2);  // S3, S2
  step(editor.undo, trace, {4});
  CHECK(editor.undo.step_count() == 2);
  CHECK(editor.undo.redo(editor).steps == 0);
  trace.clear();
  CHECK(undo_n(editor, 5) == 2);
  CHECK(trace == std::vector<int>{-4, -1});
}

TEST_CASE("undo and redo stop at the ends of the log", "[cap:layout-undo][req:Undo and redo semantics]") {
  EditorState editor;
  std::vector<int> trace;
  CHECK(undo_n(editor, 1) == 0);
  step(editor.undo, trace, {1});
  CHECK(undo_n(editor, 5) == 1);
  CHECK(undo_n(editor, 1) == 0);
  CHECK(editor.undo.redo(editor, 9).steps == 1);
  CHECK(editor.undo.redo(editor).steps == 0);
}

TEST_CASE("replay records nothing", "[cap:layout-undo][req:Undo and redo semantics]") {
  EditorState editor;
  std::vector<int> trace;
  // An event that records another event while it is replayed.
  class Recording final : public undo::Event {
   public:
    explicit Recording(std::vector<int>& t) : t_(t) {}
    void undo(EditorState& e) override { e.undo.record(std::make_unique<TraceEvent>(t_, 99)); }
    void redo(EditorState& e) override { e.undo.record(std::make_unique<TraceEvent>(t_, 99)); }
    [[nodiscard]] std::string describe() const override { return "recording"; }

   private:
    std::vector<int>& t_;
  };
  editor.undo.record(std::make_unique<Recording>(trace));
  REQUIRE(editor.undo.close_step());
  CHECK(undo_n(editor, 1) == 1);
  CHECK(editor.undo.open_event_count() == 0);
  CHECK(editor.undo.redo(editor).steps == 1);
  CHECK(editor.undo.open_event_count() == 0);
  CHECK(editor.undo.step_count() == 1);
}

TEST_CASE("the history keeps the limit's most recent steps", "[cap:layout-undo][req:History limit]") {
  EditorState editor;
  std::vector<int> trace;
  CHECK(editor.undo.limit() == 1000);

  SECTION("1001 commands with the default limit") {
    for (int i = 1; i <= 1001; ++i) {
      step(editor.undo, trace, {i});
    }
    CHECK(editor.undo.step_count() == 1000);
    trace.clear();
    CHECK(undo_n(editor, 1001) == 1000);
    CHECK(trace.back() == -2);  // the first paint remains
    CHECK(undo_n(editor, 1) == 0);
  }

  SECTION("1500 commands with the limit set to 2000") {
    REQUIRE(undo::set_history_limit(editor.undo, "2000").ok());
    for (int i = 1; i <= 1500; ++i) {
      step(editor.undo, trace, {i});
    }
    trace.clear();
    CHECK(undo_n(editor, 1500) == 1500);
    CHECK(trace.back() == -1);
  }

  SECTION("a limit below Magic's is rejected") {
    REQUIRE(undo::set_history_limit(editor.undo, "2000").ok());
    const auto r = undo::set_history_limit(editor.undo, "500");
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == undo::codes::kHistoryLimit);
    CHECK(editor.undo.limit() == 2000);
    CHECK_FALSE(undo::set_history_limit(editor.undo, "lots").ok());
    CHECK_FALSE(undo::set_history_limit(editor.undo, "").ok());
    CHECK_FALSE(editor.undo.set_limit(999));
    CHECK(editor.undo.limit() == 2000);
  }
}

TEST_CASE("suspension nests and drops events", "[cap:layout-undo][req:Non-recorded operations]") {
  EditorState editor;
  std::vector<int> trace;
  {
    undo::ScopedSuspend outer(editor.undo);
    {
      undo::ScopedSuspend inner(&editor.undo);
      editor.undo.record(std::make_unique<TraceEvent>(trace, 1));
    }
    CHECK(editor.undo.suspended());
    editor.undo.record(std::make_unique<TraceEvent>(trace, 2));
  }
  CHECK_FALSE(editor.undo.suspended());
  CHECK_FALSE(editor.undo.close_step());
  CHECK(editor.undo.step_count() == 0);
  undo::ScopedSuspend none(nullptr);  // no session: nothing to suspend
}

TEST_CASE("a step is not closed while recording is suspended", "[cap:layout-undo][req:Non-recorded operations]") {
  // Magic's UndoNext returns early while disabled: the events stay in the
  // open step and the next command's close takes them along.
  EditorState editor;
  std::vector<int> trace;
  editor.undo.record(std::make_unique<TraceEvent>(trace, 1));
  editor.undo.suspend();
  CHECK_FALSE(editor.undo.close_step());
  editor.undo.resume();
  editor.undo.record(std::make_unique<TraceEvent>(trace, 2));
  CHECK(editor.undo.close_step());
  CHECK(editor.undo.step_count() == 1);
}

TEST_CASE("undo refuses while recording is suspended; redo does not",
          "[cap:layout-undo][req:undo command][req:redo command]") {
  EditorState editor;
  std::vector<int> trace;
  step(editor.undo, trace, {1});
  editor.undo.suspend();
  const auto refused = editor.undo.undo(editor);
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.error().code == undo::codes::kUndoDisabled);
  CHECK(trace.empty());
  editor.undo.resume();
  CHECK(undo_n(editor, 1) == 1);
  editor.undo.suspend();
  CHECK(editor.undo.redo(editor).steps == 1);  // UndoForward has no check
}

TEST_CASE("deleting a cell with recorded events clears the history",
          "[cap:layout-undo][req:Non-recorded operations]") {
  EditorState editor;
  std::vector<int> trace;
  db::CellDef* a = editor.cells.create("a", 4).value();
  db::CellDef* b = editor.cells.create("b", 4).value();
  editor.undo.record(std::make_unique<TraceEvent>(trace, 1, a));
  REQUIRE(editor.undo.close_step());
  editor.undo.cell_deleted(*b);  // no event of b: the history stays
  CHECK(editor.undo.step_count() == 1);
  editor.undo.cell_deleted(*a);
  CHECK(editor.undo.step_count() == 0);
  CHECK(undo_n(editor, 1) == 0);
}

TEST_CASE("undo print and redo print list the log around the position", "[cap:layout-undo][req:undo command]") {
  EditorState editor;
  std::vector<int> trace;
  step(editor.undo, trace, {1});
  step(editor.undo, trace, {2, 3});
  CHECK(editor.undo.print(0, true) == "(delimiter)\ntrace 3\ntrace 2\n(delimiter)\ntrace 1\n");
  CHECK(editor.undo.print(2, true) == "(delimiter)\ntrace 3\n");
  CHECK(editor.undo.print(0, false).empty());
  CHECK(undo_n(editor, 1) == 1);
  CHECK(editor.undo.print(-1, false) == "trace 2\ntrace 3\n(delimiter)\n");
  CHECK(editor.undo.print(1, false) == "trace 2\n");
  CHECK(editor.undo.print(0, true) == "(delimiter)\ntrace 1\n");
}

TEST_CASE("box changes are undone and redone", "[cap:layout-undo][req:Recorded event kinds]") {
  EditorState editor;
  db::CellDef* top = editor.cells.create("top", 4).value();
  editor.box = db::Rect{0, 0, 10, 10};
  editor.box_root = top;
  const undo::BoxState before = undo::box_state(editor);
  editor.box = db::Rect{20, 20, 30, 30};
  undo::record_box_change(editor, before);
  undo::record_box_change(editor, undo::box_state(editor));  // no change: nothing recorded
  REQUIRE(editor.undo.close_step());
  CHECK(editor.undo.step_count() == 1);
  CHECK(undo_n(editor, 1) == 1);
  CHECK(editor.box == db::Rect{0, 0, 10, 10});
  CHECK(editor.box_root == top);
  CHECK(editor.undo.redo(editor).steps == 1);
  CHECK(editor.box == db::Rect{20, 20, 30, 30});
}

namespace {

// Test producers of a native operation (an accepted agent proposal): the
// parts ordinary commands never record.
class CreateCellEvent final : public undo::Event {
 public:
  explicit CreateCellEvent(std::string name) : name_(std::move(name)) {}
  void undo(EditorState& e) override {
    if (db::CellDef* def = e.cells.find(name_); def != nullptr) {
      (void)e.cells.remove(*def);
    }
  }
  void redo(EditorState& e) override { (void)e.cells.create(name_, 4); }
  [[nodiscard]] std::string describe() const override { return "create cell " + name_; }

 private:
  std::string name_;
};

class SetPropertyEvent final : public undo::Event {
 public:
  SetPropertyEvent(std::string cell, std::string key, std::string value)
      : cell_(std::move(cell)), key_(std::move(key)), value_(std::move(value)) {}
  void undo(EditorState& e) override { db::set_property(*e.cells.find(cell_), key_, std::nullopt); }
  void redo(EditorState& e) override { db::set_property(*e.cells.find(cell_), key_, db::PropertyValue{value_}); }
  [[nodiscard]] std::string describe() const override { return "property " + key_; }

 private:
  std::string cell_;
  std::string key_;
  std::string value_;
};

// A testbench edit: restores the previous file content and "saves" it.
class TestbenchEvent final : public undo::Event {
 public:
  TestbenchEvent(std::string& file, std::string before, std::string after, int& saves)
      : file_(file), before_(std::move(before)), after_(std::move(after)), saves_(saves) {}
  void undo(EditorState&) override {
    file_ = before_;
    ++saves_;
  }
  void redo(EditorState&) override {
    file_ = after_;
    ++saves_;
  }
  [[nodiscard]] std::string describe() const override { return "testbench edit"; }

 private:
  std::string& file_;
  std::string before_;
  std::string after_;
  int& saves_;
};

}  // namespace

TEST_CASE("a native compound event undoes an accepted proposal as one step",
          "[cap:layout-undo][req:Native undo events for Hefesto compound operations]") {
  EditorState editor;
  REQUIRE(editor.cells.create("top", 4).ok());
  // Applying the proposal: the producer applies each part and records them
  // as one compound event, whatever magic-compat says.
  auto proposal = std::make_unique<undo::CompoundEvent>("agent proposal");
  db::set_property(*editor.cells.find("top"), "FIXED_BBOX", db::PropertyValue{std::string{"0 0 10 10"}});
  proposal->add(std::make_unique<SetPropertyEvent>("top", "FIXED_BBOX", "0 0 10 10"));
  REQUIRE(editor.cells.create("inv_x1", 4).ok());
  proposal->add(std::make_unique<CreateCellEvent>("inv_x1"));
  proposal->add(nullptr);  // ignored
  CHECK(proposal->size() == 2);
  editor.undo.record(std::move(proposal));
  REQUIRE(editor.undo.close_step());

  CHECK(undo_n(editor, 1) == 1);
  CHECK(editor.cells.find("inv_x1") == nullptr);
  CHECK(editor.cells.find("top")->properties().find("FIXED_BBOX") == nullptr);
  CHECK(editor.undo.step_count() == 1);  // deleting inv_x1 during replay keeps the log

  CHECK(editor.undo.redo(editor).steps == 1);
  CHECK(editor.cells.find("inv_x1") != nullptr);
  CHECK(editor.cells.find("top")->properties().find("FIXED_BBOX") != nullptr);
  CHECK(editor.undo.print(0, true) == "(delimiter)\nagent proposal (2 parts)\n");
}

TEST_CASE("a testbench edit is one step that restores and saves the previous content",
          "[cap:layout-undo][req:Native undo events for Hefesto compound operations]") {
  EditorState editor;
  std::string testbench = R"({"analyses":[]})";
  int saves = 0;
  const std::string before = testbench;
  testbench = R"({"analyses":[{"tran":["10ps","20ns"]}]})";
  editor.undo.record(std::make_unique<TestbenchEvent>(testbench, before, testbench, saves));
  REQUIRE(editor.undo.close_step());
  CHECK(undo_n(editor, 1) == 1);
  CHECK(testbench == before);
  CHECK(saves == 1);
  // Native steps obey the redo-discard rule like any other step.
  std::vector<int> trace;
  step(editor.undo, trace, {1});
  CHECK(editor.undo.redo(editor).steps == 0);
}
