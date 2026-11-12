// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// pushbox, popbox, peekbox, pushstack, popstack, clearstack, suspendall and
// resumeall (ui-box-stack-and-measure "Box stack push", "Box stack pop and
// peek", "Edit stack push", "Edit stack pop and clear", "Nested redraw
// suspension"; Q-SCRIPT-15). Native versions of the tools.tcl procs, with
// their names, arguments, results and errors. With a Tcl interpreter the
// stacks are the globals `boxstack` and `editstack`, as in tools.tcl, so a
// script reads them with `set boxstack`. Owner: DISPATCH.
// (Magic ref: tcltk/tools.tcl:8-50, 122-265)

#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/ui/diag_codes.hpp"
#include "hefesto/ui/state.hpp"
#include "ui/command_util.hpp"

namespace hefesto::ui::detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

CommandResult stack_error(std::string text) {
  return raise(codes::kStack, std::move(text));
}

std::string joined(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& w : words) {
    out += (out.empty() ? "" : " ") + w;
  }
  return out;
}

// The words of a stack record (a Tcl list; braces around it are dropped).
std::vector<std::string> record_words(std::string_view record) {
  std::string text{record};
  if (text.size() >= 2 && text.front() == '{' && text.back() == '}') {
    text = text.substr(1, text.size() - 2);
  }
  return value_words(cmd::Value{text});
}

std::string difference(const std::string& hi, const std::string& lo) {
  const auto a = parse_int(hi);
  const auto b = parse_int(lo);
  return a && b ? std::to_string(*a - *b) : hi + "-" + lo;
}

// --- the Tcl globals ---------------------------------------------------------------

constexpr std::string_view kBoxStackVar = "::boxstack";
constexpr std::string_view kEditStackVar = "::editstack";

script::Interpreter* interpreter(editor::EditorState& editor) {
  return editor.script.interp.get();
}

// The elements of global `name`, or nullopt when it does not exist (or is
// not a list).
std::optional<std::vector<std::string>> read_list(editor::EditorState& editor, std::string_view name) {
  const auto value = interpreter(editor)->get_var(name);
  if (!value) {
    return std::nullopt;
  }
  auto words = pcell::split_tcl_list(value->to_string());
  return words.ok() ? std::optional{std::move(words).value()} : std::nullopt;
}

void write_list(editor::EditorState& editor, std::string_view name, const std::vector<std::string>& elements) {
  static_cast<void>(interpreter(editor)->set_var(name, cmd::Value{pcell::merge_tcl_list(elements)}));
}

// Loads the stacks from the Tcl globals for one command (a script may have
// set them) and writes them back when it ends. A global is written only
// when the procedure would set it: `boxstack` once pushed, `editstack` once
// pushed or when it existed (or `clearstack`).
class StackVariables {
 public:
  explicit StackVariables(editor::EditorState& editor) : editor_(editor) {
    if (interpreter(editor_) == nullptr) {
      return;
    }
    auto boxes = read_list(editor_, kBoxStackVar);
    editor_.box_stack.created = boxes.has_value();
    editor_.box_stack.records = boxes.value_or(std::vector<std::string>{});
    auto edits = read_list(editor_, kEditStackVar);
    edit_exists_ = edits.has_value();
    editor_.edit_stack = edits.value_or(std::vector<std::string>{});
  }
  ~StackVariables() {
    if (interpreter(editor_) == nullptr) {
      return;
    }
    if (editor_.box_stack.created) {
      write_list(editor_, kBoxStackVar, editor_.box_stack.records);
    }
    if (edit_exists_ || !editor_.edit_stack.empty()) {
      write_list(editor_, kEditStackVar, editor_.edit_stack);
    }
  }
  StackVariables(const StackVariables&) = delete;
  StackVariables& operator=(const StackVariables&) = delete;

  void create_edit_stack() { edit_exists_ = true; }

 private:
  editor::EditorState& editor_;
  bool edit_exists_ = false;
};

// --- box stack -------------------------------------------------------------------

CommandResult push_box(CommandContext& ctx, editor::EditorState& editor, const std::string& values) {
  const InternalUnits internal{editor};
  editor.box_stack.created = true;
  if (!values.empty()) {
    editor.box_stack.records.push_back(values);
    return CommandResult::success();
  }
  const CommandResult box = run_forwarded(ctx, {"box", "values"});
  if (!box.ok() && box.raises) {
    return box;
  }
  // `lappend boxstack [box values]`: without a box Magic prints "Box tool
  // must be present" and appends an empty record.
  editor.box_stack.records.push_back(box.ok() ? joined(value_words(box.value)) : std::string{});
  return CommandResult::success();
}

// The command popbox applies for `type`, or nothing for another type.
std::vector<std::string> apply_command(const std::string& type, const std::vector<std::string>& b) {
  const auto at = [&](std::size_t i) { return i < b.size() ? b[i] : std::string{}; };
  if (type == "values") {
    return {"box", "values", at(0), at(1), at(2), at(3)};
  }
  if (type == "size") {
    return {"box", "size", difference(at(2), at(0)), difference(at(3), at(1))};
  }
  if (type == "position") {
    return {"box", "position", at(0), at(1)};
  }
  return {};
}

CommandResult pop_box(CommandContext& ctx, editor::EditorState& editor, const std::string& type) {
  const InternalUnits internal{editor};
  if (!editor.box_stack.created) {
    return stack_error("No stack");
  }
  if (editor.box_stack.records.empty()) {
    return stack_error("Empty stack");
  }
  const std::string record = editor.box_stack.records.back();
  const std::vector<std::string> command = apply_command(type, record_words(record));
  if (!command.empty()) {
    const CommandResult applied = run_forwarded(ctx, command);
    if (!applied.ok() && applied.raises) {
      return applied;
    }
  }
  editor.box_stack.records.pop_back();
  return CommandResult::success(record);
}

CommandResult pushbox(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"pushbox ?values?\"");
  }
  const StackVariables globals{*editor.value()};
  return push_box(ctx, *editor.value(), inv.args.empty() ? std::string{} : inv.args[0]);
}

CommandResult popbox(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"popbox ?type?\"");
  }
  const StackVariables globals{*editor.value()};
  return pop_box(ctx, *editor.value(), inv.args.empty() ? std::string{"values"} : inv.args[0]);
}

CommandResult peekbox(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"peekbox ?type?\"");
  }
  const StackVariables globals{*editor.value()};
  CommandResult popped = pop_box(ctx, *editor.value(), inv.args.empty() ? std::string{"values"} : inv.args[0]);
  if (!popped.ok()) {
    return stack_error("No stack");
  }
  const std::string record = popped.value.to_string();
  editor.value()->box_stack.records.push_back(record);
  return CommandResult::success(record);
}

// --- redraw suspension -------------------------------------------------------------

// No windowing toolkit (a headless session): both commands are no-ops.
bool has_toolkit(const editor::EditorState& editor) {
  return editor.styles.has_value() && editor.windows.size() > 0;
}

void suspend_all(editor::EditorState& editor) {
  if (has_toolkit(editor) && ++editor.suspend_depth == 1) {
    editor.windows.set_display_suspended(true);
  }
}

CommandResult resume_all(editor::EditorState& editor) {
  if (!has_toolkit(editor)) {
    return CommandResult::success();
  }
  if (editor.suspend_depth <= 0) {
    return raise(codes::kSuspend, "resume called without suspend");
  }
  if (--editor.suspend_depth <= 0) {
    editor.suspend_depth = 0;
    editor.windows.set_display_suspended(false);
    editor.windows.redisplay_all();
  }
  return CommandResult::success();
}

CommandResult suspendall(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  suspend_all(*editor.value());
  return CommandResult::success();
}

CommandResult resumeall(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  return resume_all(*editor.value());
}

// --- edit stack --------------------------------------------------------------------

// pushstack step 1: select the cell under the box when nothing is selected,
// then take the selected cell's name.
std::string cell_to_push(CommandContext& ctx, editor::EditorState& editor) {
  const CommandResult selected = run_nested(ctx, {"what", "-list"});
  const std::vector<std::string> what = selected.ok() ? value_words(selected.value) : std::vector<std::string>{};
  if (what.size() < 3 || what[2].empty() || what[2] == "{}") {
    (void)push_box(ctx, editor, {});
    (void)run_forwarded(ctx, {"select", "cell"});
    (void)pop_box(ctx, editor, "values");
  }
  const CommandResult self = run_nested(ctx, {"cellname", "list", "self"});
  return self.ok() ? self.value.to_string() : std::string{};
}

CommandResult pushstack(CommandContext& ctx, const Invocation& inv) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  if (inv.args.size() > 1) {
    return raise(codes::kUsage, "wrong # args: should be \"pushstack ?name?\"");
  }
  editor::EditorState& ed = *editor.value();
  const StackVariables globals{ed};
  std::string name = inv.args.empty() ? std::string{} : inv.args[0];
  if (name.empty()) {
    name = cell_to_push(ctx, ed);
  }
  if (name.empty()) {
    return stack_error("No cell to push!");
  }
  if (value_words(cmd::Value{name}).size() > 1) {
    return stack_error("Too many cells selected!");
  }
  CommandResult view = CommandResult::success();
  {
    const InternalUnits internal{ed};
    view = run_nested(ctx, {"view", "get"});
  }
  const CommandResult root = run_nested(ctx, {"cellname", "list", "window"});
  ed.edit_stack.push_back(view.ok() ? joined(value_words(view.value)) : std::string{});
  ed.edit_stack.push_back(root.ok() ? root.value.to_string() : std::string{});
  const CommandResult loaded = run_forwarded(ctx, {"load", name});
  return loaded.ok() ? CommandResult::success() : loaded;
}

CommandResult popstack(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  editor::EditorState& ed = *editor.value();
  const StackVariables globals{ed};
  if (ed.edit_stack.size() < 2) {
    return stack_error("No subcell stack!");
  }
  suspend_all(ed);
  CommandResult result = run_forwarded(ctx, {"load", ed.edit_stack.back()});
  if (result.ok()) {
    const InternalUnits internal{ed};
    std::vector<std::string> view{"view"};
    for (std::string& w : record_words(ed.edit_stack[ed.edit_stack.size() - 2])) {
      view.push_back(std::move(w));
    }
    result = run_forwarded(ctx, view);
  }
  (void)resume_all(ed);
  if (!result.ok()) {
    return result;
  }
  ed.edit_stack.resize(ed.edit_stack.size() - 2);
  return CommandResult::success();
}

CommandResult clearstack(CommandContext& ctx, const Invocation& /*inv*/) {
  auto editor = session(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  StackVariables globals{*editor.value()};
  globals.create_edit_stack();
  editor.value()->edit_stack.clear();
  return CommandResult::success();
}

}  // namespace

bool register_stack_commands(cmd::CommandRegistry& registry) {
  bool ok = add(registry, "pushbox", "pushbox ?values?",
                "Pushes the box (internal units) or the given values onto the box stack; the box is unchanged.",
                pushbox);
  ok = add(registry, "popbox", "popbox ?values|size|position?",
           "Pops the box stack and applies the record to the box (all values, size or position); returns it.",
           popbox) &&
       ok;
  ok = add(registry, "peekbox", "peekbox ?values|size|position?",
           "Applies the top of the box stack like popbox and keeps it on the stack; returns it.", peekbox) &&
       ok;
  ok = add(registry, "pushstack", "pushstack ?cell?",
           "Loads a cell (the selected cell, or the cell under the box) and remembers the view and root to return "
           "to.",
           pushstack) &&
       ok;
  ok = add(registry, "popstack", "popstack", "Returns to the cell and view saved by the last pushstack.", popstack) &&
       ok;
  ok = add(registry, "clearstack", "clearstack", "Empties the edit stack.", clearstack) && ok;
  ok = add(registry, "suspendall", "suspendall",
           "Suspends display updates in every layout window; calls nest. No effect without a window system.",
           suspendall) &&
       ok;
  ok = add(registry, "resumeall", "resumeall",
           "Undoes one suspendall; the display resumes when every suspendall is matched.", resumeall) &&
       ok;
  return ok;
}

}  // namespace hefesto::ui::detail
