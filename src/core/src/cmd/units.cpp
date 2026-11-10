// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `units` command (cmd-coordinates-and-units "Units command";
// Q-CMDEDIT-6). Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdTZ.c
// (CmdUnits), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md). Owner: EDIT.

#include "hefesto/cmd/units.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::cmd {

namespace {

enum Keyword : std::uint8_t { k_default, k_internal, k_lambda, k_grid, k_user, k_microns, k_list, k_print, k_noprint };

constexpr std::array<std::string_view, 9> kNames = {"default", "internal", "lambda", "grid",   "user",
                                                    "microns", "list",     "print",  "noprint"};

constexpr std::string_view kUsage = "Usage: units [default | internal | lambda | microns | user] [print]";

std::optional<Keyword> keyword(std::string_view word) {
  const LookupResult r = lookup(word, kNames);
  if (r.match != Match::found) {
    return std::nullopt;
  }
  return static_cast<Keyword>(r.index);
}

InputUnit unit_of(Keyword k) noexcept {
  switch (k) {
    case k_internal:
      return InputUnit::internal;
    case k_lambda:
      return InputUnit::lambda;
    case k_grid:
    case k_user:
      return InputUnit::user;
    case k_microns:
      return InputUnit::microns;
    default:
      return InputUnit::default_units;
  }
}

void report(CommandContext& ctx, std::string text) {
  diag::Diagnostic d = diag::make(diag::codes::kCmdUsage, std::move(text));
  d.severity = diag::Severity::info;
  ctx.emit(d);
}

Value listing(const UnitsState& u) {
  Value::List out{std::string{units_name(u.unit)}};
  if (u.unit != InputUnit::default_units) {
    out.emplace_back(u.print ? "print" : "noprint");
  }
  return Value{std::move(out)};
}

// `units print` / `units noprint` alone (Q-CMDEDIT-6).
CommandResult set_print_flag(UnitsState& u, bool print, bool magic_compat) {
  if (u.unit == InputUnit::default_units) {
    if (!magic_compat) {
      return CommandResult::failure(diag::make(diag::codes::kCmdUsage, "set a unit type first"));
    }
    u.unit = InputUnit::microns;  // Magic's type field reads as microns
  }
  u.print = print;
  return CommandResult::success();
}

// `units T [print|noprint]`.
CommandResult set_type(UnitsState& u, Keyword k, const std::vector<std::string>& args) {
  const bool keep_flag = u.unit != InputUnit::default_units;
  u.unit = unit_of(k);
  if (u.unit == InputUnit::default_units) {
    u.print = false;
    return CommandResult::success();
  }
  if (args.size() != 2) {
    u.print = keep_flag && u.print;
    return CommandResult::success();
  }
  const auto flag = keyword(args[1]);
  if (flag == k_print || flag == k_noprint) {
    u.print = flag == k_print;
    return CommandResult::success();
  }
  u.print = false;  // Magic assigned the bare type before reading the flag
  return CommandResult::failure(diag::make(diag::codes::kCmdUsage, "Unrecognized units option " + args[1]));
}

CommandResult cmd_units(CommandContext& ctx, const Invocation& inv) {
  auto ed = require_editor(ctx);
  if (!ed.ok()) {
    return CommandResult::failure(ed.error());
  }
  UnitsState& u = ed.value()->units;
  if (inv.args.empty()) {
    return CommandResult::success(listing(u));
  }
  const auto k = keyword(inv.args[0]);
  if (!k) {
    report(ctx, std::string{kUsage});
    return CommandResult::success();
  }
  if (*k == k_list) {
    return CommandResult::success(listing(u));
  }
  if (*k == k_print || *k == k_noprint) {
    return set_print_flag(u, *k == k_print, ctx.project().compat.magic_compat);
  }
  return set_type(u, *k, inv.args);
}

}  // namespace

std::string_view units_name(InputUnit unit) noexcept {
  switch (unit) {
    case InputUnit::internal:
      return "internal";
    case InputUnit::lambda:
      return "lambda";
    case InputUnit::user:
      return "user";
    case InputUnit::microns:
      return "microns";
    case InputUnit::default_units:
      break;
  }
  return "default";
}

bool register_units_commands(CommandRegistry& registry) {
  return !registry.add(CommandSpec{"units", "units ?default|internal|lambda|user|microns|list? ?print|noprint?",
                                   "Shows or sets the units of typed and reported values.", cmd_units});
}

}  // namespace hefesto::cmd
