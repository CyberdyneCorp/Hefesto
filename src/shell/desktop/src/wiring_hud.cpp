// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wiring HUD, mode chip and Wire inspector (wiring_hud.hpp). Owner: CARRY.

#include "hefesto/shell/wiring_hud.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace hefesto::shell {

namespace {

constexpr std::string_view kDot = " · ";

std::string type_name(const editor::EditorState& editor, tech::TileType type) {
  return editor.technology ? std::string(editor.technology->type_name(type)) : std::string();
}

db::Coord leg_length(const wiring::LegPlan& leg) {
  const bool horizontal = leg.direction == wiring::LegDirection::east || leg.direction == wiring::LegDirection::west;
  return horizontal ? leg.area.x1 - leg.area.x0 : leg.area.y1 - leg.area.y0;
}

}  // namespace

std::string_view leg_direction_name(wiring::LegDirection direction) noexcept {
  switch (direction) {
    case wiring::LegDirection::north: return "north";
    case wiring::LegDirection::south: return "south";
    case wiring::LegDirection::east: return "east";
    case wiring::LegDirection::west: return "west";
    case wiring::LegDirection::none: break;
  }
  return {};
}

// --- HUD ----------------------------------------------------------------------------

std::optional<std::string> wiring_hud_text(const editor::EditorState& editor, const db::Rect& box,
                                           const db::Point& pointer, const UnitScale& scale) {
  const wiring::WireState& w = editor.wire;
  if (!w.tracking || !w.type || !editor.technology) return std::nullopt;
  std::string text = type_name(editor, *w.type) + std::string(kDot) +
                     format_length(static_cast<double>(w.width), scale);
  const auto leg = wiring::plan_leg(box, pointer, w.width, wiring::LegMode::choose);
  if (leg && leg->direction != wiring::LegDirection::none) {
    text += std::string(kDot) + std::string(leg_direction_name(leg->direction)) + std::string(kDot) +
            format_length(static_cast<double>(leg_length(*leg)), scale);
  }
  return text;
}

HudPoint wiring_hud_point(double cursor_x, double cursor_y, double hud_w, double hud_h, double canvas_w,
                          double canvas_h) {
  constexpr double kDx = 14.0;
  constexpr double kDy = 20.0;
  double x = cursor_x + kDx;
  if (x + hud_w > canvas_w) x = cursor_x - kDx - hud_w;  // flip to the left near the edge
  double y = cursor_y + kDy;
  if (y + hud_h > canvas_h) y = cursor_y - kDy - hud_h;  // and above near the bottom
  return HudPoint{std::max(0.0, x), std::max(0.0, y)};
}

// --- mode chip ----------------------------------------------------------------------

std::optional<WiringChip> wiring_chip(const editor::EditorState& editor) {
  if (!editor.wire.tracking) return std::nullopt;
  return WiringChip{"Wiring" + std::string(kDot) + "tracking", "Wiring tool tracking; Escape cancels",
                    "trackwire cancel"};
}

// --- Wire inspector -----------------------------------------------------------------

std::vector<std::string> wiring_stack(const wiring::WiringSection& section, const tech::Technology& tech) {
  std::vector<std::string> stack;
  const auto add = [&](tech::TileType type) {
    std::string name(tech.type_name(type));
    if (std::find(stack.begin(), stack.end(), name) == stack.end()) stack.push_back(std::move(name));
  };
  // `contacts` is in reverse file order (the search order).
  for (auto it = section.contacts.rbegin(); it != section.contacts.rend(); ++it) {
    add(it->layer1);
    add(it->layer2);
  }
  return stack;
}

WireInspectorView wire_inspector(const editor::EditorState& editor, const wiring::WiringSection& section,
                                 const UnitScale& scale) {
  WireInspectorView v;
  const wiring::WireState& w = editor.wire;
  v.type = w.type ? type_name(editor, *w.type) : std::string("no material");
  v.width = format_length(static_cast<double>(w.width), scale);
  v.units = std::string(scale.unit_name());
  v.width_command = "wire width ";
  v.actions = {{"Width −", "wire decrement width"},
               {"Width +", "wire increment width"},
               {"Switch layer", "wire switch"}};
  if (!editor.technology) return v;
  for (const std::string& name : wiring_stack(section, *editor.technology)) {
    v.stack.push_back(WireStackRow{name, name == v.type, "wire type " + name});
  }
  return v;
}

}  // namespace hefesto::shell
