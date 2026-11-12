// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Port of toolkit.tcl's magic::gencell, gencell_makecell, gencell_change,
// gencell_create and gencell_defaults (Magic ref: tcltk/toolkit.tcl:641-
// 1283), with the decided fixes Q-SCRIPT-23 (a fixed device's array counts
// and pitch in the non-interactive change) and Q-SCRIPT-29 (a headless
// change keeps the instance name and resumes redraw), in both magic-compat
// settings.

#include "hefesto/pcell/framework/gencell.hpp"

#include <regex>

#include "hefesto/pcell/framework/naming.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/framework/codes.hpp"
#include "pcell/framework/dialog_ops.hpp"
#include "pcell/framework/toolkit.hpp"

namespace hefesto::pcell {

using detail::Toolkit;

namespace {

cmd::CommandResult raise_outcome(const diag::Diagnostic& error) {
  return cmd::CommandResult::raised(error);
}

diag::Outcome<ParameterDict> defaults_of(Toolkit& tk, const std::string& type, const std::string& library,
                                         const ParameterDict& parameters) {
  auto base = tk.device_defaults(library, type);
  if (!base.ok()) {
    return base.error();
  }
  return base.value().merged(parameters);
}

bool exists_flag(const std::string& text) {
  return !text.empty() && text != "0";
}

// ---- gencell dispatch ------------------------------------------------------

struct Dispatch {
  Toolkit& tk;
  std::string library;
  std::string type;
  std::string instance;
  ParameterDict arguments;
  bool spice = false;
};

std::vector<std::string> split_or_empty(const std::string& text) {
  auto words = split_tcl_list(text);
  return words.ok() ? std::move(words).value() : std::vector<std::string>{};
}

// {instance cell} of the first selected instance (`lindex [what -list] 2 0`).
std::vector<std::string> first_selected(Toolkit& tk) {
  const auto what = tk.query_list({"what", "-list"});
  const auto cells = what.size() > 2 ? split_or_empty(what[2]) : std::vector<std::string>{};
  return cells.empty() ? std::vector<std::string>{} : split_or_empty(cells.front());
}

// gencell with an empty name: edit the first selected instance.
cmd::CommandResult edit_selection(Toolkit& tk) {
  const std::vector<std::string> pair = first_selected(tk);
  const std::string instance = !pair.empty() ? pair[0] : std::string{};
  const std::string cell = pair.size() > 1 ? pair[1] : std::string{};
  std::string library = tk.query({"cellname", "list", "property", cell, "library"});
  if (library.empty()) {
    library = "toolkit";
  }
  std::string type = tk.query({"cellname", "list", "property", cell, "gencell"});
  if (type.empty()) {
    std::smatch m;
    if (!std::regex_match(cell, m, std::regex{"^(.*)_[0-9]*$"})) {
      return detail::script_error("No gencell device is selected!");
    }
    type = m[1].str();
  }
  auto stored = ParameterDict::from_tcl(tk.query({"cellname", "list", "property", cell, "parameters"}));
  if (!stored.ok()) {
    return raise_outcome(stored.error());
  }
  auto parameters = defaults_of(tk, type, library, stored.value());
  if (!parameters.ok()) {
    return raise_outcome(parameters.error());
  }
  return detail::open_dialog(tk, instance, type, library, parameters.value());
}

diag::Outcome<ParameterDict> converted(Dispatch& d) {
  if (!d.spice) {
    return d.arguments;
  }
  return d.tk.device_dict(d.library, d.type, "convert", d.arguments);
}

// An existing instance: open its dialog, or change it non-interactively.
cmd::CommandResult change_existing(Dispatch& d, const std::string& cell) {
  d.tk.engine({"select", "cell", d.instance});
  auto stored = ParameterDict::from_tcl(d.tk.query({"cellname", "list", "property", cell, "parameters"}));
  if (!stored.ok()) {
    return raise_outcome(stored.error());
  }
  auto parameters = defaults_of(d.tk, d.type, d.library, stored.value());
  if (!parameters.ok()) {
    return raise_outcome(parameters.error());
  }
  ParameterDict p = std::move(parameters).value();
  if (p.contains("nocell")) {
    detail::set_array_parameters(d.tk, p);
  }
  if (d.arguments.empty()) {
    const cmd::CommandResult r = detail::open_dialog(d.tk, d.instance, d.type, d.library, p);
    return r.ok() ? cmd::CommandResult::success(0) : r;
  }
  auto args = converted(d);
  if (!args.ok()) {
    return raise_outcome(args.error());
  }
  const cmd::CommandResult r = change(d.tk.context(), d.instance, d.type, d.library, p.merged(args.value()));
  return r.ok() ? cmd::CommandResult::success(0) : r;
}

cmd::CommandResult create_named(Dispatch& d) {
  auto args = converted(d);
  if (!args.ok()) {
    return raise_outcome(args.error());
  }
  auto parameters = defaults_of(d.tk, d.type, d.library, args.value());
  if (!parameters.ok()) {
    return raise_outcome(parameters.error());
  }
  const cmd::CommandResult made = create(d.tk.context(), d.type, d.library, parameters.value());
  if (!made.ok()) {
    return made;
  }
  d.tk.engine({"select", "cell", detail::value_text(made.value)});
  d.tk.engine({"identify", d.instance});
  return cmd::CommandResult::success(0);
}

cmd::CommandResult new_device_dialog(Dispatch& d) {
  if (d.spice && d.tk.has_device(d.library, d.type, "convert")) {
    auto args = converted(d);
    if (!args.ok()) {
      return raise_outcome(args.error());
    }
    d.arguments = std::move(args).value();
  }
  auto parameters = defaults_of(d.tk, d.type, d.library, d.arguments);
  if (!parameters.ok()) {
    return raise_outcome(parameters.error());
  }
  const cmd::CommandResult r = detail::open_dialog(d.tk, "", d.type, d.library, parameters.value());
  return r.ok() ? cmd::CommandResult::success(0) : r;
}

cmd::CommandResult dispatch_named(Dispatch& d) {
  if (!d.tk.has_device(d.library, d.type, "convert")) {
    return detail::script_error("No import routine for " + d.library + " library cell " + d.type + "!");
  }
  if (d.instance.empty()) {
    return new_device_dialog(d);
  }
  const std::string cell = d.tk.query({"instance", "list", "celldef", d.instance});
  return cell.empty() ? create_named(d) : change_existing(d, cell);
}

// ---- create ------------------------------------------------------------------

struct CreatePlan {
  std::string type;
  ParameterDict parameters;
  std::string instance;  // the explicit name from the dialog, or ""
};

diag::Outcome<CreatePlan> plan_from_dialog(Toolkit& tk, const std::string& type, const std::string& library) {
  auto dialog = detail::dialog_parameters(tk);
  if (!dialog.ok()) {
    return dialog.error();
  }
  CreatePlan plan{type, {}, {}};
  if (const auto t = dialog.value().get("gencell")) {
    plan.type = std::string{*t};
  }
  auto merged = defaults_of(tk, plan.type, library, dialog.value());
  if (!merged.ok()) {
    return merged.error();
  }
  plan.parameters = std::move(merged).value();
  plan.instance = tk.state().dialog ? tk.state().dialog->instance : std::string{};
  if (plan.instance == "(default)" || !tk.query({"instance", "list", "exists", plan.instance}).empty()) {
    plan.instance.clear();
  }
  return plan;
}

diag::Outcome<CreatePlan> plan_from_arguments(Toolkit& tk, const std::string& type, const std::string& library,
                                              const ParameterDict& parameters) {
  CreatePlan plan{type, {}, {}};
  if (const auto t = parameters.get("gencell")) {
    plan.type = std::string{*t};
  }
  auto merged = defaults_of(tk, plan.type, library, parameters);
  if (!merged.ok()) {
    return merged.error();
  }
  plan.parameters = std::move(merged).value();
  return plan;
}

// The checked parameters, pushed to the dialog, without `gencell`.
ParameterDict finish_parameters(Toolkit& tk, const std::string& library, const std::string& type,
                                const ParameterDict& parameters) {
  ParameterDict p = tk.checked(library, type, parameters);
  detail::push_dialog_parameters(tk, p);
  p.erase("gencell");
  return p;
}

void focus_pointer(Toolkit& tk) {
  // catch {setpoint 0 0 $Opts(focus)}
  static_cast<void>(tk.evaluate("catch {setpoint 0 0 $::Opts(focus)}"));
}

// Draws generated cell `name` (cellname create, pushstack, _draw, the three
// properties, popstack); draw errors are printed.
void draw_generated_cell(Toolkit& tk, const std::string& name, const std::string& library,
                         const std::string& type, const ParameterDict& parameters) {
  tk.engine({"cellname", "create", name});
  tk.engine({"pushstack", name});
  const cmd::CommandResult drawn = tk.draw_device(library, type, parameters);
  if (!drawn.ok()) {
    tk.print_error(detail::error_text(drawn));
  }
  tk.engine({"property", "library", library});
  tk.engine({"property", "gencell", type});
  tk.engine({"property", "parameters", parameters.to_tcl()});
  tk.engine({"popstack"});
}

std::vector<std::string> box_values_command(const std::vector<std::string>& values) {
  std::vector<std::string> words{"box", "values"};
  words.insert(words.end(), values.begin(), values.end());
  return words;
}

diag::Outcome<std::string> place_device(Toolkit& tk, const std::string& library, const std::string& type,
                                        const ParameterDict& parameters, const std::string& orientation) {
  // `set savebox [box values]`: a plain engine call, so a missing box prints
  // "Box tool must be present" as in Magic (SCRIPT-PDK-14).
  const auto savebox = tk.engine_list({"box", "values"});
  focus_pointer(tk);
  std::string instance;
  if (parameters.contains("nocell")) {
    const cmd::CommandResult drawn = tk.draw_device(library, type, parameters);
    if (!drawn.ok()) {
      tk.print_error(detail::error_text(drawn));
      return diag::make(codes::kScriptError, "can't read \"instname\": no such variable");
    }
    instance = detail::value_text(drawn.value);
    tk.engine(box_values_command(savebox));
    return instance;
  }
  const std::string name = generated_cell_name(type, parameters);
  draw_generated_cell(tk, name, library, type, parameters);
  tk.engine(box_values_command(savebox));
  instance = tk.engine({"getcell", name, orientation});
  tk.engine({"expand"});
  return instance;
}

// ---- change ------------------------------------------------------------------

struct ChangeState {
  std::string instance;      // the instance being changed (renamed when the dialog says so)
  std::string new_instance;  // the final name
  std::string old_cell;
  std::string new_cell;
};

std::string requested_instance_name(Toolkit& tk, const std::string& instance) {
  // Q-SCRIPT-29: without a dialog the entry counts as "(default)".
  std::string name = tk.state().dialog ? tk.state().dialog->instance : instance;
  if (name == "(default)" || !tk.query({"instance", "list", "exists", name}).empty()) {
    name = instance;
  }
  return name;
}

void update_change_buttons(Toolkit& tk, const std::string& instance, const std::string& type,
                           const std::string& library) {
  if (tk.state().dialog) {
    detail::set_edit_buttons(*tk.state().dialog, instance, type, library);
  }
}

// Replaces the instance by one of `cell` (existing or created), keeping its
// orientation and abutment-box position; deletes the old cell when unused.
void replace_instance(Toolkit& tk, ChangeState& s, const std::string& library, const std::string& type,
                      const ParameterDict& parameters, bool exists) {
  tk.engine({"select", "cell", s.instance});
  const std::string orientation = tk.query({"instance", "list", "orientation"});
  const auto abox = tk.query_list({"instance", "list", "abutment"});
  tk.engine({"delete"});
  if (!abox.empty()) {
    tk.engine(box_values_command(abox));
  }
  std::string placed;
  if (exists) {
    placed = tk.engine({"getcell", s.new_cell, orientation});
    tk.engine({"select", "cell", placed});
    tk.engine({"expand"});
  } else {
    const cmd::CommandResult made = create(tk.context(), type, library, parameters, orientation);
    placed = made.ok() ? detail::value_text(made.value) : std::string{};
    tk.engine({"select", "cell", placed});
  }
  s.new_instance = placed;
  if (s.instance.rfind(s.old_cell, 0) != 0) {
    s.new_instance = s.instance;  // a user-assigned name is kept
  } else {
    update_change_buttons(tk, s.new_instance, type, library);
  }
  if (tk.query_list({"cellname", "list", "parents", s.old_cell}).empty()) {
    tk.engine({"cellname", "delete", s.old_cell, "-noprompt"});
    tk.engine({"select", "cell", placed});
  }
}

void replace_fixed_device(Toolkit& tk, ChangeState& s, const std::string& library, const std::string& type,
                          const ParameterDict& parameters) {
  tk.engine({"select", "cell", s.instance});
  const auto abox = tk.query_list({"instance", "list", "abutment"});
  tk.engine({"delete"});
  if (!abox.empty()) {
    tk.engine(box_values_command(abox));
  }
  // toolkit.tcl stores the drawn instance in `newinst`; the final
  // `identify $newinstname` then gives it the changed instance's name.
  const cmd::CommandResult drawn = tk.draw_device(library, type, parameters);
  if (!drawn.ok()) {
    tk.print_error(detail::error_text(drawn));
  }
  tk.engine({"select", "cell", drawn.ok() ? detail::value_text(drawn.value) : std::string{}});
}

void regenerate(Toolkit& tk, ChangeState& s, const std::string& library, const std::string& type,
                const ParameterDict& parameters) {
  const detail::InternalUnits units{tk};
  // `set savebox [box values]`: a plain engine call, so a missing box prints
  // "Box tool must be present" as in Magic (SCRIPT-PDK-14).
  const auto savebox = tk.engine_list({"box", "values"});
  focus_pointer(tk);
  if (parameters.contains("nocell")) {
    replace_fixed_device(tk, s, library, type, parameters);
  } else {
    const bool exists = exists_flag(tk.query({"cellname", "list", "exists", s.new_cell}));
    replace_instance(tk, s, library, type, parameters, exists);
  }
  tk.engine({"identify", s.new_instance});
  tk.engine(box_values_command(savebox));
}

void refresh_dialog_title(Toolkit& tk, const ChangeState& s) {
  if (!tk.state().dialog) {
    return;
  }
  if (s.new_cell != s.old_cell) {
    tk.state().dialog->cell_name = s.new_cell;
  }
  if (s.instance != s.new_instance) {
    tk.state().dialog->instance = s.new_instance;
  }
}

diag::Outcome<std::pair<std::string, ParameterDict>> change_parameters(Toolkit& tk, const std::string& type,
                                                                       const std::string& library,
                                                                       const ParameterDict& parameters) {
  ParameterDict p = parameters;
  if (p.empty()) {
    auto dialog = detail::dialog_parameters(tk);
    if (!dialog.ok()) {
      return dialog.error();
    }
    auto merged = defaults_of(tk, type, library, dialog.value());
    if (!merged.ok()) {
      return merged.error();
    }
    p = std::move(merged).value();
  }
  return std::pair{type, p};
}

}  // namespace

cmd::CommandResult gencell(cmd::CommandContext& ctx, const GencellRequest& request) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return raise_outcome(opened.error());
  }
  Toolkit& tk = opened.value();
  const detail::DepthGuard depth{tk.state()};
  std::vector<std::string> args = request.args;
  const bool spice = !args.empty() && args.front() == "-spice";
  if (spice) {
    args.erase(args.begin());
  }
  auto arguments = detail::dict_create(args);
  if (!arguments.ok()) {
    return raise_outcome(arguments.error());
  }
  if (request.gencell.empty()) {
    const cmd::CommandResult r = edit_selection(tk);
    return r.ok() ? cmd::CommandResult::success(0) : r;
  }
  auto name = detail::split_device_name(request.gencell);
  Dispatch d{tk, name ? name->library : "toolkit", name ? name->type : request.gencell, request.instance,
             std::move(arguments).value(), spice};
  return dispatch_named(d);
}

cmd::CommandResult create(cmd::CommandContext& ctx, const std::string& type, const std::string& library,
                          const ParameterDict& parameters, const std::string& orientation) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return raise_outcome(opened.error());
  }
  Toolkit& tk = opened.value();
  const detail::DepthGuard depth{tk.state()};
  std::string instance;
  {
    const detail::Suspended suspended{tk};
    auto plan = parameters.empty() ? plan_from_dialog(tk, type, library)
                                   : plan_from_arguments(tk, type, library, parameters);
    if (!plan.ok()) {
      return raise_outcome(plan.error());
    }
    const CreatePlan& p = plan.value();
    const ParameterDict final_parameters = finish_parameters(tk, library, p.type, p.parameters);
    const detail::InternalUnits units{tk};
    auto placed = place_device(tk, library, p.type, final_parameters, orientation);
    if (!placed.ok()) {
      return raise_outcome(placed.error());
    }
    instance = std::move(placed).value();
    if (!p.instance.empty()) {
      tk.engine({"identify", p.instance});
      instance = p.instance;
    }
  }
  tk.engine({"redraw"});
  return cmd::CommandResult::success(instance);
}

cmd::CommandResult change(cmd::CommandContext& ctx, const std::string& instance, const std::string& type,
                          const std::string& library, const ParameterDict& parameters) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return raise_outcome(opened.error());
  }
  Toolkit& tk = opened.value();
  const detail::DepthGuard depth{tk.state()};
  ChangeState s;
  {
    const detail::Suspended suspended{tk};
    auto input = change_parameters(tk, type, library, parameters);
    if (!input.ok()) {
      return raise_outcome(input.error());
    }
    s.instance = instance;
    s.new_instance = requested_instance_name(tk, instance);
    std::string device = type;
    if (const auto t = input.value().second.get("gencell")) {
      device = std::string{*t};
    }
    const ParameterDict p = finish_parameters(tk, library, device, input.value().second);
    s.old_cell = tk.query({"instance", "list", "celldef", instance});
    s.new_cell = generated_cell_name(device, p);
    if (s.new_instance != s.instance) {
      tk.engine({"identify", s.new_instance});
      update_change_buttons(tk, s.new_instance, device, library);
      s.instance = s.new_instance;
    }
    if (s.new_cell.empty() || s.new_cell == s.old_cell) {
      return cmd::CommandResult::success();
    }
    regenerate(tk, s, library, device, p);
    refresh_dialog_title(tk, s);
  }
  tk.engine({"drc", "check"});
  tk.engine({"redraw"});
  return cmd::CommandResult::success();
}

cmd::CommandResult makecell(cmd::CommandContext& ctx, const std::string& fullname,
                            const std::vector<std::string>& args) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return raise_outcome(opened.error());
  }
  Toolkit& tk = opened.value();
  const detail::DepthGuard depth{tk.state()};
  auto arguments = detail::dict_create(args);
  if (!arguments.ok()) {
    return raise_outcome(arguments.error());
  }
  // namespace tail / namespace qualifiers
  const auto sep = fullname.rfind("::");
  const std::string type = sep == std::string::npos ? fullname : fullname.substr(sep + 2);
  const std::string library = sep == std::string::npos ? std::string{} : fullname.substr(0, sep);
  auto parameters = defaults_of(tk, type, library, arguments.value());
  if (!parameters.ok()) {
    return raise_outcome(parameters.error());
  }
  const std::string name = generated_cell_name(type, parameters.value());
  const detail::Suspended suspended{tk};
  draw_generated_cell(tk, name, library, type, parameters.value());
  return cmd::CommandResult::success(name);
}

cmd::CommandResult defaults(cmd::CommandContext& ctx, const std::string& type, const std::string& library,
                            const ParameterDict& parameters) {
  auto opened = Toolkit::open(ctx);
  if (!opened.ok()) {
    return raise_outcome(opened.error());
  }
  auto merged = defaults_of(opened.value(), type, library, parameters);
  if (!merged.ok()) {
    return raise_outcome(merged.error());
  }
  return cmd::CommandResult::success(merged.value().to_tcl());
}

}  // namespace hefesto::pcell
