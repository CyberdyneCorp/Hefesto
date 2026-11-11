// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script variables read by the engine (see variables.hpp). The path cache
// follows Magic 8.3.694 (fc5daa9e) utils/path.c (the expansion table of
// PaExpand), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).

#include "hefesto/script/binding/variables.hpp"

#include <array>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "hefesto/app/settings_layers.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::script {

namespace {

constexpr std::string_view kChangedCode = "SCRIPT-BIND-001";

// The traces: the native callback does the work; a whole-variable unset
// deletes a variable's traces, so they are added again afterwards.
constexpr std::string_view kTraceScript = R"tcl(
namespace eval ::hefesto {}
proc ::hefesto::_engine_variable_trace {name1 name2 op} {
    ::hefesto::_engine_variable $op $name1 $name2
    if {$op eq "unset" && $name2 eq ""} {
        trace add variable ::[namespace tail $name1] {read write unset array} ::hefesto::_engine_variable_trace
    }
}
foreach ::hefesto::_v {VDD GND SUB globals} {
    trace add variable ::$::hefesto::_v {read write unset array} ::hefesto::_engine_variable_trace
}
unset ::hefesto::_v
)tcl";

struct PowerName {
  std::string_view variable;
  std::string_view setting;
};

constexpr std::array<PowerName, 3> kPowerNames{{{"VDD", app::kVdd}, {"GND", app::kGnd}, {"SUB", app::kSub}}};

std::string_view unqualified(std::string_view name) {
  return name.starts_with("::") ? name.substr(2) : name;
}

const PowerName* power_name(std::string_view variable) {
  for (const PowerName& p : kPowerNames) {
    if (p.variable == variable) {
      return &p;
    }
  }
  return nullptr;
}

// An unset field is absent or empty in the effective setting.
std::optional<std::string> effective_value(const app::SettingsStack& settings, std::string_view name) {
  auto value = settings.effective(name);
  if (!value || value->value.empty()) {
    return std::nullopt;
  }
  return value->value;
}

std::set<std::string> global_nets(const app::SettingsStack& settings) {
  std::set<std::string> names;
  if (auto value = effective_value(settings, app::kGlobalNets)) {
    std::istringstream words{*value};
    for (std::string word; words >> word;) {
      names.insert(word);
    }
  }
  return names;
}

// `globalNets` holds the names separated by single spaces, in byte order.
void set_global_nets(app::SettingsStack& settings, app::SettingLayer layer, const std::set<std::string>& names) {
  std::string joined;
  for (const std::string& name : names) {
    joined += (joined.empty() ? "" : " ") + name;
  }
  settings.set(layer, app::kGlobalNets, std::move(joined));
}

void set_element(Interpreter& interp, std::string_view net) {
  const std::string element = "::globals(" + std::string{net} + ")";
  if (!interp.get_var(element)) {
    interp.set_var(element, cmd::Value{"1"});
  }
}

void refresh_globals(Interpreter& interp, const app::SettingsStack& settings) {
  for (const std::string& net : global_nets(settings)) {
    set_element(interp, net);
  }
}

// One trace event on `name1`/`name2` (see kTraceScript).
void on_power_name(Interpreter& interp, app::SettingsStack& settings, const PowerName& p, std::string_view op) {
  const std::string variable = "::" + std::string{p.variable};
  if (op == "write") {
    const auto value = interp.get_var(variable);
    settings.set(app::SettingLayer::session, p.setting, value ? value->to_string() : std::string{});
  } else if (op == "unset") {
    settings.set(app::SettingLayer::session, p.setting, std::string{});
  } else if (const auto value = effective_value(settings, p.setting)) {
    interp.set_var(variable, cmd::Value{*value});  // read: the effective value
  }
}

void on_globals(Interpreter& interp, app::SettingsStack& settings, std::string_view element, std::string_view op) {
  std::set<std::string> names = global_nets(settings);
  if (op == "write" && !element.empty()) {
    names.insert(std::string{element});
  } else if (op == "unset" && element.empty()) {
    names.clear();
  } else if (op == "unset") {
    names.erase(std::string{element});
  } else {
    if (element.empty()) {
      refresh_globals(interp, settings);  // `array ...` on globals
    } else if (names.contains(std::string{element})) {
      set_element(interp, element);
    }
    return;
  }
  set_global_nets(settings, app::SettingLayer::session, names);
}

cmd::CommandResult engine_variable(Interpreter& interp, editor::EditorState& editor, const cmd::Invocation& call) {
  if (call.args.size() != 3) {
    return cmd::CommandResult::success();
  }
  const std::string_view op = call.args[0];
  const std::string_view name = unqualified(call.args[1]);
  if (const PowerName* p = power_name(name)) {
    on_power_name(interp, editor.settings, *p, op);
  } else if (name == "globals") {
    on_globals(interp, editor.settings, call.args[2], op);
  }
  return cmd::CommandResult::success();
}

void seed_variables(Interpreter& interp, const app::SettingsStack& settings) {
  for (const PowerName& p : kPowerNames) {
    if (const auto value = effective_value(settings, p.setting)) {
      interp.set_var("::" + std::string{p.variable}, cmd::Value{*value});
    }
  }
  refresh_globals(interp, settings);
}

// The script variable `name` as a string, when the project has an
// interpreter and the variable is set (an array has no string value).
std::optional<std::string> script_value(const editor::EditorState& editor, std::string_view name) {
  if (!editor.script.interp) {
    return std::nullopt;
  }
  const auto value = editor.script.interp->get_var("::" + std::string{name});
  return value ? std::optional{value->to_string()} : std::nullopt;
}

void wrap_path_variables(editor::EditorState& editor) {
  PathVariableCache& cache = editor.script.path_cache;
  if (cache.base()) {
    return;
  }
  cache.set_base(editor.path_variables.variable ? editor.path_variables.variable
                                                 : PathVariableCache::Resolver{[](std::string_view) {
                                                     return std::optional<std::string>{};
                                                   }});
  editor::EditorState* state = &editor;
  editor.path_variables.variable = [state](std::string_view name) -> std::optional<std::string> {
    PathVariableCache& paths = state->script.path_cache;
    std::optional<std::string> live = script_value(*state, name);
    if (!live) {
      live = (*paths.base())(name);
    }
    return paths.resolve(name, live, state->drc.compat_settings().magic_compat);
  };
}

// CAD_ROOT: "If unset at load time, it is set from the environment
// variable CAD_ROOT, or else the compiled default" (tclmagic.c:1658-1669).
void seed_cad_root(Interpreter& interp, const editor::EditorState& editor) {
  if (interp.get_var("::CAD_ROOT")) {
    return;
  }
  const auto& base = editor.script.path_cache.base();
  std::optional<std::string> root = base ? (*base)("CAD_ROOT") : std::nullopt;
  if (!root && !editor.path_variables.install_root.empty()) {
    root = editor.path_variables.install_root.string();
  }
  if (root) {
    interp.set_var("::CAD_ROOT", cmd::Value{*root});
  }
}

}  // namespace

cmd::CommandResult VariableTraces::install(Interpreter& interp, editor::EditorState& editor,
                                           std::function<void(const diag::Diagnostic&)> report) {
  seed_variables(interp, editor.settings);
  editor::EditorState* state = &editor;
  Interpreter* tcl = &interp;
  interp.define_command("::hefesto::_engine_variable", [tcl, state](const cmd::Invocation& call) {
    return engine_variable(*tcl, *state, call);
  });
  cmd::CommandResult traced = interp.evaluate(kTraceScript);
  if (!traced.ok()) {
    return traced;
  }
  editor.script.path_cache.set_reporter(std::move(report));
  wrap_path_variables(editor);
  seed_cad_root(interp, editor);
  return cmd::CommandResult::success();
}

void adopt_engine_variables(app::SettingsStack& settings, app::SettingLayer layer) {
  if (layer == app::SettingLayer::session) {
    return;
  }
  for (const std::string_view name : {app::kVdd, app::kGnd, app::kSub, app::kGlobalNets}) {
    if (auto value = settings.value_in(app::SettingLayer::session, name)) {
      settings.set(layer, name, std::move(*value));
      settings.clear(app::SettingLayer::session, name);
    }
  }
}

std::optional<std::string> PathVariableCache::lookup(std::string_view name) const {
  const auto it = values_.find(name);
  if (it == values_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void PathVariableCache::store(std::string_view name, std::string value) {
  values_.insert_or_assign(std::string{name}, std::move(value));
}

void PathVariableCache::clear() noexcept {
  values_.clear();
}

bool PathVariableCache::first_warning(std::string_view name) {
  return warned_.emplace(name).second;
}

std::optional<std::string> PathVariableCache::resolve(std::string_view name, const std::optional<std::string>& live,
                                                      bool magic_compat) {
  const std::optional<std::string> first = lookup(name);
  if (!first) {
    if (live) {
      store(name, *live);  // an unresolved name is retried on its next use
    }
    return live;
  }
  if (live != first && first_warning(name) && reporter_) {
    reporter_(diag::make_native(kChangedCode, "Path variable \"" + std::string{name} + "\" changed from \"" + *first +
                                                  "\" to \"" + live.value_or(std::string{}) + "\" after its first use; " +
                                                  (magic_compat ? "the first value is kept (magic-compat)."
                                                                : "the current value is used.")));
  }
  return magic_compat ? first : live;
}

}  // namespace hefesto::script
