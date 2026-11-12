// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The panel layout model (panels_layout.hpp). Owner: CARRY.

#include "hefesto/shell/panels_layout.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>

#include <nlohmann/json.hpp>


namespace hefesto::shell {

namespace {

constexpr std::array<std::string_view, 4> kZoneNames{"left", "right", "bottom", "floating"};

std::size_t zone_index(DockZone zone) noexcept { return static_cast<std::size_t>(zone); }

PanelPlacement placement(std::string id, DockZone dock, bool collapsed) {
  return PanelPlacement{std::move(id), dock, dock, collapsed, LayoutRect{}};
}

std::optional<float> parse_float(std::string_view word) {
  int value = 0;
  const auto [ptr, ec] = std::from_chars(word.data(), word.data() + word.size(), value);
  if (ec != std::errc{} || ptr != word.data() + word.size()) return std::nullopt;
  return static_cast<float>(value);
}

PanelCommandResult usage() { return PanelCommandResult{false, std::string(panel_command_usage()), false}; }

PanelCommandResult shown_result(const PanelPlacement& p, bool changed) {
  return PanelCommandResult{true, p.collapsed ? "hidden" : "shown", changed};
}

/// Moves `id` to the end of `zone`'s panels (the new tab of a dock).
void move_to_end(PanelLayout& layout, std::string_view id, DockZone zone) {
  auto it = std::find_if(layout.panels.begin(), layout.panels.end(), [&](const auto& p) { return p.id == id; });
  PanelPlacement moved = std::move(*it);
  layout.panels.erase(it);
  moved.dock = zone;
  if (zone != DockZone::floating) moved.home = zone;
  layout.panels.push_back(std::move(moved));
}

PanelCommandResult set_shown(PanelLayout& layout, PanelPlacement& p, std::string_view action) {
  bool show = p.collapsed;  // toggle
  if (action == "show") {
    show = true;
  } else if (action == "hide") {
    show = false;
  } else if (action != "toggle") {
    return usage();
  }
  const bool changed = p.collapsed == show;
  p.collapsed = !show;
  if (show && p.dock != DockZone::floating) layout.strip[zone_index(p.dock)] = false;  // opens the strip
  return shown_result(p, changed);
}

PanelCommandResult dock(PanelLayout& layout, const PanelPlacement& p, std::string_view word) {
  const auto zone = parse_dock_zone(word);
  if (!zone || *zone == DockZone::floating) return usage();
  const bool changed = p.dock != *zone;
  const std::string id = p.id;
  if (changed) move_to_end(layout, id, *zone);
  layout.strip[zone_index(*zone)] = false;
  return PanelCommandResult{true, "docked " + std::string(word), changed};
}

PanelCommandResult float_panel(PanelLayout& layout, const PanelPlacement& p, const std::vector<std::string>& args,
                               const LayoutRect& window) {
  LayoutRect r = p.floating;
  if (args.size() == 6) {
    std::array<float, 4> v{};
    for (std::size_t i = 0; i < v.size(); ++i) {
      const auto parsed = parse_float(args[i + 2]);
      if (!parsed) return usage();
      v[i] = *parsed;
    }
    r = LayoutRect{v[0], v[1], v[2], v[3]};
  } else if (args.size() != 2) {
    return usage();
  } else if (!(r.w > 0.0F) || !(r.h > 0.0F)) {
    r = LayoutRect{window.x + (window.w - 320.0F) / 2.0F, window.y + (window.h - 400.0F) / 2.0F, 320.0F, 400.0F};
  }
  const std::string id = p.id;
  const bool was_floating = p.dock == DockZone::floating;
  if (!was_floating) move_to_end(layout, id, DockZone::floating);
  PanelPlacement* moved = layout.find(id);
  const LayoutRect clamped = clamp_floating(r, window);
  const bool changed = !was_floating || !(moved->floating == clamped);
  moved->floating = clamped;
  moved->collapsed = false;
  return PanelCommandResult{true, "floating", changed};
}

LayoutRect drop_zone(const LayoutRect& dock, const LayoutRect& canvas, DockZone zone) {
  if (dock.w > 0.0F && dock.h > 0.0F) return dock;
  // An empty dock: the band at the canvas edge it would take.
  switch (zone) {
    case DockZone::left: return LayoutRect{canvas.x, canvas.y, kDockBand, canvas.h};
    case DockZone::right: return LayoutRect{canvas.x + canvas.w - kDockBand, canvas.y, kDockBand, canvas.h};
    case DockZone::bottom: return LayoutRect{canvas.x, canvas.y + canvas.h - kDockBand, canvas.w, kDockBand};
    case DockZone::floating: break;
  }
  return dock;
}

/// The tabs of `leader`'s strip take its dock, home and floating window,
/// right after it in the dock order.
void follow_leader(PanelLayout& layout, const std::string& leader) {
  std::vector<PanelPlacement> members;
  std::erase_if(layout.panels, [&](PanelPlacement& p) {
    if (p.id == leader || panel_group_leader(p.id) != leader) return false;
    members.push_back(std::move(p));
    return true;
  });
  auto at = std::find_if(layout.panels.begin(), layout.panels.end(), [&](const auto& p) { return p.id == leader; });
  if (at == layout.panels.end()) return;
  for (PanelPlacement& m : members) {
    m.dock = at->dock;
    m.home = at->home;
    m.floating = at->floating;
  }
  layout.panels.insert(at + 1, std::make_move_iterator(members.begin()), std::make_move_iterator(members.end()));
}

int rounded(float v) { return static_cast<int>(std::lround(v)); }

nlohmann::json rect_json(const LayoutRect& r) { return nlohmann::json::array({r.x, r.y, r.w, r.h}); }

std::optional<LayoutRect> rect_from(const nlohmann::json& j) {
  if (!j.is_array() || j.size() != 4) return std::nullopt;
  for (const auto& v : j) {
    if (!v.is_number()) return std::nullopt;
  }
  return LayoutRect{j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

std::optional<PanelPlacement> placement_from(const nlohmann::json& j) {
  if (!j.is_object() || !j.contains("id") || !j["id"].is_string()) return std::nullopt;
  PanelPlacement p;
  p.id = j["id"].get<std::string>();
  const auto zone = [&](const char* key) -> std::optional<DockZone> {
    return j.contains(key) && j[key].is_string() ? parse_dock_zone(j[key].get<std::string>()) : std::nullopt;
  };
  const auto dock_zone = zone("dock");
  const auto home = zone("home");
  if (!dock_zone || !home || *home == DockZone::floating) return std::nullopt;
  p.dock = *dock_zone;
  p.home = *home;
  p.collapsed = j.value("collapsed", false);
  if (j.contains("floating")) {
    const auto r = rect_from(j["floating"]);
    if (!r) return std::nullopt;
    p.floating = *r;
  }
  return p;
}

bool read_docks(const nlohmann::json& docks, PanelLayout& layout) {
  if (!docks.is_object()) return false;
  const auto size = [&](const char* key, float& out) {
    if (!docks.contains(key)) return true;
    if (!docks[key].is_number()) return false;
    out = std::max(kMinDockSize, docks[key].get<float>());
    return true;
  };
  if (!size("left", layout.left_width) || !size("right", layout.right_width) ||
      !size("bottom", layout.bottom_height)) {
    return false;
  }
  if (docks.contains("strips")) {
    if (!docks["strips"].is_array()) return false;
    for (const auto& s : docks["strips"]) {
      const auto zone = s.is_string() ? parse_dock_zone(s.get<std::string>()) : std::nullopt;
      if (!zone || *zone == DockZone::floating) return false;
      layout.strip[zone_index(*zone)] = true;
    }
  }
  return true;
}

/// Adds the panels of `defaults` that `layout` lacks after the default
/// panel before them (or first in their dock).
void add_missing(PanelLayout& layout, const PanelLayout& defaults) {
  for (std::size_t i = 0; i < defaults.panels.size(); ++i) {
    const PanelPlacement& d = defaults.panels[i];
    if (layout.find(d.id) != nullptr) continue;
    auto at = layout.panels.begin();
    if (i > 0) {
      const std::string& before = defaults.panels[i - 1].id;
      at = std::find_if(layout.panels.begin(), layout.panels.end(), [&](const auto& p) { return p.id == before; });
      at = at == layout.panels.end() ? layout.panels.end() : at + 1;
    }
    layout.panels.insert(at, d);
  }
}

}  // namespace

std::string_view dock_zone_name(DockZone zone) noexcept { return kZoneNames[zone_index(zone)]; }

std::optional<DockZone> parse_dock_zone(std::string_view word) noexcept {
  for (std::size_t i = 0; i < kZoneNames.size(); ++i) {
    if (kZoneNames[i] == word) return static_cast<DockZone>(i);
  }
  return std::nullopt;
}

const PanelPlacement* PanelLayout::find(std::string_view id) const noexcept {
  const auto it = std::find_if(panels.begin(), panels.end(), [&](const auto& p) { return p.id == id; });
  return it == panels.end() ? nullptr : &*it;
}

PanelPlacement* PanelLayout::find(std::string_view id) noexcept {
  const auto it = std::find_if(panels.begin(), panels.end(), [&](const auto& p) { return p.id == id; });
  return it == panels.end() ? nullptr : &*it;
}

std::vector<std::string> PanelLayout::in(DockZone zone) const {
  std::vector<std::string> out;
  for (const PanelPlacement& p : panels) {
    if (p.dock == zone) out.push_back(p.id);
  }
  return out;
}

PanelLayout default_panel_layout() {
  PanelLayout layout;
  layout.panels = {placement("layers", DockZone::left, false),        placement("cells", DockZone::left, true),
                   placement("libraries", DockZone::left, true),     placement("drc", DockZone::left, true),
                   placement("inspector", DockZone::right, false),   placement("techmanager", DockZone::right, true),
                   placement("console", DockZone::bottom, true),     placement("keys", DockZone::bottom, true)};
  return layout;
}

std::string_view panel_group_leader(std::string_view id) noexcept {
  if (id == "cells" || id == "libraries") return "layers";
  if (id == "keys") return "console";
  return id;
}

std::string_view panel_command_usage() noexcept {
  return "panel NAME ?show|hide|toggle? | panel NAME dock left|right|bottom | panel NAME float ?X Y W H? | "
         "panel reset";
}

PanelCommandResult run_panel_command(PanelLayout& layout, const std::vector<std::string>& args,
                                     const LayoutRect& window) {
  if (args.size() == 1 && args[0] == "reset") {
    const bool changed = !(layout == default_panel_layout());
    layout = default_panel_layout();
    return PanelCommandResult{true, "default layout", changed};
  }
  PanelPlacement* p = args.empty() ? nullptr : layout.find(args[0]);
  if (p == nullptr) return usage();
  if (args.size() == 1) return shown_result(*p, false);
  const std::string id = p->id;
  PanelPlacement* leader = layout.find(panel_group_leader(id));
  if (leader == nullptr) leader = p;
  const std::string leader_id = leader->id;  // dock() and float_panel() move the placements
  PanelCommandResult result;
  if (args[1] == "dock") {
    result = args.size() == 3 ? dock(layout, *leader, args[2]) : usage();
  } else if (args[1] == "float") {
    result = float_panel(layout, *leader, args, window);
  } else {
    if (args.size() != 2) return usage();
    result = set_shown(layout, *p, args[1]);
    // Showing a tab of a strip shows the strip.
    if (result.ok && !layout.find(id)->collapsed && leader != p && leader->collapsed) {
      leader->collapsed = false;
      result.changed = true;
    }
    return result;
  }
  if (result.ok) follow_leader(layout, leader_id);
  return result;
}

LayoutRect clamp_floating(const LayoutRect& r, const LayoutRect& window) noexcept {
  const float w = std::clamp(r.w, std::min(kMinFloatingSize, window.w), window.w);
  const float h = std::clamp(r.h, std::min(kMinFloatingSize, window.h), window.h);
  const float x = std::clamp(r.x, window.x, window.x + window.w - w);
  const float y = std::clamp(r.y, window.y, window.y + window.h - h);
  return LayoutRect{x, y, w, h};
}

// --- narrow windows and edge strips --------------------------------------------------

float canvas_width(const PanelLayout& layout, float width) {
  const auto side = [&](DockZone zone, float size) {
    if (layout.in(zone).empty()) return 0.0F;
    return layout.strip[zone_index(zone)] ? kEdgeStripWidth : size;
  };
  return width - side(DockZone::left, layout.left_width) - side(DockZone::right, layout.right_width);
}

PanelLayout fit_to_window(PanelLayout layout, float width) {
  for (const DockZone zone : {DockZone::right, DockZone::left}) {
    if (canvas_width(layout, width) >= kMinCanvasWidth) break;
    if (!layout.in(zone).empty()) layout.strip[zone_index(zone)] = true;
  }
  return layout;
}

std::vector<EdgeStrip> edge_strips(const PanelLayout& layout) {
  std::vector<EdgeStrip> out;
  for (const DockZone zone : {DockZone::left, DockZone::right, DockZone::bottom}) {
    std::vector<std::string> panels = layout.in(zone);
    if (layout.strip[zone_index(zone)] && !panels.empty()) out.push_back(EdgeStrip{zone, std::move(panels)});
  }
  return out;
}

// --- panel headers ------------------------------------------------------------------------

PanelHeader panel_header(const PanelLayout& layout, std::string_view id, std::string_view title,
                         std::optional<std::size_t> count) {
  PanelHeader h;
  h.title = std::string(title);
  if (count) h.count = std::to_string(*count);
  const PanelPlacement* p = layout.find(id);
  if (p == nullptr) return h;
  const std::string name(id);
  h.floating = p->dock == DockZone::floating;
  const char* state = h.floating ? "floating" : (p->collapsed ? "collapsed" : "expanded");
  h.accessible_name = h.title + ", " + state;
  if (!h.floating) {
    h.buttons.push_back(p->collapsed
                            ? PanelHeaderButton{"chevron-right", "Expand " + h.title, "panel " + name + " show"}
                            : PanelHeaderButton{"chevron-down", "Collapse " + h.title, "panel " + name + " hide"});
    h.buttons.push_back({"float", "Float " + h.title, "panel " + name + " float"});
  } else {
    h.buttons.push_back(
        {"dock", "Dock " + h.title, "panel " + name + " dock " + std::string(dock_zone_name(p->home))});
  }
  return h;
}

// --- drag to dock -----------------------------------------------------------------------

PanelDrag::PanelDrag(std::string id, float press_x, float press_y, const LayoutRect& panel)
    : id_(std::move(id)), grab_x_(press_x - panel.x), grab_y_(press_y - panel.y), panel_(panel) {}

const DropTarget& PanelDrag::move(float x, float y, const DockGeometry& g) {
  const LayoutRect left = drop_zone(g.left, g.canvas, DockZone::left);
  const LayoutRect right = drop_zone(g.right, g.canvas, DockZone::right);
  const LayoutRect bottom = drop_zone(g.bottom, g.canvas, DockZone::bottom);
  if (left.contains(x, y)) {
    target_ = DropTarget{DockZone::left, left};
  } else if (right.contains(x, y)) {
    target_ = DropTarget{DockZone::right, right};
  } else if (bottom.contains(x, y)) {
    target_ = DropTarget{DockZone::bottom, bottom};
  } else {
    const LayoutRect r{x - grab_x_, y - grab_y_, panel_.w, panel_.h};
    target_ = DropTarget{DockZone::floating, clamp_floating(r, g.window)};
  }
  return *target_;
}

std::optional<std::string> PanelDrag::drop_command() const {
  if (!target_) return std::nullopt;
  if (target_->zone != DockZone::floating) {
    return "panel " + id_ + " dock " + std::string(dock_zone_name(target_->zone));
  }
  const LayoutRect& r = target_->highlight;
  return "panel " + id_ + " float " + std::to_string(rounded(r.x)) + " " + std::to_string(rounded(r.y)) + " " +
         std::to_string(rounded(r.w)) + " " + std::to_string(rounded(r.h));
}

LayoutRect drop_band_rect(const DropTarget& target, const PanelLayout& layout) {
  LayoutRect r = target.highlight;
  switch (target.zone) {
    case DockZone::left:
      r.w = std::max(r.w, layout.left_width);
      break;
    case DockZone::right: {
      const float w = std::max(r.w, layout.right_width);
      r.x += r.w - w;
      r.w = w;
      break;
    }
    case DockZone::bottom: {
      const float h = std::max(r.h, layout.bottom_height);
      r.y += r.h - h;
      r.h = h;
      break;
    }
    case DockZone::floating: break;
  }
  return r;
}

DropBandText drop_band_text(const DropTarget& target, const PanelPlacement& panel, std::string_view title) {
  if (target.zone == DockZone::floating) return {};
  const std::string where =
      target.zone == DockZone::bottom ? std::string("at the bottom") : "on the " + std::string(dock_zone_name(target.zone));
  DropBandText text{"Drop to dock " + where, {}};
  if (panel.home == target.zone && panel.dock != target.zone) text.note = std::string(title) + " was undocked from here";
  return text;
}

// --- saved per user -------------------------------------------------------------------------

std::string panel_layout_json(const PanelLayout& layout) {
  nlohmann::json doc;
  doc["version"] = 1;
  nlohmann::json docks;
  docks["left"] = layout.left_width;
  docks["right"] = layout.right_width;
  docks["bottom"] = layout.bottom_height;
  docks["strips"] = nlohmann::json::array();
  for (const DockZone zone : {DockZone::left, DockZone::right, DockZone::bottom}) {
    if (layout.strip[zone_index(zone)]) docks["strips"].push_back(std::string(dock_zone_name(zone)));
  }
  doc["docks"] = std::move(docks);
  doc["panels"] = nlohmann::json::array();
  for (const PanelPlacement& p : layout.panels) {
    nlohmann::json j;
    j["id"] = p.id;
    j["dock"] = std::string(dock_zone_name(p.dock));
    j["home"] = std::string(dock_zone_name(p.home));
    j["collapsed"] = p.collapsed;
    if (p.floating.w > 0.0F && p.floating.h > 0.0F) j["floating"] = rect_json(p.floating);
    doc["panels"].push_back(std::move(j));
  }
  return doc.dump(2) + "\n";
}

std::optional<PanelLayout> parse_panel_layout(std::string_view text, const PanelLayout& defaults) {
  const auto doc = nlohmann::json::parse(text, nullptr, false);
  if (doc.is_discarded() || !doc.is_object() || doc.value("version", 0) != 1 || !doc.contains("panels") ||
      !doc["panels"].is_array()) {
    return std::nullopt;
  }
  PanelLayout layout;
  if (doc.contains("docks") && !read_docks(doc["docks"], layout)) return std::nullopt;
  for (const auto& item : doc["panels"]) {
    auto p = placement_from(item);
    if (!p) return std::nullopt;
    if (defaults.find(p->id) != nullptr && layout.find(p->id) == nullptr) layout.panels.push_back(std::move(*p));
  }
  add_missing(layout, defaults);
  return layout;
}

RestoredLayout restore_panel_layout(platform::FileSystem& fs, const platform::Path& file, const PanelLayout& defaults) {
  RestoredLayout r{defaults, false, {}};
  if (!fs.exists(file)) return r;
  auto text = fs.read(file);
  if (!text) {
    r.warning = "cannot read the panel layout " + file.string() + ": " + text.error().text + "; using the default";
    return r;
  }
  auto parsed = parse_panel_layout(text.value(), defaults);
  if (!parsed) {
    r.warning = file.string() + " is not a panel layout; using the default";
    return r;
  }
  r.layout = std::move(*parsed);
  r.from_file = true;
  return r;
}

diag::Status save_panel_layout(platform::FileSystem& fs, const platform::Path& file, const PanelLayout& layout) {
  if (!file.parent_path().empty()) {
    if (auto made = fs.create_directories(file.parent_path()); !made) return made;
  }
  return fs.write_atomic(file, panel_layout_json(layout));
}

}  // namespace hefesto::shell
