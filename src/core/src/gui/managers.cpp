// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell, library and technology managers, the text helper and the
// manager script tags (see managers.hpp).
// (Magic ref: tcltk/cellmgr.tcl:13-272, libmgr.tcl:13-249,
// wrapper.tcl:286-531,567-583, texthelper.tcl:140-338, drcmgr.tcl:13-14)

#include "hefesto/gui/managers.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "gui/gui_util.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"
#include "ui/command_util.hpp"

namespace hefesto::gui {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using Commands = std::vector<std::vector<std::string>>;
using detail::run;

// `cellname list ...` lists: a list value, or 0 / empty for none.
std::vector<std::string> cell_list(CommandContext& ctx, const std::vector<std::string>& words) {
  std::vector<std::string> out = detail::words_of(detail::run_value(ctx, words));
  if (out.size() == 1 && out[0] == "0") {
    out.clear();
  }
  return out;
}

std::string first_instance(CommandContext& ctx, const std::string& cell) {
  const std::vector<std::string> uses = cell_list(ctx, {"cellname", "list", "instances", cell});
  return uses.empty() ? std::string{} : uses.front();
}

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::string first_line(std::string_view text, std::size_t& pos) {
  const std::size_t end = text.find('\n', pos);
  std::string_view line = text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
  pos = end == std::string_view::npos ? text.size() : end + 1;
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return std::string{line};
}

void add_directory_rows(CommandContext& ctx, LibraryManagerModel& model, const std::string& dir,
                        const std::string& tech, bool filter) {
  model.directories.push_back(dir + "/");
  auto entries = ctx.services().files.list(detail::session_path(ctx, dir));
  if (!entries.ok()) {
    return;
  }
  std::vector<platform::Path> files = entries.value();
  std::sort(files.begin(), files.end());
  for (const platform::Path& path : files) {
    if (path.extension() != ".mag") {
      continue;
    }
    auto text = ctx.services().files.read(path);  // each read closes its file (Q-SCRIPT-27)
    const auto file_tech = text.ok() ? mag_file_technology(text.value()) : std::nullopt;
    if (file_tech && (!filter || *file_tech == tech)) {
      model.rows.push_back(LibraryManagerRow{dir + "/", path.stem().string(), *file_tech});
    }
  }
}

std::optional<std::string> pdk_root(CommandContext& ctx) {
  editor::EditorState* editor = ctx.editor();
  if (editor == nullptr) {
    return std::nullopt;
  }
  if (const script::Interpreter* interp = editor->script.interp.get()) {
    if (auto value = interp->get_var("PDK_ROOT")) {
      return value->to_string();
    }
  }
  return editor->path_variables.get("PDK_ROOT");
}

void add_tech_files(CommandContext& ctx, TechManagerModel& model) {
  for (const std::string& dir : detail::words_of(detail::run_value(ctx, {"path", "sys"}))) {
    auto entries = ctx.services().files.list(detail::session_path(ctx, dir));
    if (!entries.ok()) {
      continue;
    }
    for (const platform::Path& path : entries.value()) {
      const std::string name = path.filename().string();
      if (ends_with(name, ".tech") || ends_with(name, ".tech27")) {
        const std::string tech = path.stem().string();
        model.tech_menu.push_back(TechMenuEntry{tech, "tech load " + tech});
      }
    }
  }
}

void add_pdk_entries(CommandContext& ctx, TechManagerModel& model) {
  const auto root = pdk_root(ctx);
  if (!root || root->empty()) {
    return;
  }
  auto entries = ctx.services().files.list(platform::Path{*root});
  if (!entries.ok()) {
    return;
  }
  for (const platform::Path& path : entries.value()) {
    const std::string dir = path.filename().string();
    model.tech_menu.push_back(
        TechMenuEntry{dir, "source " + *root + "/" + dir + "/libs.tech/magic/" + dir + ".magicrc"});
  }
}

// `[cmd ...]` without forwarding a failure (a command that is not available
// yet, such as `cif` before BINDING or `extract` before M7): its words.
std::vector<std::string> quiet_words(CommandContext& ctx, const std::vector<std::string>& words) {
  const CommandResult r = ui::detail::run_nested(ctx, words);
  return r.ok() ? detail::words_of(r.value) : std::vector<std::string>{};
}

std::string quiet_text(CommandContext& ctx, const std::vector<std::string>& words) {
  const CommandResult r = ui::detail::run_nested(ctx, words);
  return r.ok() ? r.value.to_string() : std::string{};
}

void fill_styles(CommandContext& ctx, TechManagerModel& model) {
  model.cif_input_styles = quiet_words(ctx, {"cif", "listall", "istyle"});
  model.cif_output_styles = quiet_words(ctx, {"cif", "listall", "ostyle"});
  model.extract_styles = quiet_words(ctx, {"extract", "listall", "style"});
  model.drc_styles = quiet_words(ctx, {"drc", "listall", "style"});
  model.cif_input_style = quiet_text(ctx, {"cif", "list", "istyle"});
  model.cif_output_style = quiet_text(ctx, {"cif", "list", "ostyle"});
  model.extract_style = quiet_text(ctx, {"extract", "list", "style"});
  model.drc_style = quiet_text(ctx, {"drc", "list", "style"});
}

std::string format_g(double value) {
  char text[64];
  std::snprintf(text, sizeof text, "%g", value);
  return text;
}

void fill_lambda(CommandContext& ctx, TechManagerModel& model) {
  const editor::EditorState& editor = *ctx.editor();
  model.lambda_numerator = std::to_string(editor.grid.lambda);
  model.lambda_denominator = std::to_string(editor.grid.internal);
  // cif scale output (µm per internal unit) × λden/λnum = µm per lambda.
  const double per_unit = ui::detail::microns_per_unit(editor);
  model.microns_per_lambda = format_g(per_unit * static_cast<double>(editor.grid.internal) /
                                      static_cast<double>(editor.grid.lambda));
}

// make_new_label's `label` with a font (texthelper.tcl:308-325).
std::vector<std::string> font_label(const TextHelperModel& m) {
  const std::string size = m.size.empty() || m.size == "0" ? "1" : m.size;
  const std::string rot = m.rotation.empty() ? "0" : m.rotation;
  const std::string off = m.offset.empty() ? "0 0" : m.offset;
  std::vector<std::string> label = {"label", m.text, m.font, size + "um", rot, off};
  if (m.default_layer || m.layer.empty()) {
    if (m.justification != "default") {
      label.push_back(m.justification);
    }
    return label;
  }
  label.push_back(m.justification == "default" ? "center" : m.justification);
  label.push_back(m.sticky ? "-" + m.layer : m.layer);
  return label;
}

std::string just_or_default(const std::string& just) {
  return just == "default" ? "-1" : just;
}

}  // namespace

// --- tags --------------------------------------------------------------------------------

const std::vector<std::pair<std::string, std::string>>& gui_script_tags(GuiScript script) {
  using Tags = std::vector<std::pair<std::string, std::string>>;
  static const Tags kWrapper = {{"load", "magic::captions"},
                                {"edit", "magic::captions"},
                                {"save", "magic::captions"},
                                {"down", "magic::captions"},
                                {"box", "magic::boxview %W %1"},
                                {"move", "magic::boxview %W"},
                                {"scroll", "magic::scrollupdate %W"},
                                {"view", "magic::scrollupdate %W"},
                                {"zoom", "magic::scrollupdate %W"},
                                {"findbox", "magic::scrollupdate %W"},
                                {"see", "magic::toolupdate %W %1 %2"},
                                {"tech", "magic::techrebuild %W %1; magic::captions %1"},
                                {"drc", "magic::drcupdate %1"},
                                {"path", "magic::techmanager update"},
                                {"cellname", "magic::mgrupdate %W %1"},
                                {"cif", "magic::mgrupdate %W %1"},
                                {"gds", "magic::mgrupdate %W %1"}};
  static const Tags kCellManager = {{"select", "magic::mgrselect %r"},
                                    {"load", "catch {magic::clearstack}; magic::cellmanager"},
                                    {"getcell", "magic::cellmanager"}};
  static const Tags kLibraryManager = {{"addpath", "magic::libmanager"},
                                       {"path", "magic::libmanager"},
                                       {"save", "magic::libmanager"},
                                       {"writeall", "magic::libmanager"},
                                       {"select", "magic::libmanager %1"}};
  static const Tags kDrcManager = {{"addpath", "magic::drcmanager"}, {"path", "magic::drcmanager"}};
  static const Tags kTextHelper = {{"select", "magic::update_texthelper"}};
  switch (script) {
    case GuiScript::wrapper: return kWrapper;
    case GuiScript::cell_manager: return kCellManager;
    case GuiScript::library_manager: return kLibraryManager;
    case GuiScript::drc_manager: return kDrcManager;
    case GuiScript::text_helper: break;
  }
  return kTextHelper;
}

void install_gui_tags(script::TagTable& tags, GuiScript script) {
  for (const auto& [command, text] : gui_script_tags(script)) {
    const auto existing = tags.get(command);
    tags.set(command, existing && !existing->empty() ? *existing + " ; " + text : text);
  }
}

// --- cell manager ------------------------------------------------------------------------

CellManagerModel cell_manager_model(CommandContext& ctx) {
  CellManagerModel model;
  model.window = "default";
  if (ctx.editor() == nullptr) {
    return model;
  }
  if (const script::Interpreter* interp = ctx.editor()->script.interp.get()) {
    if (auto target = interp->get_var("Opts(target)")) {
      model.window = target->to_string();
    }
  }
  (void)run(ctx, {"suspendall"});
  for (const std::string& top : cell_list(ctx, {"cellname", "list", "topcells"})) {
    const CellManagerRow row{top, top, {}, 0};
    model.rows.push_back(row);
    for (CellManagerRow& child : cell_manager_children(ctx, row)) {
      model.rows.push_back(std::move(child));
    }
  }
  (void)run(ctx, {"resumeall"});
  return model;
}

std::vector<CellManagerRow> cell_manager_children(CommandContext& ctx, const CellManagerRow& row) {
  std::vector<CellManagerRow> out;
  for (const std::string& child : cell_list(ctx, {"cellname", "list", "children", row.cell})) {
    out.push_back(CellManagerRow{row.id + "/" + child, child, first_instance(ctx, child), row.depth + 1});
  }
  return out;
}

Commands cell_manager_commands(CellManagerAction action, const CellManagerRow& row, std::string_view instance_path) {
  const bool top = row.depth == 0;
  switch (action) {
    case CellManagerAction::load: return {{"load", row.cell}};
    case CellManagerAction::edit:
      return top ? Commands{{"select", "top", "cell"}, {"expand"}, {"edit"}}
                 : Commands{{"select", "cell", std::string{instance_path}}, {"expand"}, {"edit"}};
    case CellManagerAction::expand:
      return top ? Commands{{"select", "top", "cell"}, {"expand"}}
                 : Commands{{"select", "cell", std::string{instance_path}}, {"expand", "toggle"}};
    case CellManagerAction::place: return {{"getcell", row.cell}};
    case CellManagerAction::zoom:
      return top ? Commands{{"view"}} : Commands{{"select", "cell", std::string{instance_path}}, {"findbox", "zoom"}};
  }
  return {};
}

// --- library manager ---------------------------------------------------------------------

std::optional<std::string> mag_file_technology(std::string_view text) {
  std::size_t pos = 0;
  if (pos >= text.size() || first_line(text, pos) != "magic" || pos >= text.size()) {
    return std::nullopt;
  }
  const std::string line = first_line(text, pos);
  if (!line.starts_with("tech ") || line.find(' ', 5) != std::string::npos || line.size() == 5) {
    return std::nullopt;
  }
  return line.substr(5);
}

LibraryManagerModel library_manager_model(CommandContext& ctx, bool filter) {
  LibraryManagerModel model;
  if (ctx.editor() == nullptr) {
    return model;
  }
  const std::string tech = run(ctx, {"tech", "name"});
  std::vector<std::string> seen;  // normalized directories already listed (Q-SCRIPT-27)
  const auto add = [&](const std::string& dir) {
    const std::string normal = detail::session_path(ctx, dir).lexically_normal().generic_string();
    if (std::find(seen.begin(), seen.end(), normal) != seen.end()) {
      return;
    }
    seen.push_back(normal);
    add_directory_rows(ctx, model, dir, tech, filter);
  };
  for (const std::string& dir : detail::words_of(detail::run_value(ctx, {"path", "search"}))) {
    add(dir);
  }
  for (const std::string& entry : detail::words_of(detail::run_value(ctx, {"path", "cell"}))) {
    // Q-CELL-17: path variables from the table, never script substitution.
    add(cell::expand_entry(entry, ctx.editor()->path_variables).value_or(entry));
  }
  return model;
}

Commands library_manager_commands(LibraryManagerAction action, std::string_view cell) {
  const std::string c{cell};
  switch (action) {
    case LibraryManagerAction::load: return {{"load", c}};
    case LibraryManagerAction::place: return {{"getcell", c, "child", "0", "0"}};
    case LibraryManagerAction::pick: return {{"tool", "pick"}, {"getcell", c, "child", "0", "0"}, {"startselect", "pick"}};
  }
  return {};
}

// --- technology manager ------------------------------------------------------------------

namespace {

// The elements of `tech version` as `lindex` reads them: Magic's technology
// manager shows `[lindex [tech version] 0]` and `[lindex [tech version] 1]`
// (wrapper.tcl). With magic-compat ON the value is Magic's string, whose
// description element may be braced or backslash-quoted; it is split with
// Tcl's own list rules. A malformed list (lindex would raise) shows nothing.
std::vector<std::string> tech_version_elements(const cmd::Value& value) {
  if (std::holds_alternative<cmd::Value::List>(value.data())) {
    return detail::words_of(value);
  }
  auto elements = pcell::split_tcl_list(value.to_string());
  return elements.ok() ? std::move(elements).value() : std::vector<std::string>{};
}

}  // namespace

TechManagerModel tech_manager_model(CommandContext& ctx) {
  TechManagerModel model;
  if (ctx.editor() == nullptr) {
    return model;
  }
  model.technology = run(ctx, {"tech", "name"});
  const std::vector<std::string> version = tech_version_elements(detail::run_value(ctx, {"tech", "version"}));
  if (!version.empty()) {
    model.version = "(version " + version.front() + ")";
  }
  if (version.size() > 1) {
    model.description = version[1];  // a list element: no braces or backslashes (SHELL-B open item 3)
  }
  fill_lambda(ctx, model);
  fill_styles(ctx, model);
  add_tech_files(ctx, model);
  add_pdk_entries(ctx, model);
  return model;
}

std::vector<std::string> tech_manager_scalegrid(std::int64_t lval0, std::int64_t lval1, std::int64_t lambda_num,
                                                std::int64_t lambda_den) {
  return {"scalegrid", std::to_string(lval0 * lambda_num), std::to_string(lval1 * lambda_den)};
}

// --- text helper -------------------------------------------------------------------------

Commands new_label_commands(const TextHelperModel& m) {
  if (m.text.empty()) {
    return {};
  }
  if (m.font.empty() || m.font == "default") {
    // Q-SCRIPT-31: a justification `default` reads as the default font; no port step follows.
    return {{"label", m.text, m.justification}};
  }
  Commands out = {font_label(m)};
  if (m.port && !m.port_index.empty()) {
    out.push_back({"port", "make", m.port_index});
  } else {
    out.push_back({"port", "remove"});
  }
  return out;
}

Commands change_label_commands(const TextHelperModel& m, bool port_exists) {
  Commands out;
  if (!m.text.empty()) {
    out.push_back({"setlabel", "text", m.text});
  }
  if (!m.font.empty()) {
    out.push_back({"setlabel", "font", m.font == "default" ? "-1" : m.font});
  }
  if (!m.justification.empty()) {
    out.push_back({"setlabel", "justify", just_or_default(m.justification)});
  }
  if (!m.size.empty()) {
    out.push_back({"setlabel", "size", m.size});
  }
  if (!m.offset.empty()) {
    out.push_back({"setlabel", "offset", m.offset});
  }
  if (!m.rotation.empty()) {
    out.push_back({"setlabel", "rotate", m.rotation});
  }
  if (!m.layer.empty()) {
    out.push_back({"setlabel", "layer", m.layer});
  }
  out.push_back({"setlabel", "sticky", m.sticky ? "1" : "0"});
  if (m.port && !m.port_index.empty()) {
    out.push_back(port_exists ? std::vector<std::string>{"port", "index", m.port_index}
                              : std::vector<std::string>{"port", "make", m.port_index});
  } else if (port_exists) {
    out.push_back({"port", "remove"});
  }
  return out;
}

CommandResult text_helper_apply(CommandContext& ctx, TextHelperModel& model) {
  if (ctx.editor() == nullptr) {
    return detail::no_session(ctx);
  }
  if (!run(ctx, {"setlabel", "text"}).empty()) {
    const bool port_exists = run(ctx, {"port", "exists"}) == "1";
    const CommandResult units = ui::detail::run_nested(ctx, {"units"});
    (void)run(ctx, {"units", "microns"});
    for (const auto& words : change_label_commands(model, port_exists)) {
      (void)run(ctx, words);
    }
    std::vector<std::string> restore = {"units"};
    for (std::string& w : detail::words_of(units.value)) {
      restore.push_back(std::move(w));
    }
    (void)run(ctx, restore);
    return CommandResult::success();
  }
  for (const auto& words : new_label_commands(model)) {
    (void)run(ctx, words);
  }
  if (!model.text.empty() && model.port && !model.port_index.empty()) {
    const auto last = detail::number_of(detail::run_value(ctx, {"port", "last"}));
    model.port_index = std::to_string(static_cast<long long>(last.value_or(0.0)) + 1);
  }
  return CommandResult::success();
}

std::vector<std::string> text_helper_fonts(CommandContext& ctx) {
  std::vector<std::string> out = {"default"};
  for (std::string& font : detail::words_of(detail::run_value(ctx, {"setlabel", "fontlist"}))) {
    out.push_back(std::move(font));
  }
  return out;
}

}  // namespace hefesto::gui
