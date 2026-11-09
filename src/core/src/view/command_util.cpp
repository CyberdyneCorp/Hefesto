// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "view/command_util.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/view/diag_codes.hpp"

namespace hefesto::view::detail {

cmd::CommandResult fail(std::string_view code, std::string text) {
  return cmd::CommandResult::failure(diag::make(code, std::move(text)));
}

cmd::CommandResult failure(const diag::Diagnostic& d) {
  return cmd::CommandResult::failure(d);
}

diag::Outcome<editor::EditorState*> session(const cmd::CommandContext& ctx) {
  // Same contract as cmd::require_editor, without linking every area's commands.
  if (ctx.editor() == nullptr) {
    return diag::make(diag::codes::kCmdSession, "this command needs an editor session and none is attached");
  }
  return ctx.editor();
}

diag::Outcome<Target> pointed(const cmd::CommandContext& ctx, std::string_view no_window_text) {
  auto editor = session(ctx);
  if (!editor) {
    return editor.error();
  }
  LayoutWindow* window = editor.value()->windows.current();
  if (window == nullptr) {
    return diag::make(codes::kNoWindow, std::string(no_window_text));
  }
  return Target{*editor.value(), *window};
}

std::optional<double> parse_number(std::string_view text, bool prefix_ok) {
  const std::string copy(text);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(value) || (!prefix_ok && *end != '\0')) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::int64_t> parse_int(std::string_view text) {
  std::string_view digits = text;
  if (!digits.empty() && (digits.front() == '-' || digits.front() == '+')) {
    digits.remove_prefix(1);
  }
  if (digits.empty() || digits.find_first_not_of("0123456789") != std::string_view::npos) {
    return std::nullopt;
  }
  return std::strtoll(std::string(text).c_str(), nullptr, 10);
}

std::optional<db::Coord> parse_coord(std::string_view text, const tech::GridScale& grid) {
  cmd::CoordinateUnits units;  // shared units (cmd::parse_coordinate), default lambda
  units.internal_per = grid.internal;
  units.lambda_per = grid.lambda;
  const auto value = cmd::parse_coordinate(text, units);
  if (!value.ok()) {
    return std::nullopt;
  }
  return static_cast<db::Coord>(std::round(value.value()));
}

std::string format_coord(db::Coord value, const tech::GridScale& grid) {
  const std::int64_t num = std::int64_t{value} * grid.lambda;
  if (num % grid.internal == 0) {
    return std::to_string(num / grid.internal);
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(num) / static_cast<double>(grid.internal));
  return buf;
}

std::string format_rect(const db::Rect& r, const tech::GridScale& grid) {
  return "(" + format_coord(r.x0, grid) + ", " + format_coord(r.y0, grid) + ") to (" + format_coord(r.x1, grid) + ", " +
         format_coord(r.y1, grid) + ")";
}

int lookup(std::string_view word, std::initializer_list<std::string_view> choices) {
  int found = -2;
  int index = 0;
  for (const std::string_view choice : choices) {
    if (choice == word) {
      return index;
    }
    if (!word.empty() && choice.starts_with(word)) {
      found = found == -2 ? index : -1;
    }
    ++index;
  }
  return found;
}

std::optional<db::Rect> box_in_window(const editor::EditorState& editor, const LayoutWindow& window) {
  if (!editor.box || window.root() == nullptr) {
    return std::nullopt;
  }
  const db::CellDef* box_root = editor.box_root;
  if (box_root == nullptr) {
    box_root = editor.edit_cell != nullptr ? editor.edit_cell : window.root();
  }
  if (box_root != window.root()) {
    return std::nullopt;
  }
  return editor.box;
}

namespace {

bool contains_def(const db::CellDef& root, const db::CellDef& wanted, std::unordered_set<const db::CellDef*>& seen) {
  if (&root == &wanted) {
    return true;
  }
  if (!seen.insert(&root).second) {
    return false;
  }
  for (const auto& use : root.uses()) {
    if (use && use->def != nullptr && contains_def(*use->def, wanted, seen)) {
      return true;
    }
  }
  return false;
}

}  // namespace

void refresh_captions(editor::EditorState& editor) {
  const db::CellDef* edit = editor.edit_cell;
  editor.windows.for_each([&](LayoutWindow& w) {
    if (w.root() == nullptr) {
      return;
    }
    std::unordered_set<const db::CellDef*> seen;
    const bool editing = edit != nullptr && contains_def(*w.root(), *edit, seen);
    std::string caption = window_caption(w.root()->name(), editing ? edit->name() : "", editing);
    if (caption != w.caption()) {
      w.set_caption(std::move(caption));
      editor.windows.notify(w.id(), Change::caption);
    }
  });
}

bool add(cmd::CommandRegistry& registry, std::string name, std::string synopsis, std::string help, cmd::Handler handler) {
  return !registry.add(cmd::CommandSpec{std::move(name), std::move(synopsis), std::move(help), std::move(handler)});
}

}  // namespace hefesto::view::detail
