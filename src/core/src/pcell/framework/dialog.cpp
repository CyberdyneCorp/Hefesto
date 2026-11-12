// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Port of toolkit.tcl's dialog procedures onto DialogModel (Magic ref:
// tcltk/toolkit.tcl:811-865,1285-1771). Tk widgets become DialogWidget rows;
// Tk bindings and -command options become the widgets' handler scripts and
// the buttons' scripts. Q-SCRIPT-25: add_dependency without parameter names
// takes the keys of the device's _defaults (Magic takes the procedure's
// name as the dictionary and fails), in both magic-compat settings.

#include "hefesto/pcell/framework/dialog.hpp"

#include <regex>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/framework/dialog_ops.hpp"

namespace hefesto::pcell {

ParameterDict DialogModel::parameters() const {
  ParameterDict result;
  for (const auto& widget : widgets) {
    if (widget.kind != WidgetKind::message) {
      result.set(widget.parameter, widget.value);
    }
  }
  return result;
}

DialogWidget* DialogModel::find(std::string_view pname) {
  for (auto& widget : widgets) {
    if (widget.parameter == pname && widget.kind != WidgetKind::message) {
      return &widget;
    }
  }
  return nullptr;
}

std::string_view mode_label(DialogMode mode) noexcept {
  return mode == DialogMode::edit_device ? "Edit device:" : "New device:";
}

namespace detail {

namespace {

constexpr std::string_view kEdits = ".params.body.area.edits";

cmd::CommandResult no_dialog_error() {
  return script_error("bad window path name \"" + std::string{kEdits} + "\"");
}

cmd::CommandResult raise_error(const diag::Diagnostic& error) {
  return cmd::CommandResult::raised(error);
}

std::string widget_suffix(WidgetKind kind) {
  switch (kind) {
    case WidgetKind::entry: return "_ent";
    case WidgetKind::checkbox: return "_chk";
    case WidgetKind::message: return "_txt";
    case WidgetKind::selectlist:
    case WidgetKind::selectindex: return "_sel";
  }
  return {};
}

// Copies the magic::<pname>_val variables into the widgets (PDK _check
// procedures write them directly).
void sync_values(Toolkit& tk) {
  if (!tk.state().dialog) {
    return;
  }
  for (auto& widget : tk.state().dialog->widgets) {
    if (tk.has_dialog_value(widget.parameter)) {
      widget.value = tk.dialog_value(widget.parameter);
    }
  }
}

// `string map {\\ ""}` then the backslash substitution of Tcl's `subst` on a
// plain instance name.
std::string strip_backslashes(std::string_view name) {
  std::string out;
  for (const char c : name) {
    if (c != '\\') {
      out += c;
    }
  }
  return out;
}

std::string escape_brackets(std::string_view name) {
  std::string out;
  for (const char c : name) {
    if (c == '[' || c == ']') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

struct EditTarget {
  std::string instance;
  std::string cell;
};

// The instance's cell, with an array subscript stripped (toolkit.tcl:1656-1672).
EditTarget locate_instance(Toolkit& tk, const std::string& instance) {
  const std::string base = strip_backslashes(instance);
  std::smatch m;
  std::string root;
  EditTarget t{instance, {}};
  if (std::regex_match(base, m, std::regex{R"(^(.*)\[[0-9,]+\]$)"})) {
    root = m[1].str();
    t.instance = root;
  }
  t.cell = tk.query({"instance", "list", "celldef", strip_backslashes(t.instance)});
  if (t.cell.empty() && !root.empty()) {
    t.cell = tk.query({"instance", "list", "celldef", base});
    t.instance = instance;
  }
  return t;
}

struct DialogSpec {
  DialogMode mode = DialogMode::new_device;
  std::string instance;  // "" for a new device
  std::string type;
  std::string library;
  std::string cell_name;
  std::string instance_text;
  ParameterDict parameters;
};

// The edit-mode spec of an existing instance; nullopt when it is not a gencell.
diag::Outcome<std::optional<DialogSpec>> edit_spec(Toolkit& tk, DialogSpec spec, const ParameterDict& given) {
  const EditTarget target = locate_instance(tk, spec.instance);
  spec.instance = target.instance;
  spec.type = tk.query({"cellname", "list", "property", target.cell, "gencell"});
  if (spec.library.empty()) {
    spec.library = tk.query({"cellname", "list", "property", target.cell, "library"});
  }
  auto stored = given.empty() ? ParameterDict::from_tcl(tk.query({"cellname", "list", "property", target.cell,
                                                                   "parameters"}))
                              : diag::Outcome<ParameterDict>{given};
  if (!stored.ok()) {
    return stored.error();
  }
  if (spec.type.empty() || spec.library.empty()) {
    return std::optional<DialogSpec>{};
  }
  spec.parameters = std::move(stored).value();
  if (spec.parameters.empty()) {
    auto defaults = tk.device_defaults(spec.library, spec.type);
    if (!defaults.ok()) {
      return defaults.error();
    }
    spec.parameters = std::move(defaults).value();
  }
  if (spec.parameters.contains("nocell")) {
    tk.engine({"select", "cell", spec.instance});
    set_array_parameters(tk, spec.parameters);
  }
  spec.mode = DialogMode::edit_device;
  spec.cell_name = target.cell;
  spec.instance_text = spec.instance;
  return std::optional<DialogSpec>{std::move(spec)};
}

diag::Outcome<DialogSpec> new_spec(Toolkit& tk, DialogSpec spec, const ParameterDict& given) {
  auto defaults = tk.device_defaults(spec.library, spec.type);
  if (!defaults.ok()) {
    return defaults.error();
  }
  spec.parameters = defaults.value().merged(given);
  spec.mode = DialogMode::new_device;
  spec.cell_name = "(default)";
  spec.instance_text = "(default)";
  return spec;
}

void set_new_buttons(DialogModel& dialog, const std::string& type, const std::string& library) {
  const std::string create = "set inst [magic::gencell_create " + type + " " + library +
                             " {}] ; magic::gencell_dialog $inst " + type + " " + library + " {}";
  dialog.buttons = {{"Create", create, false}, {"Create and Close", create, true}};
}

void add_common_buttons(DialogModel& dialog, const std::string& type, const std::string& library) {
  dialog.buttons.push_back({"Reset", "magic::gencell_dialog {} " + type + " " + library + " {}", false});
  dialog.buttons.push_back({"Close", "", true});
}

// Builds (or reuses and empties) the dialog for `spec`.
void build_dialog(Toolkit& tk, const DialogSpec& spec) {
  auto& slot = tk.state().dialog;
  if (!slot) {
    slot.emplace();
  }
  DialogModel& d = *slot;
  d.mode = spec.mode;
  d.device = spec.type;
  d.library = spec.library;
  d.cell_name = spec.cell_name;
  d.instance = spec.instance_text;
  d.widgets.clear();
  d.dependencies.clear();
  d.messages.clear();
  if (spec.mode == DialogMode::new_device) {
    set_new_buttons(d, spec.type, spec.library);
  } else {
    set_edit_buttons(d, spec.instance, spec.type, spec.library);
  }
  add_common_buttons(d, spec.type, spec.library);
}

// The type and library shown by an open edit dialog (gencell_dialog with an
// empty type and instance); false when there is nothing to show.
bool reuse_shown_device(Toolkit& tk, DialogSpec& spec) {
  if (!tk.state().dialog) {
    return true;  // Magic goes on and fails on the empty type
  }
  if (tk.state().dialog->mode != DialogMode::edit_device) {
    return false;
  }
  spec.type = tk.state().dialog->device;
  spec.library = tk.state().dialog->library;
  return true;
}

// ---- builders -------------------------------------------------------------------

struct BuilderArgs {
  std::string pname;
  std::string text;
  std::vector<std::string> choices;
  std::string parameters;
  std::string fallback;
  std::string color = "blue";
};

std::string_view builder_usage(WidgetKind kind) {
  switch (kind) {
    case WidgetKind::entry: return "magic::add_entry pname ptext parameters";
    case WidgetKind::checkbox: return "magic::add_checkbox pname ptext parameters";
    case WidgetKind::message: return "magic::add_message pname ptext parameters ?color?";
    case WidgetKind::selectlist: return "magic::add_selectlist pname ptext all_values parameters ?itext?";
    case WidgetKind::selectindex: return "magic::add_selectindex pname ptext all_values parameters ?ival?";
  }
  return {};
}

std::optional<BuilderArgs> parse_builder(WidgetKind kind, const std::vector<std::string>& a) {
  const bool select = kind == WidgetKind::selectlist || kind == WidgetKind::selectindex;
  const std::size_t fixed = select ? 4 : 3;
  const std::size_t optional = kind == WidgetKind::entry || kind == WidgetKind::checkbox ? 0 : 1;
  if (a.size() < fixed || a.size() > fixed + optional) {
    return std::nullopt;
  }
  BuilderArgs b{a[0], a[1], {}, a[fixed - 1], kind == WidgetKind::selectindex ? "0" : "", "blue"};
  if (select) {
    auto choices = split_tcl_list(a[2]);
    b.choices = choices.ok() ? std::move(choices).value() : std::vector<std::string>{};
  }
  if (a.size() > fixed) {
    (kind == WidgetKind::message ? b.color : b.fallback) = a[fixed];
  }
  return b;
}

bool has_row(const DialogModel& dialog, std::string_view pname) {
  for (const auto& w : dialog.widgets) {
    if (w.parameter == pname) {
      return true;
    }
  }
  return false;
}

}  // namespace

void set_edit_buttons(DialogModel& dialog, const std::string& instance, const std::string& type,
                      const std::string& library) {
  const std::string apply = "magic::gencell_change " + escape_brackets(instance) + " " + type + " " + library + " {}";
  std::vector<DialogButton> rest;
  for (auto& b : dialog.buttons) {
    if (b.text == "Reset" || b.text == "Close") {
      rest.push_back(b);
    }
  }
  dialog.buttons = {{"Apply", apply, false}, {"Okay", apply, true}};
  dialog.buttons.insert(dialog.buttons.end(), rest.begin(), rest.end());
}

cmd::CommandResult open_dialog(Toolkit& tk, std::string instance, std::string type, std::string library,
                               ParameterDict parameters) {
  const DepthGuard depth{tk.state()};
  DialogSpec spec{DialogMode::new_device, std::move(instance), std::move(type), std::move(library), {}, {}, {}};
  if (spec.type.empty() && spec.instance.empty() && !reuse_shown_device(tk, spec)) {
    return cmd::CommandResult::success();
  }
  if (!spec.instance.empty()) {
    auto edit = edit_spec(tk, spec, parameters);
    if (!edit.ok()) {
      return raise_error(edit.error());
    }
    if (!edit.value()) {
      return cmd::CommandResult::success();
    }
    spec = std::move(*edit.value());
  } else {
    auto made = new_spec(tk, spec, parameters);
    if (!made.ok()) {
      return raise_error(made.error());
    }
    spec = std::move(made).value();
  }
  build_dialog(tk, spec);
  const cmd::CommandResult built = tk.device(spec.library, spec.type, "dialog", {spec.parameters.to_tcl()});
  if (!built.ok()) {
    return script_error(error_text(built));
  }
  const cmd::CommandResult checks = add_check_callbacks(tk, spec.type, spec.library);
  sync_values(tk);
  if (!checks.ok()) {
    return checks;
  }
  ++tk.state().dialog->raise_count;  // raise .params
  return cmd::CommandResult::success();
}

diag::Outcome<ParameterDict> dialog_parameters(Toolkit& tk) {
  if (!tk.state().dialog) {
    return no_dialog_error().diagnostics.front();
  }
  // Magic reads them through Tk's `grid slaves`, which lists the last row
  // first (toolkit.tcl:819-833).
  ParameterDict result;
  const std::vector<DialogWidget>& widgets = tk.state().dialog->widgets;
  for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
    const DialogWidget& widget = *it;
    if (widget.kind == WidgetKind::message) {
      continue;
    }
    if (!tk.has_dialog_value(widget.parameter)) {
      return script_error("can't read \"magic::" + widget.parameter + "_val\": no such variable").diagnostics.front();
    }
    result.set(widget.parameter, tk.dialog_value(widget.parameter));
  }
  return result;
}

cmd::CommandResult push_dialog_parameters(Toolkit& tk, const ParameterDict& parameters) {
  if (!tk.state().dialog) {
    return cmd::CommandResult::success();
  }
  // Copy: set_dialog_value() writes into the widgets.
  const std::vector<DialogWidget> widgets = tk.state().dialog->widgets;
  for (const auto& widget : widgets) {
    const auto value = parameters.get(widget.parameter);
    if (widget.kind == WidgetKind::message) {
      if (value) {
        tk.set_dialog_value(widget.parameter, std::string{*value});
      }
      continue;
    }
    if (widget.parameter == "gencell" && widget_suffix(widget.kind) == "_sel") {
      continue;  // .params.body.area.edits.gencell_sel is not a parameter
    }
    if (!value) {
      return script_error("key \"" + widget.parameter + "\" not known in dictionary");
    }
    tk.set_dialog_value(widget.parameter, std::string{*value});
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult add_widget(Toolkit& tk, WidgetKind kind, const std::vector<std::string>& args) {
  const auto b = parse_builder(kind, args);
  if (!b) {
    return wrong_args(builder_usage(kind));
  }
  auto parameters = ParameterDict::from_tcl(b->parameters);
  if (!parameters.ok()) {
    return raise_error(parameters.error());
  }
  if (!tk.state().dialog) {
    return no_dialog_error();
  }
  if (has_row(*tk.state().dialog, b->pname)) {
    return script_error("window name \"" + b->pname + "_lab\" already exists in parent");
  }
  const auto given = parameters.value().get(b->pname);
  const std::string value = given ? std::string{*given} : b->fallback;
  DialogWidget widget{kind, b->pname, b->text, value, b->choices, kind == WidgetKind::message ? b->color : "", {}};
  tk.state().dialog->widgets.push_back(std::move(widget));
  if (kind != WidgetKind::message || !tk.has_dialog_value(b->pname)) {
    tk.set_dialog_value(b->pname, value);
  } else {
    sync_values(tk);
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult add_dependency(Toolkit& tk, const std::string& callback, const std::string& type,
                                  const std::string& library, std::vector<std::string> parameters) {
  if (parameters.empty()) {
    // Q-SCRIPT-25: every parameter of the device's _defaults.
    auto defaults = tk.device_defaults(library, type);
    if (!defaults.ok()) {
      return raise_error(defaults.error());
    }
    for (const auto& [key, value] : defaults.value().entries()) {
      parameters.push_back(key);
    }
    if (parameters.empty()) {
      return cmd::CommandResult::success();
    }
  }
  if (!tk.state().dialog) {
    return no_dialog_error();
  }
  DialogModel& dialog = *tk.state().dialog;
  for (const auto& pname : parameters) {
    if (DialogWidget* w = dialog.find(pname)) {
      w->handlers.push_back("magic::update_dialog " + callback + " " + pname + " " + type + " " + library);
    }
  }
  if (callback != "{}") {
    dialog.dependencies.push_back({callback, type, library, parameters});
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult add_check_callbacks(Toolkit& tk, const std::string& type, const std::string& library) {
  if (!tk.state().dialog) {
    return no_dialog_error();
  }
  // winfo children: the label and the widget of each row both match
  // {\.params\.body\.area\.edits\.(.+)_.+}, so each row is bound twice.
  std::vector<std::string> rows;
  for (const auto& w : tk.state().dialog->widgets) {
    rows.push_back(w.parameter);
  }
  for (const auto& pname : rows) {
    for (int child = 0; child < 2; ++child) {
      const cmd::CommandResult r = add_dependency(tk, "{}", type, library, {pname});
      if (!r.ok()) {
        return r;
      }
    }
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult update_dialog(Toolkit& tk, const std::string& callback, const std::string& pname,
                                 std::string type, const std::string& library) {
  const DepthGuard depth{tk.state()};
  auto dialog = dialog_parameters(tk);
  if (!dialog.ok()) {
    return raise_error(dialog.error());
  }
  if (const auto t = dialog.value().get("gencell")) {
    type = std::string{*t};
  }
  auto defaults = tk.device_defaults(library, type);
  if (!defaults.ok()) {
    return raise_error(defaults.error());
  }
  ParameterDict p = tk.checked(library, type, defaults.value().merged(dialog.value()));
  if (!callback.empty()) {
    const std::vector<std::string> words{callback, pname, p.to_tcl()};
    const cmd::CommandResult r = tk.evaluate(merge_tcl_list(words));
    if (!r.ok()) {
      return script_error(error_text(r));
    }
    auto returned = ParameterDict::from_tcl(value_text(r.value));
    if (!returned.ok()) {
      return raise_error(returned.error());
    }
    p = std::move(returned).value();
  }
  const cmd::CommandResult pushed = push_dialog_parameters(tk, p);
  sync_values(tk);
  return pushed;
}

cmd::CommandResult gencell_update(Toolkit& tk, const std::string& command) {
  // [info level] <= 1: only a top-level `select`, not one run by a
  // framework or device procedure.
  if (tk.state().depth > 0 || !tk.state().dialog || command != "cell") {
    return cmd::CommandResult::success();
  }
  const auto what = tk.query_list({"what", "-list"});
  std::string instance;
  if (what.size() > 2) {
    auto cells = split_tcl_list(what[2]);
    if (cells.ok() && !cells.value().empty()) {
      auto pair = split_tcl_list(cells.value().front());
      instance = pair.ok() && !pair.value().empty() ? pair.value().front() : std::string{};
    }
  }
  return open_dialog(tk, instance, "", "", {});
}

}  // namespace detail

}  // namespace hefesto::pcell
