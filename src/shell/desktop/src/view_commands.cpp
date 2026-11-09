// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/view_commands.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/codes.hpp"

namespace hefesto::shell {

namespace {

cmd::CommandResult usage(std::string_view synopsis) {
  return cmd::CommandResult::failure(diag::make(diag::codes::kCmdUsage, "usage: " + std::string(synopsis)));
}

// A finite number filling all of `text`; strtod rather than
// from_chars<double>, which needs macOS 15+ libc++.
std::optional<double> parse_number(const std::string& text) {
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (text.empty() || *end != '\0' || !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

cmd::CommandResult pan(const ViewActions& actions, const cmd::Invocation& inv) {
  const auto dx = inv.args.size() == 2 ? parse_number(inv.args[0]) : std::nullopt;
  const auto dy = inv.args.size() == 2 ? parse_number(inv.args[1]) : std::nullopt;
  if (!dx || !dy || !actions.pan) {
    return usage("pan <dx> <dy>  (framebuffer pixels)");
  }
  actions.pan(*dx, *dy);
  return cmd::CommandResult::success("pan " + inv.args[0] + " " + inv.args[1]);
}

cmd::CommandResult theme(const ViewActions& actions, const cmd::Invocation& inv) {
  const bool valid = inv.args.size() == 1 && (inv.args[0] == "dark" || inv.args[0] == "light");
  if (!valid || !actions.theme) {
    return usage("theme dark|light");
  }
  actions.theme(inv.args[0] == "light" ? ThemeMode::light : ThemeMode::dark);
  return cmd::CommandResult::success("theme " + inv.args[0]);
}

using ViewHandler = cmd::CommandResult (*)(const ViewActions&, const cmd::Invocation&);

cmd::Handler bind(ViewHandler handler, const ViewActions& actions) {
  return [handler, actions](cmd::CommandContext& /*ctx*/, const cmd::Invocation& inv) { return handler(actions, inv); };
}

}  // namespace

bool register_view_commands(cmd::CommandRegistry& registry, const ViewActions& actions) {
  bool ok = !registry.add({"theme", "theme dark|light", "Switches the UI theme.", bind(theme, actions)}).has_value();
  ok = !registry.add({"pan", "pan <dx> <dy>", "Moves the view by framebuffer pixels.", bind(pan, actions)}).has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::shell
