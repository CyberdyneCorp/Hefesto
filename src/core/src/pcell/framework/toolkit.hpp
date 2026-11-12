// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers of the gencell framework: one Toolkit per running
// framework procedure, giving the ported toolkit.tcl code
//  - the engine commands it calls (run nested, as the M2 ports of
//    tools.tcl run them: not logged, diagnostics forwarded),
//  - the PDK device procedures (`<lib>::<type>_<suffix>`, evaluated in the
//    project's interpreter),
//  - Tcl's `error` (a raised PCELL-GEN-001) and `puts stderr` (a
//    PCELL-GEN-002 warning) and the dialog state.
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/pcell/framework/parameters.hpp"
#include "hefesto/pcell/framework/state.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::pcell::detail {

/// A Tcl error raised by a framework procedure (PCELL-GEN-001, raises).
[[nodiscard]] cmd::CommandResult script_error(std::string text);
/// Tcl's "wrong # args: should be \"<usage>\"".
[[nodiscard]] cmd::CommandResult wrong_args(std::string_view usage);
/// The Tcl string form of a command value (lists formatted as Tcl does).
[[nodiscard]] std::string value_text(const cmd::Value& value);

/// The error text of a failed result (its first diagnostic), or "".
[[nodiscard]] std::string error_text(const cmd::CommandResult& result);

class Toolkit {
 public:
  /// The toolkit of `ctx`'s session, or the PCELL-GEN-003 / CMD-SESSION
  /// failure when there is no editor session or no interpreter.
  [[nodiscard]] static diag::Outcome<Toolkit> open(cmd::CommandContext& ctx);

  /// Runs an engine command nested; its diagnostics are forwarded to the
  /// running command. The result's Tcl string form ("" after a failure, as
  /// Magic's quiet commands leave the result empty).
  std::string engine(const std::vector<std::string>& words);
  /// A query command (`instance list`, `cellname list`, `what -list`,
  /// `array -list`, `box values`, `units`) run nested without printing:
  /// Magic's Tcl build answers them quietly (e.g. an undefined property of
  /// `cellname list property` is an empty result). Its Tcl string form.
  std::string query(const std::vector<std::string>& words);
  /// query() split as a Tcl list (empty when malformed).
  std::vector<std::string> query_list(const std::vector<std::string>& words);

  /// Evaluates the device procedure `<library>::<type>_<suffix>` with `args`.
  [[nodiscard]] cmd::CommandResult device(std::string_view library, std::string_view type,
                                          std::string_view suffix, const std::vector<std::string>& args);
  /// True when `<library>::<type>_<suffix>` exists as a command.
  [[nodiscard]] bool has_device(std::string_view library, std::string_view type, std::string_view suffix);
  /// The device's _defaults as a dictionary (a script error otherwise).
  [[nodiscard]] diag::Outcome<ParameterDict> device_defaults(std::string_view library, std::string_view type);
  /// A device procedure returning a dictionary (_convert, _check, a
  /// dependency callback); failures are returned as the raised error.
  [[nodiscard]] diag::Outcome<ParameterDict> device_dict(std::string_view library, std::string_view type,
                                                         std::string_view suffix, const ParameterDict& parameters);
  /// `catch {set P [<lib>::<type>_check $P]} err; puts stderr $err`.
  [[nodiscard]] ParameterDict checked(std::string_view library, std::string_view type, const ParameterDict& parameters);

  /// Evaluates `script` in the interpreter (global level).
  [[nodiscard]] cmd::CommandResult evaluate(std::string_view script);

  /// `puts stderr $text` (a PCELL-GEN-002 warning with the text).
  void print_error(std::string text);
  /// A netlist_to_layout report line (`puts`, PCELL-NETLIST-001).
  void print(std::string text);

  /// The variable magic::<pname>_val ("" when unset).
  [[nodiscard]] std::string dialog_value(std::string_view pname) const;
  void set_dialog_value(std::string_view pname, const std::string& value);
  [[nodiscard]] bool has_dialog_value(std::string_view pname) const;

  [[nodiscard]] FrameworkState& state() noexcept { return *state_; }
  [[nodiscard]] script::Interpreter& interp() noexcept { return *interp_; }
  [[nodiscard]] cmd::CommandContext& context() noexcept { return *ctx_; }

 private:
  Toolkit(cmd::CommandContext& ctx, script::Interpreter& interp, FrameworkState& state)
      : ctx_(&ctx), interp_(&interp), state_(&state) {}

  cmd::CommandContext* ctx_;
  script::Interpreter* interp_;
  FrameworkState* state_;
};

/// Holds FrameworkState::depth up while a framework procedure runs.
class DepthGuard {
 public:
  explicit DepthGuard(FrameworkState& state) : state_(state) { ++state_.depth; }
  ~DepthGuard() { --state_.depth; }
  DepthGuard(const DepthGuard&) = delete;
  DepthGuard& operator=(const DepthGuard&) = delete;

 private:
  FrameworkState& state_;
};

/// suspendall ... resumeall around a framework procedure.
class Suspended {
 public:
  explicit Suspended(Toolkit& tk) : tk_(tk) { tk_.engine({"suspendall"}); }
  ~Suspended() { tk_.engine({"resumeall"}); }
  Suspended(const Suspended&) = delete;
  Suspended& operator=(const Suspended&) = delete;

 private:
  Toolkit& tk_;
};

/// `set curunits [units]; units internal` ... `units {*}$curunits`.
class InternalUnits {
 public:
  explicit InternalUnits(Toolkit& tk);
  ~InternalUnits();
  InternalUnits(const InternalUnits&) = delete;
  InternalUnits& operator=(const InternalUnits&) = delete;

 private:
  Toolkit& tk_;
  std::vector<std::string> saved_;
};

/// `<library>::<type>` split as toolkit.tcl's regexp {^([^:]+)::([^:]+)$};
/// nullopt when the name has another form.
struct DeviceName {
  std::string library;
  std::string type;
};
[[nodiscard]] std::optional<DeviceName> split_device_name(std::string_view name);

/// The fixed-device array parameters of the selected instance
/// (toolkit.tcl:1686-1694): nx = xhi-xlo+1, ny = yhi-ylo+1 from `array
/// -list count` and pitchx/pitchy from `array -list pitch` (the first
/// selected use's sublist when magic-compat is off, Q-CMDEDIT-63).
void set_array_parameters(Toolkit& tk, ParameterDict& parameters);

/// `dict create {*}$words` (Tcl's error for an odd count).
[[nodiscard]] diag::Outcome<ParameterDict> dict_create(const std::vector<std::string>& words);

}  // namespace hefesto::pcell::detail
