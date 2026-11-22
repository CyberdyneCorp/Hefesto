// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `wind3d` commands that read layers and the layout box (view-3d "3D
// view commands"; design D9): `render` (with Q-MISC-73's "No CIF output
// style set!"), `see` on the current mode's visibility set (Q-MISC-71),
// `cutbox`, and the wrapper's `magic::render3d` (tcltk/wrapper.tcl:
// 1589-1614). Ported from Magic 8.3.694 (fc5daa9e) graphics/W3Dmain.c
// (w3dRenderValues, w3dSeeLayers, w3dCutBox) and cif/CIFmain.c
// (CIFNameToMask), Copyright (C) 1985, 1990 Regents of the University of
// California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: WIND3D (design D3).

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "db/edit_util.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/layer_list.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "view3d/client/internal.hpp"

namespace hefesto::view3d {

namespace detail {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// The indexes of the mask layers named `name` (exact, case-sensitive).
std::vector<std::size_t> layers_named(const mask::OutputStyle& style, std::string_view name) {
  std::vector<std::size_t> found;
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    if (style.layers[i].name == name) {
      found.push_back(i);
    }
  }
  return found;
}

// The `render` command's values start over when the output style changes
// (Magic re-reads the section; RenderOverride).
std::map<std::size_t, RenderOverride>& overrides(editor::EditorState& editor) {
  Session& session = editor.view3d;
  const OutputStyleKey key = current_key(editor);
  if (!(session.render_key == key)) {
    session.render_overrides.clear();
    session.render_key = key;
  }
  return session.render_overrides;
}

CommandResult render_query(CommandContext& ctx, editor::EditorState& editor, std::size_t index) {
  const LayerStack stack = mask_stack(editor, true, ctx.sink());
  const StackLayer& layer = stack.layers()[index];
  return CommandResult::success(cmd::Value::List{static_cast<double>(layer.height),
                                                 static_cast<double>(layer.thickness),
                                                 std::int64_t{render_style_position(layer.style)}});
}

// `render name height thick [style]`: every layer of that name; a style
// is taken only when it is a non-negative integer, as Magic stores it.
CommandResult render_set(Target t, const mask::OutputStyle& style, const std::vector<std::string>& args) {
  std::optional<int> position;
  if (args.size() == 4) {
    position = integer(args[3]);
  }
  const auto height = numeric(args[1]);
  const auto thick = numeric(args[2]);
  if (!height || !thick) {
    return usage("Usage: render name [height thick [style]]");
  }
  std::map<std::size_t, RenderOverride>& values = overrides(t.editor);
  for (const std::size_t index : layers_named(style, args[0])) {
    RenderOverride& value = values[index];
    if (position && *position >= 0) {
      value.style_position = *position;
    }
    value.height = *height;
    value.thickness = *thick;
  }
  redraw(t.editor, t.window);
  return CommandResult::success();
}

// CIFNameToMask's failure: the name, then the style's layer names.
CommandResult unknown_mask_layer(const mask::OutputStyle& style, std::string_view name) {
  std::string text = "CIF name \"" + std::string{name} + "\" doesn't exist in style \"" + style.name +
                     "\".\nThe valid CIF layer names are: ";
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    text += (i == 0 ? "" : ", ") + style.layers[i].name;
  }
  return fail(kUnknownMaskLayer, text + ".");
}

CommandResult see_mask(Target t, std::string_view name, bool hide) {
  const mask::OutputStyle* style = current_style(t.editor);
  if (style == nullptr) {
    return fail(kNoStyle, std::string{kNoStyleText});
  }
  const std::vector<std::size_t> named = layers_named(*style, name);
  if (named.empty()) {
    return unknown_mask_layer(*style, name);
  }
  std::vector<bool>& hidden = t.window.visibility.mask_hidden;
  hidden.resize(std::max(hidden.size(), style->layers.size()), false);
  for (const std::size_t index : named) {
    hidden[index] = hide;
  }
  return CommandResult::success();
}

CommandResult see_layout(CommandContext& ctx, Target t, std::string_view name, bool hide) {
  const diag::Outcome<db::LayerList> list = db::parse_layer_list(name, *t.editor.technology);
  if (!list.ok()) {
    if (db::is_bad_layer_name(list.error())) {
      std::string names = tech::layer_names_text(*t.editor.technology);
      if (names.ends_with('\n')) {
        names.pop_back();
      }
      print(ctx, std::move(names));
    }
    return CommandResult::failure(list.error());
  }
  tech::TypeMask& hidden = t.window.visibility.layout_hidden;
  if (hide) {
    hidden |= list.value().types;
  } else {
    hidden &= ~list.value().types;
  }
  return CommandResult::success();
}

// `cutbox box`: the layout box, only when it is in the rendered cell.
void cut_to_box(const editor::EditorState& editor, WindowState& window) {
  const db::CellDef* root = db::edit::box_root(editor);
  if (root != nullptr && editor.box && root == editor.cells.find(window.cell)) {
    window.cut_box = *editor.box;
  }
}

CommandResult cutbox_query(CommandContext& ctx, const editor::EditorState& editor, const WindowState& window) {
  if (!window.cut_box) {
    return CommandResult::success(std::string{"none"});
  }
  const cmd::CoordinateUnits units = editor::coordinate_units(editor, magic_compat(ctx));
  const db::Rect& r = *window.cut_box;
  cmd::Value::List values;
  for (const db::Coord v : {r.x0, r.y0, r.x1, r.y1}) {
    values.emplace_back(cmd::format_value(v, editor.units, units));
  }
  return CommandResult::success(std::move(values));
}

// Four integers (StrIsInt each, atoi), or nullopt.
std::optional<db::Rect> four_integers(const std::vector<std::string>& args) {
  const auto x0 = integer(args[0]);
  const auto y0 = integer(args[1]);
  const auto x1 = integer(args[2]);
  const auto y1 = integer(args[3]);
  if (!x0 || !y0 || !x1 || !y1) {
    return std::nullopt;
  }
  return db::Rect{*x0, *y0, *x1, *y1};
}

void evaluate_quietly(editor::EditorState& editor, const std::string& script) {
  if (script::Interpreter* interp = editor.script.interp.get()) {
    static_cast<void>(interp->evaluate(script));
  }
}

}  // namespace

// w3dRenderValues.
CommandResult render_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  const mask::OutputStyle* style = current_style(t.value().editor);
  std::size_t index = 0;
  if (!inv.args.empty()) {
    if (style == nullptr) {
      return fail(kNoStyle, std::string{kNoStyleText});  // Q-MISC-73: Magic dereferences the missing style
    }
    const std::vector<std::size_t> named = layers_named(*style, inv.args[0]);
    if (named.empty()) {
      return fail(kUnknownMaskLayer, "Unknown CIF layer \"" + inv.args[0] + "\"");
    }
    index = named.front();
  }
  if (inv.args.size() == 1) {
    return render_query(ctx, t.value().editor, index);
  }
  if (inv.args.size() == 3 || inv.args.size() == 4) {
    return render_set(t.value(), *style, inv.args);
  }
  return usage("Usage: render name [height thick [style]]");
}

// w3dSeeLayers on the current mode's set (Q-MISC-71).
CommandResult see_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  if (inv.args.size() != 1 && inv.args.size() != 2) {
    return usage("Usage: see [no] layer");
  }
  const bool hide = inv.args.size() == 2 && inv.args[0] == "no";
  const std::string& name = inv.args.back();
  CommandResult result = t.value().window.mode == Mode::mask_layer ? see_mask(t.value(), name, hide)
                                                                   : see_layout(ctx, t.value(), name, hide);
  if (result.ok()) {
    redraw(t.value().editor, t.value().window);
  }
  return result;
}

// w3dCutBox. As in Magic, `cutbox WORD` redraws whatever WORD is, and four
// words that are not all integers change nothing.
CommandResult cutbox_cmd(CommandContext& ctx, const Invocation& inv) {
  auto t = target(ctx);
  if (!t.ok()) {
    return CommandResult::failure(t.error());
  }
  WindowState& window = t.value().window;
  if (inv.args.empty()) {
    return cutbox_query(ctx, t.value().editor, window);
  }
  if (inv.args.size() == 1) {
    if (inv.args[0] == "none") {
      window.cut_box.reset();
    }
    if (inv.args[0] == "box") {
      cut_to_box(t.value().editor, window);
    }
    redraw(t.value().editor, window);
    return CommandResult::success();
  }
  if (inv.args.size() == 4) {
    if (const auto r = four_integers(inv.args)) {
      window.cut_box = *r;
      redraw(t.value().editor, window);
    }
    return CommandResult::success();
  }
  return usage("Usage: cutbox [none|box|llx lly urx ur]");
}

// magic::render3d (wrapper.tcl): the window at `.render.magic` in its own
// top-level window `.render`, `cutbox box` once, Opts(cutbox) set; the
// window manager's close sets Opts(wind3d) to 0 before closing the window.
// The widget commands run only where the front end has a widget model.
CommandResult render3d_cmd(CommandContext& ctx, const Invocation& inv) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr || inv.args.size() > 1) {
    return usage("Usage: render3d [cell]");
  }
  evaluate_quietly(*editor, "if {[llength [info commands toplevel]]} {catch {toplevel .render}; catch {tkwait "
                            "visibility .render}}");
  OpenRequest request;
  request.cell = inv.args.empty() ? std::string{} : inv.args[0];
  request.toolkit_path = kRender3dToolkitPath;
  CommandResult opened = open_wind3d(ctx, request);
  if (!opened.ok() || !editor->view3d.window) {
    return opened;
  }
  cut_to_box(*editor, *editor->view3d.window);
  redraw(*editor, *editor->view3d.window);
  evaluate_quietly(*editor, "set ::Opts(cutbox) 1");
  evaluate_quietly(*editor, "if {[llength [info commands wm]]} {catch {wm protocol .render WM_DELETE_WINDOW "
                            "{set Opts(wind3d) 0 ; .render.magic closewindow ; destroy .render}}}");
  return CommandResult::success();
}

}  // namespace detail

}  // namespace hefesto::view3d
