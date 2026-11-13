// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/pcell_dialog_model.hpp"

#include <algorithm>
#include <cctype>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/pcell/native/registry.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::shell {

namespace {

using pcell::DialogModel;
using pcell::DialogMode;
using pcell::DialogWidget;
using pcell::WidgetKind;

std::string lower(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool contains_folded(std::string_view text, std::string_view filter) {
  return filter.empty() || lower(text).find(lower(filter)) != std::string::npos;
}

/// `sky130::sky130_fd_pr__res_xhigh_po_draw`: a draw procedure called
/// directly (board note 2: paints at the box and opens no dialog).
bool is_draw_script(std::string_view script) {
  const std::size_t word_end = script.find(' ');
  const std::string_view word = script.substr(0, word_end);
  return word.find("::") != std::string_view::npos && word.size() > 5 && word.substr(word.size() - 5) == "_draw";
}

std::string_view mode_text(DialogMode mode) {
  return pcell::mode_label(mode);
}

std::optional<std::size_t> choice_index(const DialogWidget& w) {
  if (w.kind == WidgetKind::selectindex) {
    std::size_t index = 0;
    for (const char c : w.value) {
      if (c < '0' || c > '9') return std::nullopt;
      index = index * 10 + static_cast<std::size_t>(c - '0');
    }
    if (w.value.empty() || index >= w.choices.size()) return std::nullopt;
    return index;
  }
  const auto found = std::find(w.choices.begin(), w.choices.end(), w.value);
  if (found == w.choices.end()) return std::nullopt;
  return static_cast<std::size_t>(found - w.choices.begin());
}

PCellRow row_of(const DialogWidget& w, const PCellEditState& edits) {
  PCellRow row;
  row.kind = w.kind;
  row.parameter = w.parameter;
  row.label = w.label;
  row.value = w.value;
  row.choices = w.choices;
  row.color = w.color;
  if (w.kind == WidgetKind::selectlist || w.kind == WidgetKind::selectindex) row.choice = choice_index(w);
  if (w.kind == WidgetKind::checkbox) row.checked = tcl_true(w.value);
  if (w.kind != WidgetKind::entry) return row;
  if (const auto typed = edits.typed.find(w.parameter); typed != edits.typed.end() && typed->second != w.value) {
    row.corrected = true;
    row.typed = typed->second;
  }
  if (const auto old = edits.before.find(w.parameter);
      old != edits.before.end() && w.parameter != edits.edited && old->second != w.value) {
    row.updated = true;
  }
  return row;
}

std::string banner_of(const std::vector<std::string>& messages) {
  std::string text;
  for (const std::string& m : messages) {
    if (m.empty()) continue;
    text += (text.empty() ? "" : "\n") + m;
  }
  return text.empty() ? text : "▲ " + text;
}

std::vector<PCellButton> buttons_of(DialogMode mode) {
  using R = PCellButtonRole;
  if (mode == DialogMode::new_device) {
    return {{"Reset", R::reset, false, false},
            {"Close", R::close, false, true},
            {"Create", R::create, false, false},
            {"Create and Close", R::create_and_close, true, true}};
  }
  return {{"Reset", R::reset, false, false},
          {"Close", R::close, false, true},
          {"Apply", R::apply, false, false},
          {"Okay", R::okay, true, true}};
}

std::string announcement_of(const std::vector<PCellRow>& rows) {
  for (const PCellRow& row : rows) {
    if (row.updated) return row.label + " updated to " + row.value;
  }
  for (const PCellRow& row : rows) {
    if (row.corrected) return row.label + " corrected to " + row.value;
  }
  return {};
}

std::string instance_word(std::string_view instance) {
  if (instance.empty() || instance == "(default)") return "{}";
  const std::string text{instance};
  return pcell::merge_tcl_list(std::span<const std::string>(&text, 1));
}

std::string reset_script(const DialogModel& model) {
  for (const pcell::DialogButton& b : model.buttons) {
    if (b.text == "Reset") return b.script;
  }
  return "magic::gencell_dialog {} " + model.device + " " + model.library + " {}";
}

}  // namespace

// --- Place device -------------------------------------------------------------------

/// The command items of `menu` whose label contains `filter`.
static std::vector<PlaceDeviceEntry> matching_items(const pcell::ToolkitMenu& menu, std::string_view filter) {
  std::vector<PlaceDeviceEntry> items;
  for (const pcell::ToolkitItem& item : menu.items) {
    if (item.kind == pcell::ToolkitItem::Kind::command && contains_folded(item.text, filter)) {
      items.push_back({item.text, item.command, false, is_draw_script(item.command), false});
    }
  }
  return items;
}

/// The label of the first command item running `script` (empty: none).
static std::string label_of(const pcell::ToolkitMenus& menus, std::string_view script) {
  if (script.empty()) return {};
  for (const pcell::ToolkitMenu& menu : menus.menus) {
    for (const pcell::ToolkitItem& item : menu.items) {
      if (item.kind == pcell::ToolkitItem::Kind::command && item.command == script) return item.text;
    }
  }
  return {};
}

PlaceDeviceView place_device_view(const pcell::ToolkitMenus& menus, std::string_view pdk_name, bool scripts_declined,
                                  std::string_view filter, std::string_view recent) {
  PlaceDeviceView view;
  view.title = pdk_name.empty() ? "Place device" : "Place device · " + std::string{pdk_name};
  view.enabled = !scripts_declined && !menus.menus.empty();
  if (!view.enabled) {
    view.tooltip = scripts_declined ? "No PDK device scripts (the PDK scripts were not run)" : "No PDK device scripts";
    return view;
  }
  view.tooltip = "Place device: the PDK's device generators";
  for (const pcell::ToolkitMenu& menu : menus.menus) {
    std::vector<PlaceDeviceEntry> items = matching_items(menu, filter);
    if (items.empty()) continue;
    view.entries.push_back({menu.text, {}, true, false, false});
    view.entries.insert(view.entries.end(), items.begin(), items.end());
  }
  if (const std::string label = label_of(menus, recent); !label.empty()) {
    view.entries.push_back({"Recent: " + label, std::string{recent}, false, is_draw_script(recent), true});
  }
  return view;
}

std::optional<std::size_t> first_device_row(const std::vector<PlaceDeviceEntry>& entries) {
  const auto it = std::find_if(entries.begin(), entries.end(), [](const PlaceDeviceEntry& e) { return !e.heading; });
  if (it == entries.end()) return std::nullopt;
  return static_cast<std::size_t>(it - entries.begin());
}

std::optional<std::size_t> step_device_row(const std::vector<PlaceDeviceEntry>& entries,
                                           std::optional<std::size_t> row, int step) {
  if (!row || *row >= entries.size()) return first_device_row(entries);
  if (step == 0) return row;
  std::size_t i = *row;
  while (step > 0 ? i + 1 < entries.size() : i > 0) {
    i = step > 0 ? i + 1 : i - 1;
    if (!entries[i].heading) return i;
  }
  return row;
}

// --- parameter dialog ---------------------------------------------------------------

void PCellEditState::begin_edit(const DialogModel& model, std::string_view parameter, std::string_view value) {
  before.clear();
  for (const DialogWidget& w : model.widgets) {
    if (w.kind != WidgetKind::message) before[w.parameter] = w.value;
  }
  edited = std::string{parameter};
  typed[edited] = std::string{value};
}

bool PCellEditState::begin_typing(std::string_view parameter) {
  const auto it = typed.find(parameter);
  if (it == typed.end()) return false;
  typed.erase(it);
  return true;
}

void PCellEditState::clear() noexcept {
  typed.clear();
  before.clear();
  edited.clear();
}

bool tcl_true(std::string_view value) noexcept {
  const std::string v = lower(value);
  if (v == "true" || v == "yes" || v == "on") return true;
  if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos) return false;
  return v.find_first_not_of('0') != std::string::npos;
}

PCellDialogView pcell_dialog_view(const DialogModel& model, bool native, const PCellEditState& edits) {
  PCellDialogView view;
  view.mode = std::string{mode_text(model.mode)};
  view.device = model.device;
  view.library = model.library;
  view.cell_name = model.cell_name;
  view.instance = model.instance;
  view.generator = native ? StatusChip{StatusKind::success, "native generator"}
                          : StatusChip{StatusKind::info, "Tcl generator"};
  for (const DialogWidget& w : model.widgets) view.rows.push_back(row_of(w, edits));
  view.banner = banner_of(model.messages);
  view.buttons = buttons_of(model.mode);
  view.announcement = announcement_of(view.rows);
  return view;
}

std::string pcell_value_script(const DialogWidget& widget, std::string_view value) {
  const std::string text{value};
  std::string script = "set ::magic::" + widget.parameter + "_val " +
                       pcell::merge_tcl_list(std::span<const std::string>(&text, 1));
  for (const std::string& handler : widget.handlers) script += " ; " + handler;
  return script;
}

namespace {

/// `k v ...` of every parameter of the dialog, each word Tcl-quoted.
std::string parameter_words(const DialogModel& model) {
  const pcell::ParameterDict parameters = model.parameters();  // by value: keep it alive for the loop
  std::vector<std::string> words;
  for (const auto& [key, value] : parameters.entries()) {
    words.emplace_back(key);
    words.emplace_back(value);
  }
  return pcell::merge_tcl_list(words);
}

bool default_instance(std::string_view instance) {
  return instance.empty() || instance == "(default)";
}

}  // namespace

std::string pcell_gencell_command(const DialogModel& model) {
  const std::string words = parameter_words(model);
  if (model.mode == DialogMode::new_device && default_instance(model.instance)) {
    // Magic's `gencell <lib>::<type> {} k v` only opens the dialog, so a new
    // device without a name is created by gencell_create with the whole
    // dictionary (self-contained and logged like the named form).
    const std::string dict = pcell::merge_tcl_list(std::span<const std::string>(&words, 1));
    return "magic::gencell_create " + model.device + " " + model.library + " " + dict;
  }
  return "magic::gencell " + model.library + "::" + model.device + " " + instance_word(model.instance) +
         (words.empty() ? std::string{} : " " + words);
}

std::string pcell_button_script(const DialogModel& model, PCellButtonRole role) {
  switch (role) {
    case PCellButtonRole::create:
      return "magic::gencell_dialog [" + pcell_gencell_command(model) + "] " + model.device + " " + model.library +
             " {}";
    case PCellButtonRole::create_and_close:
    case PCellButtonRole::apply:
    case PCellButtonRole::okay: return pcell_gencell_command(model);
    case PCellButtonRole::reset: return reset_script(model);
    case PCellButtonRole::close: return {};
  }
  return {};
}

std::optional<script::PdkIdentity> technology_pdk(const tech::Technology& technology) {
  if (technology.file_name().empty()) return std::nullopt;
  return script::pdk_identity_of(platform::Path{technology.file_name()});
}

bool native_generator_available(const std::optional<script::PdkIdentity>& pdk, std::string_view device) {
  return pdk && pcell::builtin_generators().find(*pdk, device) != nullptr;
}

}  // namespace hefesto::shell
