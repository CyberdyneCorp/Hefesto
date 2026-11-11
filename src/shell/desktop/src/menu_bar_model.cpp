// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/menu_bar_model.hpp"

#include <algorithm>
#include <cctype>
#include <functional>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/wrapper.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/shell/pcell_dialog_model.hpp"

namespace hefesto::shell {

namespace {

using Kind = MenuBarItem::Kind;

bool same_title(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

/// `(d)` -> `d` (gui::apply_accelerators writes Magic's parenthesized form).
std::string accelerator_text(std::string_view accelerator) {
  if (accelerator.size() >= 2 && accelerator.front() == '(' && accelerator.back() == ')') {
    return std::string{accelerator.substr(1, accelerator.size() - 2)};
  }
  return std::string{accelerator};
}

MenuBarItem separator_item() {
  MenuBarItem item;
  item.kind = Kind::separator;
  return item;
}

MenuBarItem from_wrapper(const gui::WrapperMenuItem& w, const MenuBarInputs& inputs) {
  if (w.kind == gui::WrapperMenuItem::Kind::separator) return separator_item();
  MenuBarItem item;
  item.kind = w.kind == gui::WrapperMenuItem::Kind::check ? Kind::check : Kind::command;
  item.label = collapse_spaces(w.label);
  item.script = w.command;
  item.accelerator = accelerator_text(w.accelerator);
  item.variable = w.variable;
  item.enabled = w.enabled;
  if (item.kind == Kind::check && inputs.option) item.checked = inputs.option(w.variable).value_or(false);
  if (const auto later = later_milestone(w.command)) {
    item.enabled = false;
    item.tooltip = item.label + ": arrives in " + std::string{*later};
  } else if (inputs.layout_read_only && saves_layout(w.command)) {
    item.enabled = false;
    item.tooltip = item.label + ": off, the layout is read-only";
  }
  return item;
}

MenuBarItem from_pdk(const script::MenuItem& m) {
  if (m.kind == script::MenuItem::Kind::separator) return separator_item();
  MenuBarItem item;
  item.label = collapse_spaces(m.label);
  item.script = m.command;
  item.pdk = true;
  return item;
}

/// A wrapper item for a native entry of the PDK-edited menu, matched by its
/// label (the wrapper's has the accelerator and the M4 enablement).
MenuBarItem native_item(const script::MenuItem& m, const gui::WrapperMenu& wrapper, const MenuBarInputs& inputs) {
  if (m.kind == script::MenuItem::Kind::separator) return separator_item();
  const std::string label = collapse_spaces(m.label);
  for (const gui::WrapperMenuItem& w : wrapper.items) {
    if (w.kind != gui::WrapperMenuItem::Kind::separator && collapse_spaces(w.label) == label) {
      return from_wrapper(w, inputs);
    }
  }
  MenuBarItem item = from_pdk(m);
  item.pdk = false;
  return item;
}

/// The wrapper menu with the PDK's edits: a menu the PDK inserted into
/// holds Magic's entries itself (MenuItem::native), one it only appended to
/// gets the PDK items after the wrapper's.
MenuBarMenu wrapper_menu(const gui::WrapperMenu& wrapper, const std::vector<script::MenuItem>* edits,
                         const MenuBarInputs& inputs) {
  MenuBarMenu menu;
  menu.title = wrapper.title;
  const bool seeded = edits != nullptr && std::any_of(edits->begin(), edits->end(),
                                                      [](const script::MenuItem& m) { return m.native; });
  if (!seeded) {
    for (const gui::WrapperMenuItem& w : wrapper.items) menu.items.push_back(from_wrapper(w, inputs));
  }
  if (edits == nullptr) return menu;
  for (const script::MenuItem& m : *edits) {
    menu.items.push_back(m.native ? native_item(m, wrapper, inputs) : from_pdk(m));
  }
  return menu;
}

const std::vector<script::MenuItem>* edits_of(const script::MenuModel& pdk_menus, std::string_view title) {
  for (const std::string& name : pdk_menus.menus()) {
    if (same_title(name, title)) return &pdk_menus.items(name);
  }
  return nullptr;
}

bool is_wrapper_title(const gui::WrapperMenuModel& wrapper, std::string_view title) {
  return std::any_of(wrapper.menus.begin(), wrapper.menus.end(),
                     [title](const gui::WrapperMenu& m) { return same_title(m.title, title); });
}

MenuBarMenu toolkit_menu(const pcell::ToolkitMenu& t) {
  MenuBarMenu menu;
  menu.title = t.text;
  menu.pdk = true;
  menu.source = t.text + " · " + t.library;
  for (const pcell::ToolkitItem& i : t.items) {
    if (i.kind == pcell::ToolkitItem::Kind::separator) {
      menu.items.push_back(separator_item());
      continue;
    }
    MenuBarItem item;
    item.label = collapse_spaces(i.text);
    item.script = i.command;
    item.pdk = true;
    menu.items.push_back(std::move(item));
  }
  return menu;
}

/// Menus a PDK script created through intercepted widget calls that are not
/// wrapper menus (none in the three verified PDKs).
void add_other_pdk_menus(MenuBarView& view, const gui::WrapperMenuModel& wrapper, const script::MenuModel& pdk_menus) {
  for (const std::string& name : pdk_menus.menus()) {
    if (is_wrapper_title(wrapper, name)) continue;
    MenuBarMenu menu;
    menu.title = name;
    menu.pdk = true;
    menu.source = name;
    for (const script::MenuItem& m : pdk_menus.items(name)) menu.items.push_back(from_pdk(m));
    view.menus.push_back(std::move(menu));
  }
}

bool in_frame(const pcell::ToolkitMenu& menu, std::string_view frame) {
  if (frame.empty() || menu.frame.empty()) return true;
  return menu.frame.size() >= frame.size() && menu.frame.compare(0, frame.size(), frame) == 0 &&
         (menu.frame.size() == frame.size() || menu.frame[frame.size()] == '.');
}

void hash_mix(std::size_t& seed, std::size_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

std::string file_name(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return std::string{slash == std::string_view::npos ? path : path.substr(slash + 1)};
}

}  // namespace

std::string MenuBarItem::description() const {
  if (kind == Kind::separator) return {};
  return script.empty() ? label : label + " · " + script;
}

std::string collapse_spaces(std::string_view label) {
  std::string out;
  for (const char c : label) {
    if (c == ' ' && !out.empty() && out.back() == ' ') continue;
    out += c;
  }
  return out;
}

bool saves_layout(std::string_view command) {
  return command == "magic::promptsave magic" || command == "magic::prompt_dialog save";
}

std::optional<std::string_view> later_milestone(std::string_view command) {
  if (command == "magic::promptload gds" || command == "magic::promptsave gds") return "M5";
  if (command == "magic::specialopen netlist") return "M12";
  if (command == "magic::render3d") return "M10";
  return std::nullopt;
}

MenuBarView menu_bar_view(const gui::WrapperMenuModel& wrapper, const script::MenuModel& pdk_menus,
                          const pcell::ToolkitMenus& toolkit, const MenuBarInputs& inputs) {
  MenuBarView view;
  static const script::MenuModel kNone;
  const script::MenuModel& edits = inputs.scripts_declined ? kNone : pdk_menus;
  for (const gui::WrapperMenu& w : wrapper.menus) {
    view.menus.push_back(wrapper_menu(w, edits_of(edits, w.title), inputs));
  }
  view.divider = view.menus.size();
  if (inputs.scripts_declined) return view;
  for (const pcell::ToolkitMenu& t : toolkit.menus) {
    if (in_frame(t, inputs.frame)) view.menus.push_back(toolkit_menu(t));
  }
  add_other_pdk_menus(view, wrapper, edits);
  return view;
}

MenuBarView session_menu_bar(const editor::EditorState& editor, bool scripts_declined) {
  script::Interpreter* interp = editor.script.interp.get();
  MenuBarInputs inputs;
  inputs.frame = {};  // the toolkit menus of every frame: the desktop has one layout window
  inputs.scripts_declined = scripts_declined;
  const view::LayoutWindow* window = editor.windows.current();
  inputs.layout_read_only = window != nullptr && window->root() != nullptr && window->root()->flags().no_edit;
  inputs.option = [interp](std::string_view variable) -> std::optional<bool> {
    if (interp == nullptr) return std::nullopt;
    const auto value = interp->get_var(variable);
    if (!value) return std::nullopt;
    return tcl_true(value->to_string());
  };
  return menu_bar_view(gui::wrapper_menus(".layout1"), editor.script.menu_model, editor.script.gencell.menus, inputs);
}

std::string menu_item_script(const MenuBarItem& item) {
  if (item.kind != Kind::check || item.variable.empty()) return item.script;
  const std::string set = "set ::" + item.variable + " " + (item.checked ? "0" : "1");
  return item.script.empty() ? set : set + " ; " + item.script;
}

std::size_t menu_bar_signature(const MenuBarView& view) {
  std::size_t seed = view.divider;
  const std::hash<std::string> h;
  for (const MenuBarMenu& m : view.menus) {
    hash_mix(seed, h(m.title));
    for (const MenuBarItem& i : m.items) {
      hash_mix(seed, h(i.label + '\n' + i.script + '\n' + i.accelerator));
      hash_mix(seed, (static_cast<std::size_t>(i.kind) << 3U) | (i.checked ? 4U : 0U) | (i.enabled ? 2U : 0U) |
                         (i.pdk ? 1U : 0U));
    }
  }
  return seed;
}

std::string skipped_statement_row(const script::SkippedStatement& statement) {
  std::string where = file_name(statement.location.file);
  if (statement.location.line > 0) where += ":" + std::to_string(statement.location.line);
  std::string row = "▲ SCRIPT-PDK-004 skipped widget statement " + where + "\n" + statement.statement;
  if (!statement.procedure.empty()) {
    const std::string_view name = std::string_view{statement.procedure}.substr(
        statement.procedure.starts_with("::") ? 2 : 0);
    row += "\nin " + std::string{name} + " · evaluation continued";
  }
  return row;
}

std::string skipped_statements_message(const std::vector<script::SkippedStatement>& statements) {
  if (statements.empty()) return {};
  const std::string file = file_name(statements.front().location.file);
  const bool one_file = std::all_of(statements.begin(), statements.end(), [&file](const script::SkippedStatement& s) {
    return file_name(s.location.file) == file;
  });
  const std::string count = std::to_string(statements.size());
  const std::string noun = statements.size() == 1 ? " menu statement" : " menu statements";
  const std::string verb = statements.size() == 1 ? " was skipped" : " were skipped";
  return "▲ " + count + noun + (one_file && !file.empty() ? " of " + file : std::string{}) + verb;
}

}  // namespace hefesto::shell
