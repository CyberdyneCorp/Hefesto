// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "pcell/framework/toolkit.hpp"

#include <variant>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/interp.hpp"
#include "pcell/framework/codes.hpp"
#include "ui/command_util.hpp"

namespace hefesto::pcell::detail {

namespace {

std::string device_command(std::string_view library, std::string_view type, std::string_view suffix) {
  // Called as toolkit.tcl calls it (`${library}::${gencell_type}_check`), so
  // Tcl's messages name the procedure the same way.
  return std::string{library} + "::" + std::string{type} + "_" + std::string{suffix};
}

std::string variable_name(std::string_view pname) {
  return "::magic::" + std::string{pname} + "_val";
}

}  // namespace

cmd::CommandResult script_error(std::string text) {
  return cmd::CommandResult::raised(diag::make(codes::kScriptError, std::move(text)));
}

cmd::CommandResult wrong_args(std::string_view usage) {
  return script_error("wrong # args: should be \"" + std::string{usage} + "\"");
}

std::string value_text(const cmd::Value& value) {
  return tcl_string(value);
}

std::string error_text(const cmd::CommandResult& result) {
  return result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text;
}

diag::Outcome<Toolkit> Toolkit::open(cmd::CommandContext& ctx) {
  auto editor = cmd::require_editor(ctx);
  if (!editor.ok()) {
    return editor.error();
  }
  editor::EditorState& state = *editor.value();
  if (!state.script.interp) {
    return diag::make(codes::kNoInterpreter, "no script interpreter is open for this project");
  }
  return Toolkit{ctx, *state.script.interp, state.script.gencell};
}

std::string Toolkit::engine(const std::vector<std::string>& words) {
  const cmd::CommandResult r = ui::detail::run_forwarded(*ctx_, words);
  return r.ok() ? value_text(r.value) : std::string{};
}

std::vector<std::string> Toolkit::engine_list(const std::vector<std::string>& words) {
  auto split = split_tcl_list(engine(words));
  return split.ok() ? std::move(split).value() : std::vector<std::string>{};
}

std::string Toolkit::query(const std::vector<std::string>& words) {
  // A context whose sink drops what the query prints.
  diag::NullSink quiet;
  cmd::CommandContext silent{ctx_->services(), ctx_->project(), ctx_->session(), quiet, ctx_->registry(),
                             ctx_->editor()};
  const cmd::CommandResult r = ui::detail::run_nested(silent, words);
  return r.ok() ? value_text(r.value) : std::string{};
}

std::vector<std::string> Toolkit::query_list(const std::vector<std::string>& words) {
  auto split = split_tcl_list(query(words));
  return split.ok() ? std::move(split).value() : std::vector<std::string>{};
}

cmd::CommandResult Toolkit::device(std::string_view library, std::string_view type, std::string_view suffix,
                                   const std::vector<std::string>& args) {
  std::vector<std::string> words{device_command(library, type, suffix)};
  words.insert(words.end(), args.begin(), args.end());
  return interp_->evaluate(merge_tcl_list(words));
}

bool Toolkit::has_device(std::string_view library, std::string_view type, std::string_view suffix) {
  return interp_->has_command("::" + device_command(library, type, suffix));
}

diag::Outcome<ParameterDict> Toolkit::device_defaults(std::string_view library, std::string_view type) {
  if (const NativeGenerator* generator = native(library, type)) {
    return generator->defaults();
  }
  const cmd::CommandResult r = device(library, type, "defaults", {});
  if (!r.ok()) {
    return diag::make(codes::kScriptError, error_text(r));
  }
  return ParameterDict::from_tcl(value_text(r.value));
}

diag::Outcome<ParameterDict> Toolkit::device_dict(std::string_view library, std::string_view type,
                                                  std::string_view suffix, const ParameterDict& parameters) {
  const cmd::CommandResult r = device(library, type, suffix, {parameters.to_tcl()});
  if (!r.ok()) {
    return diag::make(codes::kScriptError, error_text(r));
  }
  return ParameterDict::from_tcl(value_text(r.value));
}

ParameterDict Toolkit::checked(std::string_view library, std::string_view type, const ParameterDict& parameters) {
  if (auto native_result = native_check(library, type, parameters)) {
    if (!native_result->ok()) {
      print_error(native_result->error().text);
      return parameters;
    }
    return std::move(*native_result).value();
  }
  auto result = device_dict(library, type, "check", parameters);
  if (!result.ok()) {
    print_error(result.error().text);
    return parameters;
  }
  return std::move(result).value();
}

cmd::CommandResult Toolkit::evaluate(std::string_view script) {
  return interp_->evaluate(script);
}

void Toolkit::print_error(std::string text) {
  ctx_->emit(diag::make(codes::kDeviceMessage, std::move(text)));
}

void Toolkit::print(std::string text) {
  ctx_->emit(diag::make(codes::kNetlistReport, std::move(text)));
}

std::string Toolkit::dialog_value(std::string_view pname) const {
  const auto value = interp_->get_var(variable_name(pname));
  return value ? value_text(*value) : std::string{};
}

bool Toolkit::has_dialog_value(std::string_view pname) const {
  return interp_->get_var(variable_name(pname)).has_value();
}

void Toolkit::set_dialog_value(std::string_view pname, const std::string& value) {
  static_cast<void>(interp_->evaluate("namespace eval ::magic {}"));
  static_cast<void>(interp_->set_var(variable_name(pname), cmd::Value{value}));
  if (state_->dialog) {
    if (DialogWidget* w = state_->dialog->find(pname)) {
      w->value = value;
    }
  }
}

InternalUnits::InternalUnits(Toolkit& tk) : tk_(tk) {
  saved_ = tk_.query_list({"units"});
  tk_.engine({"units", "internal"});
}

InternalUnits::~InternalUnits() {
  std::vector<std::string> words{"units"};
  words.insert(words.end(), saved_.begin(), saved_.end());
  tk_.engine(words);
}

std::optional<DeviceName> split_device_name(std::string_view name) {
  const auto sep = name.find("::");
  if (sep == std::string_view::npos || sep == 0 || sep + 2 >= name.size()) {
    return std::nullopt;
  }
  const std::string_view library = name.substr(0, sep);
  const std::string_view type = name.substr(sep + 2);
  if (library.find(':') != std::string_view::npos || type.find(':') != std::string_view::npos) {
    return std::nullopt;
  }
  return DeviceName{std::string{library}, std::string{type}};
}

namespace {

std::vector<std::string> array_values(Toolkit& tk, const char* option) {
  std::vector<std::string> values = tk.query_list({"array", "-list", option});
  if (values.size() == 1) {
    auto first = split_tcl_list(values.front());
    values = first.ok() ? std::move(first).value() : std::vector<std::string>{};
  }
  return values;
}

std::int64_t to_int(const std::string& text) {
  try {
    return std::stoll(text);
  } catch (const std::exception&) {
    return 0;
  }
}

}  // namespace

void set_array_parameters(Toolkit& tk, ParameterDict& parameters) {
  const auto count = array_values(tk, "count");
  const auto pitch = array_values(tk, "pitch");
  if (count.size() == 4) {
    parameters.set("nx", std::to_string(to_int(count[1]) - to_int(count[0]) + 1));
    parameters.set("ny", std::to_string(to_int(count[3]) - to_int(count[2]) + 1));
  }
  if (pitch.size() == 2) {
    parameters.set("pitchx", pitch[0]);
    parameters.set("pitchy", pitch[1]);
  }
}

diag::Outcome<ParameterDict> dict_create(const std::vector<std::string>& words) {
  if (words.size() % 2 != 0) {
    return diag::make(codes::kScriptError, "wrong # args: should be \"dict create ?key value ...?\"");
  }
  ParameterDict dict;
  for (std::size_t i = 0; i < words.size(); i += 2) {
    dict.set(words[i], words[i + 1]);
  }
  return dict;
}

}  // namespace hefesto::pcell::detail
