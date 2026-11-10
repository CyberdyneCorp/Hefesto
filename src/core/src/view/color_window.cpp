// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `color` window client (capability ui-color-window). Owner: VIEW.
// Layout, conversion, button and command behavior follow Magic 8.3.694
// cmwind/CMWmain.c, cmwind/CMWcmmnds.c, cmwind/CMWrgbhsv.c and
// cmwind/CMWundo.c, Copyright (C) 1985, 1990 Regents of the University of
// California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md).

#include "hefesto/view/color_window.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "hefesto/cmd/prompt.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::view {

namespace {

constexpr std::size_t kStyleRed = 35;
constexpr std::size_t kStyleGreen = 38;
constexpr std::size_t kStyleBlue = 41;
constexpr std::size_t kStyleYellow = 45;
constexpr std::size_t kStyleGray = 31;
constexpr std::size_t kStyleBrown1 = 48;
constexpr std::size_t kStyleBBox = 15;
constexpr std::size_t kStyleCmEdit = 24;

constexpr std::array<ColorBar, 6> kBars = {{
    {ColorComponent::red, "Red", {2000, 8000, 10000, 9000}, {2000, 9500, 10000, 10500}, kStyleRed},
    {ColorComponent::green, "Green", {2000, 5000, 10000, 6000}, {2000, 6500, 10000, 7500}, kStyleGreen},
    {ColorComponent::blue, "Blue", {2000, 2000, 10000, 3000}, {2000, 3500, 10000, 4500}, kStyleBlue},
    {ColorComponent::hue, "Hue", {14000, 8000, 22000, 9000}, {14000, 9500, 22000, 10500}, kStyleYellow},
    {ColorComponent::saturation, "Saturation", {14000, 5000, 22000, 6000}, {14000, 6500, 22000, 7500}, kStyleGray},
    {ColorComponent::value, "Value", {14000, 2000, 22000, 3000}, {14000, 3500, 22000, 4500}, kStyleBrown1},
}};

constexpr std::array<ColorPump, 12> kPumps = {{
    {ColorComponent::red, {500, 8000, 1500, 9000}, -.0078},
    {ColorComponent::red, {10500, 8000, 11500, 9000}, .0078},
    {ColorComponent::green, {500, 5000, 1500, 6000}, -.0078},
    {ColorComponent::green, {10500, 5000, 11500, 6000}, .0078},
    {ColorComponent::blue, {500, 2000, 1500, 3000}, -.0078},
    {ColorComponent::blue, {10500, 2000, 11500, 3000}, .0078},
    {ColorComponent::hue, {12500, 8000, 13500, 9000}, -.01},
    {ColorComponent::hue, {22500, 8000, 23500, 9000}, .01},
    {ColorComponent::saturation, {12500, 5000, 13500, 6000}, -.01},
    {ColorComponent::saturation, {22500, 5000, 23500, 6000}, .01},
    {ColorComponent::value, {12500, 2000, 13500, 3000}, -.01},
    {ColorComponent::value, {22500, 2000, 23500, 3000}, .01},
}};

display::ColorEntry* entry(editor::EditorState& e, int color) {
  if (!e.styles || color < 0 || static_cast<std::size_t>(color) >= e.styles->colors.entries.size()) {
    return nullptr;
  }
  return &e.styles->colors.entries[static_cast<std::size_t>(color)];
}

struct Rgb8 {
  int r = 0;
  int g = 0;
  int b = 0;
};

Rgb8 get_color(const editor::EditorState& e, int color) {
  if (!e.styles || color < 0 || static_cast<std::size_t>(color) >= e.styles->colors.entries.size()) {
    return {};
  }
  const display::ColorEntry& c = e.styles->colors.entries[static_cast<std::size_t>(color)];
  return Rgb8{c.r, c.g, c.b};
}

/// GrPutColor: stores the intensities and clears the entry's name.
void put_color(editor::EditorState& e, int color, const Rgb8& rgb) {
  if (display::ColorEntry* c = entry(e, color)) {
    c->r = static_cast<std::uint8_t>(rgb.r & 0xFF);
    c->g = static_cast<std::uint8_t>(rgb.g & 0xFF);
    c->b = static_cast<std::uint8_t>(rgb.b & 0xFF);
    c->name.clear();
  }
}

/// The color undo event (cmwind/CMWundo.c): old and new RGB of one entry.
class ColorEvent final : public undo::Event {
 public:
  ColorEvent(int color, Rgb8 before, Rgb8 after) : color_(color), before_(before), after_(after) {}
  void undo(editor::EditorState& editor) override { put_color(editor, color_, before_); }
  void redo(editor::EditorState& editor) override { put_color(editor, color_, after_); }
  [[nodiscard]] std::string describe() const override {
    return "color " + std::to_string(color_) + " " + std::to_string(before_.r) + " " + std::to_string(before_.g) +
           " " + std::to_string(before_.b) + " -> " + std::to_string(after_.r) + " " + std::to_string(after_.g) +
           " " + std::to_string(after_.b);
  }

 private:
  int color_;
  Rgb8 before_;
  Rgb8 after_;
};

double& component(std::array<double, 6>& values, ColorComponent c) noexcept {
  return values[static_cast<std::size_t>(c)];
}

std::array<double, 6> components(const Rgb& rgb) noexcept {
  const Hsv hsv = rgb_to_hsv(rgb);
  return {rgb.r, rgb.g, rgb.b, hsv.h, hsv.s, hsv.v};
}

int to_channel(double v) noexcept {
  return static_cast<int>(v * 255.0 + 0.5);
}

display::HighlightItem scene_item(display::HighlightShape shape, std::size_t style, const db::Rect& r,
                                  std::string text = {}) {
  display::HighlightItem it;
  it.shape = shape;
  it.style = style;
  it.rect = r;
  it.text = std::move(text);
  return it;
}

}  // namespace

std::span<const ColorBar> color_bars() noexcept {
  return kBars;
}

std::span<const ColorPump> color_pumps() noexcept {
  return kPumps;
}

Hsv rgb_to_hsv(const Rgb& c) noexcept {
  Hsv out;
  const double max = std::max({c.r, c.g, c.b});
  const double delta = max - std::min({c.r, c.g, c.b});
  out.v = max;
  if (max == 0.0 || delta == 0.0) {
    return out;  // achromatic: s = 0, h = 0
  }
  out.s = delta / max;
  const double mr = (max - c.r) / delta;
  const double mg = (max - c.g) / delta;
  const double mb = (max - c.b) / delta;
  if (c.r == max) {
    out.h = mb - mg;
  } else if (c.g == max) {
    out.h = 2.0 + mr - mb;
  } else {
    out.h = 4.0 + mg - mr;
  }
  out.h /= 6;
  if (out.h < 0.0) {
    out.h += 1.0;
  }
  return out;
}

Rgb hsv_to_rgb(const Hsv& c) noexcept {
  const double h = c.h * 6;
  const int i = static_cast<int>(h);  // truncation toward zero, as C's int conversion
  const double f = h - i;
  const double vs = c.v * c.s;
  const double vsf = vs * f;
  const double m = c.v - vs;
  const double n = c.v - vsf;
  const double k = c.v - vs + vsf;
  switch (i % 6) {
    case 0:
      return {c.v, k, m};
    case 1:
      return {n, c.v, m};
    case 2:
      return {m, c.v, k};
    case 3:
      return {m, n, c.v};
    case 4:
      return {k, m, c.v};
    case 5:
      return {c.v, m, n};
    default:
      return {};  // i % 6 < 0 (negative hue): Magic leaves r, g, b unset
  }
}

ColorWindow* ColorMapState::find(WindowId id) noexcept {
  const auto it = std::find_if(windows.begin(), windows.end(), [id](const ColorWindow& w) { return w.id == id; });
  return it == windows.end() ? nullptr : &*it;
}

ColorWindow* ColorMapState::active(std::optional<WindowId> pointer) noexcept {
  if (pointer) {
    if (ColorWindow* w = find(*pointer)) {
      return w;
    }
  }
  return windows.empty() ? nullptr : &windows.back();
}

std::string color_caption(int index) {
  char buf[40];
  std::snprintf(buf, sizeof(buf), "COLOR = 0%o", static_cast<unsigned>(index));
  return buf;
}

void load_color(ColorWindow& window, int color) {
  window.color = color;
  window.caption = color_caption(color);
}

diag::Outcome<ColorWindow*> open_color_window(editor::EditorState& editor, std::string_view octal,
                                              const ScreenRect& frame) {
  long color = 0;  // CMWcreate: sscanf("%o"), then & 0377
  if (!octal.empty()) {
    const std::string text(octal);
    char* end = nullptr;
    const long v = std::strtol(text.c_str(), &end, 8);
    color = end == text.c_str() ? 0 : v;
  }
  auto id = editor.windows.reserve_id();
  if (!id) {
    return id.error();
  }
  ColorWindow w;
  w.id = id.value();
  w.frame = frame;
  load_color(w, static_cast<int>(color & 0377));
  editor.color_map.windows.push_back(std::move(w));
  return &editor.color_map.windows.back();
}

ViewMapping color_window_mapping(editor::EditorState& editor, const ColorWindow& window) {
  ChromeFlags flags = editor.windows.chrome_defaults();
  flags.caption = false;
  flags.scrollbars = false;
  return ViewMapping::fit(inner_area(window.frame, flags, editor.windows.chrome_metrics()), kColorWindowSurface);
}

bool close_color_window(editor::EditorState& editor, WindowId id) {
  auto& ws = editor.color_map.windows;
  const auto it = std::find_if(ws.begin(), ws.end(), [id](const ColorWindow& w) { return w.id == id; });
  if (it == ws.end()) {
    return false;
  }
  ws.erase(it);
  editor.windows.release_id(id);
  return true;
}

std::size_t color_count(const editor::EditorState& editor) noexcept {
  return editor.styles ? editor.styles->colors.entries.size() : 0;
}

void update_component(editor::EditorState& editor, const ColorWindow& window, ColorComponent c, double x,
                      bool replace) {
  const Rgb8 old = get_color(editor, window.color);
  std::array<double, 6> v = components(Rgb{old.r / 255.0, old.g / 255.0, old.b / 255.0});
  double& value = component(v, c);
  value = std::clamp(replace ? x : value + x, 0.0, 1.0);
  if (c == ColorComponent::hue || c == ColorComponent::saturation || c == ColorComponent::value) {
    const Rgb rgb = hsv_to_rgb(Hsv{v[3], v[4], v[5]});
    v[0] = rgb.r;
    v[1] = rgb.g;
    v[2] = rgb.b;
  }
  const Rgb8 now{to_channel(v[0]), to_channel(v[1]), to_channel(v[2])};
  put_color(editor, window.color, now);
  editor.color_map.modified = true;
  editor.undo.record(std::make_unique<ColorEvent>(window.color, old, now));
}

bool press_button(editor::EditorState& editor, const ColorWindow& window, db::Point p, Button button) {
  for (const ColorBar& bar : kBars) {
    if (db::encloses(bar.bar, p)) {
      const db::Coord x = std::clamp(p.x, bar.bar.x0, bar.bar.x1);
      const double dx = static_cast<double>(x - bar.bar.x0) / static_cast<double>(bar.bar.x1 - bar.bar.x0);
      update_component(editor, window, bar.component, dx, true);
      return true;
    }
  }
  for (const ColorPump& pump : kPumps) {
    if (db::encloses(pump.rect, p)) {
      update_component(editor, window, pump.component, button == Button::left ? pump.amount : 5 * pump.amount,
                       false);
      return true;
    }
  }
  return false;  // Q-UI-37: no pick from the swatch (the Tcl build has none)
}

bool check_color_map_written(editor::EditorState& editor) {
  if (!editor.color_map.modified) {
    return true;
  }
  const std::array<std::string, 2> answers{"no", "yes"};
  return cmd::ask(editor.prompt, "The color map has been modified.\n  Do you want to lose the changes? ", answers,
                  0) == 1;
}

display::HighlightScene color_window_scene(const editor::EditorState& editor, const ColorWindow& window) {
  using display::HighlightShape;
  display::HighlightScene scene;
  const Rgb8 c = get_color(editor, window.color);
  const std::array<double, 6> v = components(Rgb{(c.r + 0.5) / 255.0, (c.g + 0.5) / 255.0, (c.b + 0.5) / 255.0});
  for (const ColorBar& bar : kBars) {
    const double x = v[static_cast<std::size_t>(bar.component)];
    const db::Rect& r = bar.bar;
    const db::Coord top = r.x0 + static_cast<db::Coord>(x * static_cast<double>(r.x1 - r.x0));
    scene.items.push_back(scene_item(HighlightShape::box, bar.style, db::Rect{r.x0, r.y0, top, r.y1}));
    scene.items.push_back(scene_item(HighlightShape::outline, kStyleBBox, r));
    scene.items.push_back(scene_item(HighlightShape::text, kStyleBBox, bar.name_rect, std::string(bar.name)));
  }
  for (const ColorPump& pump : kPumps) {
    scene.items.push_back(scene_item(HighlightShape::outline, kStyleBBox, pump.rect));
    scene.items.push_back(scene_item(HighlightShape::text, kStyleBBox, pump.rect, pump.amount < 0 ? "-" : "+"));
  }
  scene.items.push_back(scene_item(HighlightShape::box, kStyleCmEdit, kCurrentColorArea));
  scene.items.push_back(scene_item(HighlightShape::outline, kStyleBBox, kCurrentColorArea));
  scene.items.push_back(
      scene_item(HighlightShape::text, kStyleBBox, kCurrentColorTextBox, std::string(kCurrentColorText)));
  const display::StyleSet* styles = editor.styles ? &*editor.styles : nullptr;
  for (display::HighlightItem& it : scene.items) {
    display::resolve_highlight(it, styles);
    if (it.style == kStyleCmEdit) {  // STYLE_CMEDIT takes the edited color
      it.resolved.r = static_cast<float>(c.r) / 255.0F;
      it.resolved.g = static_cast<float>(c.g) / 255.0F;
      it.resolved.b = static_cast<float>(c.b) / 255.0F;
      it.resolved.a = 1.0F;
      it.resolved.fill = display::FillKind::solid;
    }
  }
  return scene;
}

}  // namespace hefesto::view
