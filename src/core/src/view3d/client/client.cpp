// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` client (openspec/changes/m10-3d-view/design.md D9): opening
// and closing the 3D window, the output-style bookkeeping of Q-MISC-71/78
// and the helpers the command handlers share. Ported from Magic 8.3.694
// (fc5daa9e) graphics/W3Dmain.c (W3Dcreate, W3DloadWindow, W3Ddelete,
// Set3DDefaults), Copyright (C) 1985, 1990 Regents of the University of
// California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: WIND3D (design D3).

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <string>
#include <utility>

#include "view3d/client/internal.hpp"
#include "db/edit_util.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/ui/bindings.hpp"

namespace hefesto::view3d {

namespace detail {

cmd::CommandResult fail(std::string_view code, std::string text) {
  return cmd::CommandResult::failure(diag::make(code, std::move(text)));
}

cmd::CommandResult usage(std::string text) {
  return fail(kUsage, std::move(text));
}

void print(cmd::CommandContext& ctx, std::string text) {
  ctx.emit(diag::make(kReport, std::move(text)));
}

std::optional<float> numeric(std::string_view word) {
  const std::string text{word};
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) {
    return std::nullopt;
  }
  return static_cast<float>(value);
}

std::optional<int> integer(std::string_view word) {
  std::string_view digits = word;
  if (!digits.empty() && (digits.front() == '-' || digits.front() == '+')) {
    digits.remove_prefix(1);
  }
  if (!std::all_of(digits.begin(), digits.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
    return std::nullopt;
  }
  const std::string text{word};
  errno = 0;
  const long long value = std::strtoll(text.c_str(), nullptr, 10);
  return static_cast<int>(std::clamp<long long>(value, INT_MIN, INT_MAX));
}

bool starts3(std::string_view word, std::string_view prefix) {
  return word.substr(0, 3) == prefix.substr(0, 3);
}

bool magic_compat(cmd::CommandContext& ctx) {
  return ctx.project().compat.magic_compat;
}

diag::Outcome<Target> target(cmd::CommandContext& ctx) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr || !editor->view3d.window) {
    return diag::make(kNoWindow, "No 3D window is open.");
  }
  sync_output_style(*editor, magic_compat(ctx));
  if (!editor->view3d.window) {
    return diag::make(kNoWindow, "No 3D window is open.");
  }
  return Target{*editor, *editor->view3d.window};
}

const mask::OutputStyle* current_style(const editor::EditorState& editor) {
  return editor.mask.output_styles.current_style();
}

OutputStyleKey current_key(const editor::EditorState& editor) {
  const mask::OutputStyleSet& set = editor.mask.output_styles;
  if (!set.current || *set.current >= set.styles.size()) {
    return {};
  }
  const std::size_t index = *set.current;
  return OutputStyleKey{index < set.base.size() ? set.base[index] : set.styles[index], index};
}

db::Rect cell_bbox(const editor::EditorState& editor, const WindowState& window) {
  const db::CellDef* def = editor.cells.find(window.cell);
  return def != nullptr ? def->bbox() : db::Rect{};
}

void set_defaults(const editor::EditorState& editor, WindowState& window, bool magic_compat) {
  const float f = magic_compat ? current_factor(editor) : window.applied_factor;
  window.view = default_view(cell_bbox(editor, window), window.mode, f);
  window.visibility = VisibilitySets{};
  window.cut_box.reset();
}

void redraw(const editor::EditorState& editor, const WindowState& window) {
  if (editor.view3d.display != nullptr) {
    editor.view3d.display->redraw(editor, window);
  }
}

}  // namespace detail

namespace {

using detail::kCannotOpen;

// W3Dcreate's choice of cell: the one named, else the cell of the box's
// window (ToolGetBox), else the root of the only layout window.
diag::Outcome<std::string> cell_name(const editor::EditorState& editor, const std::string& named) {
  if (!named.empty()) {
    return named;
  }
  if (const db::CellDef* root = db::edit::box_root(editor)) {
    return std::string{root->name()};
  }
  const view::LayoutWindow* only = editor.windows.size() == 1 ? editor.windows.current() : nullptr;
  if (only != nullptr && only->root() != nullptr) {
    return std::string{only->root()->name()};
  }
  return diag::make(kCannotOpen, "Ambiguous directive:  Put cursor box in one of the windows.");
}

// W3DloadWindow: an existing definition (DBCellLookDef), read from disk if
// it is not loaded yet (DBCellRead); the 3D window never creates a cell.
diag::Outcome<db::CellDef*> rendered_cell(cmd::CommandContext& ctx, editor::EditorState& editor,
                                          const std::string& named) {
  diag::Outcome<std::string> name = cell_name(editor, named);
  if (!name.ok()) {
    return name.error();
  }
  db::CellDef* def = editor.cells.find(name.value());
  if (def != nullptr && !def->flags().available && editor.windows.cell_reader() != nullptr) {
    static_cast<void>(editor.windows.cell_reader()(ctx, *def));
  }
  if (def == nullptr || !def->flags().available) {
    return diag::make(kCannotOpen, "Cells cannot be created in the 3D window.");
  }
  return def;
}

bool top_level(std::string_view path) {
  return path.size() > 1 && path.front() == '.' && path.find('.', 1) == std::string_view::npos;
}

// MakeWindowCommand: the toolkit path becomes a window command, through the
// front end's hook (hefesto-cli's wrapper mode defines it; the desktop
// routes the 3D view's commands by focus). A closed window's command is
// still defined and is reused.
void announce_path(editor::EditorState& editor, const std::string& path) {
  script::Interpreter* interp = editor.script.interp.get();
  if (interp == nullptr || interp->has_command(path)) {
    return;
  }
  static_cast<void>(interp->evaluate(
      "catch {rename {::hefesto::wind3d_closed::" + path + "} {}}; "
      "if {[llength [info commands ::hefesto::tk::window_command]]} {::hefesto::tk::window_command {" + path + "}}"));
}

// GrTOGLDelete: the closed window's toolkit path stops being a command
// (`info commands` no longer lists it; calling it is "invalid command name").
// The command is moved aside, not deleted: `<path> closewindow` is still
// running it, and deleting a running command would free its binding.
constexpr std::string_view kRetiredNamespace = "::hefesto::wind3d_closed";

std::string retired_name(const std::string& path) { return std::string{kRetiredNamespace} + "::" + path; }

void retire_path(editor::EditorState& editor, const std::string& path) {
  script::Interpreter* interp = editor.script.interp.get();
  if (interp == nullptr || !interp->has_command(path)) {
    return;
  }
  static_cast<void>(interp->evaluate("namespace eval " + std::string{kRetiredNamespace} + " {}; catch {rename {" +
                                     retired_name(path) + "} {}}; catch {rename {" + path + "} {" +
                                     retired_name(path) + "}}"));
}

}  // namespace

float WindowState::exit_mask_factor(bool magic_compat, float current_factor) const {
  return view3d::exit_mask_factor(magic_compat, applied_factor, current_factor);
}

float current_factor(const editor::EditorState& editor) {
  const mask::OutputStyle* style = detail::current_style(editor);
  return style != nullptr ? static_cast<float>(style->scale.scale_factor) : 1.0F;
}

void sync_output_style(editor::EditorState& editor, bool magic_compat) {
  const OutputStyleKey now = detail::current_key(editor);
  Session& session = editor.view3d;
  if (!(session.render_key == now)) {
    session.render_overrides.clear();  // Magic re-reads a style when it becomes current
    session.render_key = now;
  }
  if (!session.window) {
    return;
  }
  WindowState& window = *session.window;
  if (now == window.style_key) {
    return;
  }
  window.visibility.mask_hidden.clear();
  if (window.mode == Mode::mask_layer && now.base == nullptr) {
    rescale(window.view, inverse_factor(window.applied_factor));
    window.mode = Mode::layout_layer;
    window.applied_factor = 1.0F;
  } else if (window.mode == Mode::mask_layer) {
    window.applied_factor = change_output_style(window.view, magic_compat, window.applied_factor, current_factor(editor));
  }
  window.style_key = now;
}

cmd::CommandResult open_wind3d(cmd::CommandContext& ctx, const OpenRequest& request) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr) {
    return detail::fail(detail::kNoWindow, "No editor session is attached.");
  }
  Session& session = editor->view3d;
  if (session.window) {
    return detail::fail(detail::kOneWindow, "Only one 3D window allowed.");
  }
  diag::Outcome<db::CellDef*> def = rendered_cell(ctx, *editor, request.cell);
  if (!def.ok()) {
    return cmd::CommandResult::failure(def.error());
  }
  const bool compat = detail::magic_compat(ctx);
  WindowState window;
  window.cell = std::string{def.value()->name()};
  window.toolkit_path = request.toolkit_path;
  window.style_key = detail::current_key(*editor);
  // Q-MISC-73 amendment: mask-layer mode only with a current output style.
  window.mode = detail::current_style(*editor) != nullptr ? Mode::mask_layer : Mode::layout_layer;
  window.applied_factor = window.mode == Mode::mask_layer ? current_factor(*editor) : 1.0F;
  detail::set_defaults(*editor, window, compat);
  session.closed_paths.erase(window.toolkit_path);
  session.window = std::move(window);
  announce_path(*editor, request.toolkit_path);
  ui::install_wind3d_bindings(*editor);
  detail::redraw(*editor, *session.window);
  if (compat && !top_level(request.toolkit_path)) {
    return cmd::CommandResult::success("window \"" + request.toolkit_path + "\" isn't a top-level window");
  }
  return cmd::CommandResult::success(std::string{});
}

bool close_wind3d(editor::EditorState& editor) {
  Session& session = editor.view3d;
  if (!session.window) {
    return false;
  }
  const WindowState closed = std::move(*session.window);
  session.window.reset();
  session.closed_paths.insert(closed.toolkit_path);
  retire_path(editor, closed.toolkit_path);
  if (session.display != nullptr) {
    session.display->closed(closed);
  }
  return true;
}

std::vector<diag::Diagnostic> unknown_command_messages(const std::vector<std::string>& words) {
  diag::Diagnostic unknown = diag::make(detail::kUnknownCommand, "Unknown command:");
  unknown.no_newline = true;
  std::string echo;
  for (const std::string& word : words) {
    echo += " '" + word + "'";
  }
  echo += " at (20, 20)";
  return {std::move(unknown), diag::make(detail::kReport, std::move(echo))};
}

bool is_open_path(const editor::EditorState& editor, std::string_view path) {
  return editor.view3d.window && editor.view3d.window->toolkit_path == path;
}

bool is_closed_path(const editor::EditorState& editor, std::string_view path) {
  return !is_open_path(editor, path) && editor.view3d.closed_paths.contains(std::string{path});
}

void note_window_command(editor::EditorState& editor, bool to_3d) { editor.view3d.script_target_3d = to_3d; }

bool routes_script_commands(const editor::EditorState& editor, bool magic_compat) {
  return magic_compat && editor.view3d.script_target_3d && editor.view3d.window.has_value();
}

bool is_retired_command(std::string_view name) {
  constexpr std::string_view kGlobal = "::";
  const std::string_view retired = kRetiredNamespace.substr(kGlobal.size());  // "hefesto::wind3d_closed"
  if (name.starts_with(kGlobal)) {
    name.remove_prefix(kGlobal.size());
  }
  return name.size() > retired.size() + kGlobal.size() && name.starts_with(retired) &&
         name.substr(retired.size()).starts_with(kGlobal);
}

}  // namespace hefesto::view3d
