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
#include "hefesto/editor/editor_state.hpp"

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

std::optional<PanelId> panel_id(const std::string& word) {
  if (word == "layers") return PanelId::layers;
  if (word == "inspector") return PanelId::inspector;
  if (word == "console") return PanelId::console;
  if (word == "keys") return PanelId::keys;
  if (word == "drc") return PanelId::drc;
  return std::nullopt;
}

constexpr std::string_view kPanelUsage =
    "panel layers|inspector|console|keys|drc ?show|hide|toggle? | panel layers|inspector dock left|right | panel reset";

cmd::CommandResult dock_panel(const ViewActions& actions, PanelId id, const cmd::Invocation& inv) {
  const bool valid = inv.args.size() == 3 && (inv.args[2] == "left" || inv.args[2] == "right");
  if (!valid || !actions.dock) return usage(kPanelUsage);
  if (!actions.dock(id, inv.args[2] == "left" ? DockSide::left : DockSide::right)) {
    return cmd::CommandResult::failure(
        diag::make(diag::codes::kCmdUsage, "panel " + inv.args[0] + " cannot be docked (layers and inspector can)"));
  }
  return cmd::CommandResult::success("docked " + inv.args[2]);
}

cmd::CommandResult panel(const ViewActions& actions, const cmd::Invocation& inv) {
  constexpr std::string_view kUsage = kPanelUsage;
  if (inv.args.size() == 1 && inv.args[0] == "reset" && actions.reset) {
    actions.reset();
    return cmd::CommandResult::success("default layout");
  }
  const auto id = !inv.args.empty() && inv.args.size() <= 3 ? panel_id(inv.args[0]) : std::nullopt;
  if (!id || !actions.panel || !actions.panel_shown) return usage(kUsage);
  if (inv.args.size() >= 2 && inv.args[1] == "dock") return dock_panel(actions, *id, inv);
  if (inv.args.size() == 3) return usage(kUsage);
  if (inv.args.size() == 1) return cmd::CommandResult::success(actions.panel_shown(*id) ? "shown" : "hidden");
  const std::string& action = inv.args[1];
  std::optional<bool> show;
  if (action == "show") {
    show = true;
  } else if (action == "hide") {
    show = false;
  } else if (action != "toggle") {
    return usage(kUsage);
  }
  return cmd::CommandResult::success(actions.panel(*id, show) ? "shown" : "hidden");
}

cmd::CommandResult palette(const ViewActions& actions, const cmd::Invocation& inv) {
  constexpr std::string_view kUsage = "palette paint ?layer|none?";
  if (inv.args.empty() || inv.args.size() > 2 || inv.args[0] != "paint" || !actions.paint_layer) return usage(kUsage);
  std::optional<std::string> wanted;
  if (inv.args.size() == 2) wanted = inv.args[1] == "none" ? std::string() : inv.args[1];
  const auto now = actions.paint_layer(wanted);
  if (!now) {
    return cmd::CommandResult::failure(
        diag::make(diag::codes::kCmdUsage, "palette paint: unknown layer \"" + inv.args[1] + "\""));
  }
  return cmd::CommandResult::success(now->empty() ? std::string("none") : *now);
}

// The `notifications` setting in the session layer; "on" when unset.
cmd::CommandResult notifications(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  constexpr std::string_view kUsage = "hefesto::notifications ?on|off?";
  editor::EditorState* editor = ctx.editor();
  const bool valid = inv.args.empty() || (inv.args.size() == 1 && (inv.args[0] == "on" || inv.args[0] == "off"));
  if (!valid || editor == nullptr) return usage(kUsage);
  if (inv.args.size() == 1) editor->settings.set(app::SettingLayer::session, "notifications", inv.args[0]);
  const auto value = editor->settings.effective("notifications");
  return cmd::CommandResult::success(value && value->value == "off" ? "off" : "on");
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
  ok = !registry
            .add({"hefesto::notifications", "hefesto::notifications ?on|off?",
                  "Turns the job-completion notifications (a long DRC run finished while the window was not "
                  "focused) on or off, or reports them.",
                  notifications})
            .has_value() &&
       ok;
  if (actions.panel) {
    ok = !registry
              .add({"panel", std::string(kPanelUsage),
                    "Shows, hides (collapses), toggles or docks a panel of the main window.", bind(panel, actions)})
              .has_value() &&
         ok;
  }
  if (actions.paint_layer) {
    ok = !registry
              .add({"palette", "palette paint ?layer|none?",
                    "Sets or reports the layer palette's paint layer (Paint tool, Paint / Erase on the box).",
                    bind(palette, actions)})
              .has_value() &&
         ok;
  }
  return ok;
}

}  // namespace hefesto::shell
