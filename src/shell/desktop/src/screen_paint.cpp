// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "screen_paint.hpp"

#include <algorithm>
#include <cmath>

namespace hefesto::shell::paint {

namespace {

constexpr float kRadius = static_cast<float>(kRadiusControl);
constexpr float kPanelRadius = static_cast<float>(kRadiusPanel);
constexpr float kDisabledAlpha = 0.45F;

Rgba alpha(const Rgba& c, float a) {
  return Rgba{c.r, c.g, c.b, c.a * a};
}

/// Selection and the box are white in both themes ("Layer colors come from
/// the PDK, not the theme": the canvas UI stays neutral over PDK colors).
Rgba white(float a = 1.0F) {
  return Rgba{1.0F, 1.0F, 1.0F, a};
}

void focus_ring(ui::Painter& p, float w, float h, float radius, const ThemeTokens& t) {
  p.stroke(0.0F, 0.0F, w, h, radius, 2.0F, t.accent);
}

float centered_x(ui::Painter& p, TextRole role, float w, std::string_view text) {
  return (w - p.text_width(role, text)) / 2.0F;
}

void stipple(ui::Painter& p, float x, float y, float w, float h, const Rgba& c) {
  p.push_clip(x, y, w, h);
  for (float d = -h; d < w; d += 4.0F) {
    p.line(x + d, y + h, x + d + h, y, 1.0F, alpha(c, 0.7F));
  }
  p.pop_clip();
}

void swatch(ui::Painter& p, float x, float y, const LayerRow& row, const ThemeTokens& t) {
  const auto w = static_cast<float>(kSwatchWidth);
  const auto h = static_cast<float>(kSwatchHeight);
  if (row.crossed) {
    p.fill(x, y, w, h, 3.0F, t.canvas);  // the canvas behind a contact cross
    p.line(x + 5.0F, y + 3.0F, x + w - 5.0F, y + h - 3.0F, 1.2F, row.outline);
    p.line(x + w - 5.0F, y + 3.0F, x + 5.0F, y + h - 3.0F, 1.2F, row.outline);
  } else {
    p.fill(x, y, w, h, 3.0F, row.fill);
    if (row.stippled) stipple(p, x, y, w, h, row.outline);
  }
  if (row.dashed) {
    p.dashed_rect(x, y, w, h, 1.0F, 3.0F, 2.0F, row.outline);
  } else {
    p.stroke(x, y, w, h, 3.0F, 1.0F, row.outline);
  }
}

/// A left-ruler label: compact, on one line when it fits the ruler, else
/// stacked one character per line; never wider than the ruler.
void left_ruler_label(ui::Painter& p, float y, const std::string& label, const ThemeTokens& t) {
  const auto rs = static_cast<float>(kRulerSize);
  const std::string text = compact_ruler_label(label);
  const float char_w = std::max(p.text_width(TextRole::mono_small, "0"), 1.0F);
  const auto max_chars = static_cast<std::size_t>((rs - 4.0F) / char_w);
  const float step = p.line_height(TextRole::mono_small) * 0.8F;
  float line_y = y + 2.0F;
  for (const std::string& line : ruler_label_lines(text, max_chars)) {
    const float x = (rs - 1.0F - p.text_width(TextRole::mono_small, line)) / 2.0F;
    p.text(TextRole::mono_small, x, line_y, line, t.text_tertiary);
    line_y += step;
  }
}

void rulers(ui::Painter& p, float w, float h, const OverlayState& s, const ThemeTokens& t) {
  const auto rs = static_cast<float>(kRulerSize);
  p.fill(0.0F, 0.0F, w, rs, 0.0F, t.bar);
  p.fill(0.0F, 0.0F, rs, h, 0.0F, t.bar);
  const double r = s.pixel_ratio;
  const auto& cam = s.camera;
  const auto wd = static_cast<double>(w);
  const auto hd = static_cast<double>(h);
  p.push_clip(rs, 0.0F, w - rs, rs);
  for (const RulerTick& tick : ruler_ticks(cam.world_x(0.0), cam.world_x(wd * r), wd, s.scale)) {
    const auto x = static_cast<float>(tick.pixel);
    if (x < rs + 2.0F) continue;
    p.line(x, rs - 5.0F, x, rs, 1.0F, t.text_tertiary);
    p.text(TextRole::mono_small, x + 3.0F, 3.0F, tick.label, t.text_tertiary);
  }
  p.pop_clip();
  p.push_clip(0.0F, rs, rs, h - rs);
  for (const RulerTick& tick : ruler_ticks(cam.world_y(0.0), cam.world_y(hd * r), hd, s.scale)) {
    const auto y = static_cast<float>(tick.pixel);
    if (y < rs + 2.0F) continue;
    p.line(rs - 5.0F, y, rs, y, 1.0F, t.text_tertiary);
    left_ruler_label(p, y, tick.label, t);
  }
  p.pop_clip();
}

/// The mockup's dot grid, fixed to world coordinates so it moves with the
/// view. The canvas is dark in both themes, so the dots use the dark shade.
void dot_grid(ui::Painter& p, float w, float h, const OverlayState& s) {
  const Rgba& dots = theme_tokens(ThemeMode::dark).row_selected;
  const double ppu = s.camera.pixels_per_unit() / s.pixel_ratio;
  if (!(ppu > 0.0)) return;
  double step = std::pow(10.0, std::ceil(std::log10(20.0 / ppu)));
  if (step * ppu / 2.0 >= 20.0) step /= 2.0;
  const double x0 = s.camera.world_x(0.0);
  const double y0 = s.camera.world_y(0.0);
  const double first_x = std::ceil(x0 / step) * step;
  const double first_y = std::floor(y0 / step) * step;
  int drawn = 0;
  const auto wd = static_cast<double>(w);
  const auto hd = static_cast<double>(h);
  for (double wy = first_y; (y0 - wy) * ppu < hd && drawn < 6000; wy -= step) {
    for (double wx = first_x; (wx - x0) * ppu < wd && drawn < 6000; wx += step, ++drawn) {
      p.fill(static_cast<float>((wx - x0) * ppu), static_cast<float>((y0 - wy) * ppu), 1.5F, 1.5F, 0.75F, dots);
    }
  }
}

Rgba style_rgba(const display::ResolvedStyle& style) {
  return Rgba{style.r, style.g, style.b, 1.0F};  // the PDK style's color, opaque
}

bool outside(const PixelRect& r, float w, float h) {
  return r.x > static_cast<double>(w) || r.y > static_cast<double>(h) || r.x + r.w < 0.0 || r.y + r.h < 0.0;
}

/// A caption centered at `cy` in a cell `w` wide at `x`; like Magic's
/// GrPutText with `adjust`, a caption that does not fit is not drawn.
void caption(ui::Painter& p, float x, float w, float cy, const std::string& text, const Rgba& color) {
  const float tw = p.text_width(TextRole::mono_small, text);
  if (tw > w - 4.0F) return;
  p.text_centered_y(TextRole::mono_small, x + (w - tw) / 2.0F, cy, text, color);
}

/// Unexpanded subcells: the definition name at 2/3 of the height and the
/// instance id at 1/3, in the cell style (15), as Magic's DBWdisplay does.
/// The outline itself is drawn by the renderer (cell_box batches).
void subcell_captions(ui::Painter& p, const OverlayState& s) {
  for (const display::SceneCell& cell : s.cells) {
    const PixelRect r = world_to_pixels(s.camera, cell.bbox, s.pixel_ratio);
    if (r.h <= 28.0) continue;
    const Rgba color = style_rgba(cell.style);
    const auto x = static_cast<float>(r.x);
    const auto y = static_cast<float>(r.y);
    const auto w = static_cast<float>(r.w);
    const auto h = static_cast<float>(r.h);
    caption(p, x, w, y + h / 3.0F, cell.name, color);
    caption(p, x, w, y + h * 2.0F / 3.0F, cell.id, color);
  }
}

/// `cross` styles (contacts): both diagonals of every rectangle.
void contact_crosses(ui::Painter& p, float w, float h, const OverlayState& s) {
  for (const display::SceneLayer& layer : s.crosses) {
    const Rgba color = style_rgba(layer.style);
    for (const db::Rect& rect : layer.rects) {
      const PixelRect r = world_to_pixels(s.camera, rect, s.pixel_ratio);
      if (outside(r, w, h) || r.w < 2.0 || r.h < 2.0) continue;
      const auto x0 = static_cast<float>(r.x);
      const auto y0 = static_cast<float>(r.y);
      const auto x1 = static_cast<float>(r.x + r.w);
      const auto y1 = static_cast<float>(r.y + r.h);
      p.line(x0, y0, x1, y1, 1.0F, color);
      p.line(x0, y1, x1, y0, 1.0F, color);
    }
  }
}

/// A font label: its glyph contours filled (nonzero rule) one pixel row at a time.
void font_label(ui::Painter& p, float h, const display::SceneLabel& label, const OverlayState& s) {
  std::vector<std::vector<PixelPoint>> polygons;
  double top = 1e300;
  double bottom = -1e300;
  for (const auto& contour : label.outline_polygons) {
    auto& poly = polygons.emplace_back();
    for (const display::PointF& pt : contour) {
      poly.push_back(world_point_to_pixels(s.camera, pt.x, pt.y, s.pixel_ratio));
      top = std::min(top, poly.back().y);
      bottom = std::max(bottom, poly.back().y);
    }
  }
  const Rgba color = style_rgba(label.style);
  const double first = std::max(std::floor(top), 0.0);
  const double last = std::min(std::ceil(bottom), static_cast<double>(h));
  for (double y = first; y < last; y += 1.0) {
    for (const PixelSpan& span : nonzero_spans(polygons, y + 0.5)) {
      p.fill(static_cast<float>(span.x0), static_cast<float>(y), static_cast<float>(span.x1 - span.x0), 1.0F, 0.0F,
             color);
    }
  }
}

/// Labels as Magic draws them: rendered text placed by the label's
/// justification (the label rectangle and port lines come from the renderer).
void labels(ui::Painter& p, float w, float h, const OverlayState& s) {
  for (const display::SceneLabel& label : s.labels) {
    const PixelRect r = world_to_pixels(s.camera, label.font_label ? label.font_bbox : label.rect, s.pixel_ratio);
    if (outside(r, w, h)) continue;
    if (label.font_label) {
      font_label(p, h, label, s);
      continue;
    }
    const auto tw = static_cast<double>(p.text_width(TextRole::mono, label.text));
    const auto th = static_cast<double>(p.line_height(TextRole::mono));
    const PixelPoint at = label_text_origin(r, label.justification, tw, th);
    p.text(TextRole::mono, static_cast<float>(at.x), static_cast<float>(at.y), label.text, style_rgba(label.style));
  }
}

void zoom_control(ui::Painter& p, const OverlayLayout& l, const std::string& percent, const ThemeTokens& t) {
  const auto control = [&](const PixelRect& r) {
    p.fill(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), kRadius,
           t.surface2);
    p.stroke(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h),
             kRadius, 1.0F, t.control_border);
  };
  control(l.zoom_out);
  control(l.zoom_label);
  control(l.zoom_in);
  const auto cy = [](const PixelRect& r) { return static_cast<float>(r.y + r.h / 2.0); };
  const auto cx = [](const PixelRect& r) { return static_cast<float>(r.x + r.w / 2.0); };
  p.line(cx(l.zoom_out) - 5.0F, cy(l.zoom_out), cx(l.zoom_out) + 5.0F, cy(l.zoom_out), 1.6F, t.text_primary);
  p.line(cx(l.zoom_in) - 5.0F, cy(l.zoom_in), cx(l.zoom_in) + 5.0F, cy(l.zoom_in), 1.6F, t.text_primary);
  p.line(cx(l.zoom_in), cy(l.zoom_in) - 5.0F, cx(l.zoom_in), cy(l.zoom_in) + 5.0F, 1.6F, t.text_primary);
  const float tw = p.text_width(TextRole::command_prompt, percent);
  p.text_centered_y(TextRole::command_prompt, cx(l.zoom_label) - tw / 2.0F, cy(l.zoom_label), percent, t.icon);
}

void minimap(ui::Painter& p, const OverlayLayout& l, const OverlayState& s, const ThemeTokens& t) {
  const auto x = static_cast<float>(l.minimap.x);
  const auto y = static_cast<float>(l.minimap.y);
  const auto w = static_cast<float>(l.minimap.w);
  const auto h = static_cast<float>(l.minimap.h);
  // Popover elevation approximated by stacked translucent rectangles (no blur in the painter).
  for (int i = 1; i <= 4; ++i) {
    const auto d = static_cast<float>(i) * 2.0F;
    p.fill(x - d / 2.0F, y + d, w + d, h, kPanelRadius + d / 2.0F, Rgba{0.0F, 0.0F, 0.0F, 0.09F});
  }
  p.fill(x, y, w, h, kPanelRadius, t.bar);
  p.stroke(x, y, w, h, kPanelRadius, 1.0F, t.control_border);
  const auto g = minimap_geometry(s.bounds, s.camera.visible_window(), l.minimap.w, l.minimap.h);
  if (!g) return;
  p.fill(x + static_cast<float>(g->content.x), y + static_cast<float>(g->content.y),
         static_cast<float>(g->content.w), static_cast<float>(g->content.h), 0.0F, alpha(t.text_tertiary, 0.18F));
  p.stroke(x + static_cast<float>(g->content.x), y + static_cast<float>(g->content.y),
           static_cast<float>(g->content.w), static_cast<float>(g->content.h), 0.0F, 1.0F, t.text_tertiary);
  p.stroke(x + static_cast<float>(g->view.x), y + static_cast<float>(g->view.y), static_cast<float>(g->view.w),
           static_cast<float>(g->view.h), 0.0F, 1.5F, white());
}

}  // namespace

void fill_rect(ui::Painter& p, float w, float h, const Rgba& c) {
  p.fill(0.0F, 0.0F, w, h, 0.0F, c);
}

void app_mark(ui::Painter& p, float x, float y, float size, const ThemeTokens& t) {
  const float k = size / 44.0F;  // the mark's 44-unit viewBox
  p.fill(x + k, y + k, 42.0F * k, 42.0F * k, 10.0F * k, t.surface2);
  p.stroke(x + k, y + k, 42.0F * k, 42.0F * k, 10.0F * k, 1.0F, t.border);
  // Ember base (M11 27h22l-3 5H14z) and the anvil (M15 27V19h14v8).
  p.fill(x + 14.0F * k, y + 27.0F * k, 16.0F * k, 5.0F * k, 0.0F, t.accent);
  p.fill(x + 11.0F * k, y + 27.0F * k, 22.0F * k, 2.0F * k, 0.0F, t.accent);
  const float sw = 2.2F * k;
  p.line(x + 15.0F * k, y + 27.0F * k, x + 15.0F * k, y + 19.0F * k, sw, t.text_primary);
  p.line(x + 15.0F * k, y + 19.0F * k, x + 29.0F * k, y + 19.0F * k, sw, t.text_primary);
  p.line(x + 29.0F * k, y + 19.0F * k, x + 29.0F * k, y + 27.0F * k, sw, t.text_primary);
}

void header_title(ui::Painter& p, float x, float h, const HeaderText& text, const ThemeTokens& t) {
  const float total = p.line_height(TextRole::cell_title) + p.line_height(TextRole::cell_subtitle);
  const float top = (h - total) / 2.0F;
  p.text(TextRole::cell_title, x, top, text.title, t.text_primary);
  p.text(TextRole::cell_subtitle, x, top + p.line_height(TextRole::cell_title), text.subtitle, t.text_tertiary);
}

float segmented_width(const ui::Runtime& rt, const std::vector<std::string_view>& labels) {
  float w = 6.0F + 2.0F;  // padding 3 + border 1, both sides
  for (std::size_t i = 0; i < labels.size(); ++i) {
    w += rt.text_width(i == 0 ? TextRole::tab_active : TextRole::tab, labels[i]) + 28.0F;
  }
  return w;
}

void segmented(ui::Painter& p, float w, float h, const std::vector<std::string_view>& labels, const ThemeTokens& t) {
  p.fill(0.0F, 0.0F, w, h, kPanelRadius, t.input);
  p.stroke(0.0F, 0.0F, w, h, kPanelRadius, 1.0F, t.border);
  float x = 4.0F;
  for (std::size_t i = 0; i < labels.size(); ++i) {
    const TextRole role = i == 0 ? TextRole::tab_active : TextRole::tab;
    const float bw = p.text_width(role, labels[i]) + 28.0F;
    if (i == 0) {
      p.fill(x, 4.0F, bw, h - 8.0F, kRadius, t.row_selected);
    }
    // Later-milestone tabs are disabled: tertiary text.
    p.text_centered_y(role, x + 14.0F, h / 2.0F, labels[i], i == 0 ? t.text_primary : t.text_tertiary);
    x += bw;
  }
}

float chip_width(const ui::Runtime& rt, const StatusChip& c) {
  return rt.text_width(TextRole::chip, c.label()) + 20.0F;
}

void chip(ui::Painter& p, float w, float h, const StatusChip& c, const ThemeTokens& t) {
  const Rgba base = status_color(t, c.kind);
  const Rgba text = status_text_color(t, c.kind);
  p.fill(0.0F, 0.0F, w, h, h / 2.0F, alpha(base, kChipTintAlpha));
  p.text_centered_y(TextRole::chip, 10.0F, h / 2.0F, c.label(), text);
}

float button_width(const ui::Runtime& rt, std::string_view label, bool with_icon) {
  return rt.text_width(TextRole::button, label) + 28.0F + (with_icon ? 23.0F : 0.0F);
}

/// The button background: primary ember, secondary surface, ghost only on hover.
static Rgba button_fill(ButtonStyle style, const WidgetState& s, const ThemeTokens& t) {
  const bool primary = style == ButtonStyle::primary;
  if (s.hovered && s.enabled) return primary ? t.accent_hover : t.row_selected;
  return primary ? t.accent : t.surface2;
}

void button(ui::Painter& p, float w, float h, std::string_view label, std::string_view icon, ButtonStyle style,
            WidgetState s, const ThemeTokens& t) {
  const float a = s.enabled ? 1.0F : kDisabledAlpha;
  if (style != ButtonStyle::ghost || s.hovered) p.fill(0.0F, 0.0F, w, h, kRadius, alpha(button_fill(style, s, t), a));
  if (style == ButtonStyle::secondary) p.stroke(0.0F, 0.0F, w, h, kRadius, 1.0F, alpha(t.control_border, a));
  const Rgba fg = alpha(style == ButtonStyle::primary ? t.accent_text : t.text_primary, a);
  // Content centered when the button is wider than it (flex-grow buttons).
  const float content = p.text_width(TextRole::button, label) + (icon.empty() ? 0.0F : 23.0F);
  float x = std::max(14.0F, (w - content) / 2.0F);
  if (!icon.empty()) {
    p.icon(icon, x, (h - 15.0F) / 2.0F, 15.0F, fg);
    x += 23.0F;
  }
  p.text_centered_y(TextRole::button, x, h / 2.0F, label, fg);
  if (s.focused) focus_ring(p, w, h, kRadius, t);
}

void tool(ui::Painter& p, float w, float h, const ToolButton& tool, WidgetState s, const ThemeTokens& t) {
  if (s.active) {
    p.fill(0.0F, 0.0F, w, h, kRadius, alpha(t.accent, 0.14F));
    p.stroke(0.0F, 0.0F, w, h, kRadius, 1.0F, alpha(t.accent, 0.6F));
  } else if (s.hovered && s.enabled) {
    p.fill(0.0F, 0.0F, w, h, kRadius, t.surface2);
  }
  const Rgba color = s.active ? t.accent_soft : alpha(t.icon, s.enabled ? 1.0F : kDisabledAlpha);
  p.icon(tool.icon, (w - 18.0F) / 2.0F, (h - 18.0F) / 2.0F, 18.0F, color);
  if (s.focused) focus_ring(p, w, h, kRadius, t);
}

void section_label(ui::Painter& p, float x, float y, std::string_view text, const ThemeTokens& t) {
  p.text(TextRole::section_label, x, y, text, t.text_tertiary);
}

void layer_row(ui::Painter& p, float w, float h, const LayerRow& row, WidgetState s, const ThemeTokens& t) {
  if (s.hovered) p.fill(0.0F, 0.0F, w, h, kRadius, alpha(t.row_selected, 0.6F));
  swatch(p, 8.0F, (h - static_cast<float>(kSwatchHeight)) / 2.0F, row, t);
  const Rgba name_color = row.visible ? t.text_primary : t.text_tertiary;
  p.text_centered_y(TextRole::body, 36.0F, h / 2.0F, row.name, name_color);
  const std::string meta = row.meta();
  const float mw = p.text_width(TextRole::meta, meta);
  p.text_centered_y(TextRole::meta, w - 8.0F - mw, h / 2.0F, meta, t.text_tertiary);
  if (s.hovered || !row.visible) {
    p.icon(row.visible ? "eye" : "eye-off", w - 8.0F - mw - 20.0F, (h - 14.0F) / 2.0F, 14.0F, t.text_tertiary);
  }
  if (s.focused) focus_ring(p, w, h, kRadius, t);
}

void hierarchy_row(ui::Painter& p, float w, float h, const HierarchyRow& row, WidgetState s, const ThemeTokens& t) {
  if (s.hovered && row.expandable) p.fill(0.0F, 0.0F, w, h, kRadius, alpha(t.row_selected, 0.6F));
  const Rgba color = row.depth == 0 ? t.text_primary : (row.use != nullptr ? t.icon : t.text_tertiary);
  // The name is ellipsized so that the suffix (×N, instance id) always shows.
  const float x = 16.0F * static_cast<float>(row.depth);
  const auto measure = [&p](std::string_view str) { return p.text_width(TextRole::mono, str); };
  const HierarchyRowLayout layout = hierarchy_row_layout(row, w - x, measure);
  p.push_clip(0.0F, 0.0F, w, h);
  p.text_centered_y(TextRole::mono, x, h / 2.0F, layout.name, color);
  p.text_centered_y(TextRole::mono, x + layout.suffix_x, h / 2.0F, row.suffix, color);
  p.pop_clip();
  if (s.focused) focus_ring(p, w, h, kRadius, t);
}

void inspector(ui::Painter& p, float w, float h, const InspectorView& view, const ThemeTokens& t) {
  const float pad = 14.0F;
  float y = pad;
  section_label(p, pad, y, view.section, t);
  y += p.line_height(TextRole::section_label) + 4.0F;
  // Long cell names are ellipsized inside the padding; the tooltip has them whole.
  const float content = w - 2.0F * pad;
  const auto heading = [&p](std::string_view str) { return p.text_width(TextRole::heading, str); };
  const auto small = [&p](std::string_view str) { return p.text_width(TextRole::small, str); };
  p.text(TextRole::heading, pad, y, ellipsize(view.title, content, heading), t.text_primary);
  y += p.line_height(TextRole::heading) + 4.0F;
  p.text(TextRole::small, pad, y, ellipsize(view.subtitle, content, small), t.text_secondary);
  y += p.line_height(TextRole::small) + 16.0F;
  const float col2 = pad + (w - 2.0F * pad - 12.0F) / 2.0F + 12.0F;
  for (const InspectorRow& row : view.rows) {
    p.text(TextRole::small, pad, y, row.key, t.text_tertiary);
    p.text(TextRole::mono, col2, y, row.value, t.text_primary);
    y += p.line_height(TextRole::small) + 10.0F;
  }
  // "Design rules nearby" card: DRC arrives in M3, so it is a disabled placeholder.
  const float card_h = 70.0F;
  const float cy = h - pad - card_h;
  p.fill(pad, cy, w - 2.0F * pad, card_h, kPanelRadius, t.surface2);
  p.stroke(pad, cy, w - 2.0F * pad, card_h, kPanelRadius, 1.0F, t.border);
  p.text(TextRole::small, pad + 12.0F, cy + 12.0F, "Design rules nearby", t.text_primary);
  p.text(TextRole::small, pad + 12.0F, cy + 34.0F, std::string(status_glyph(StatusKind::info)) + " Arrives with DRC (M3)",
         t.text_secondary);
}

void status_bar(ui::Painter& p, float w, float h, const StatusBarText& text, const StatusBarKinds& kinds,
                const ThemeTokens& t) {
  p.fill(0.0F, 0.0F, w, h, 0.0F, t.input);
  p.fill(0.0F, 0.0F, w, 1.0F, 0.0F, t.border);
  float x = 14.0F;
  for (const std::string* part : {&text.cursor, &text.box, &text.edit, &text.snap}) {
    p.text_centered_y(TextRole::mono_small, x, h / 2.0F, *part, t.text_secondary);
    x += p.text_width(TextRole::mono_small, *part) + 22.0F;
  }
  float right = w - 14.0F - p.text_width(TextRole::mono_small, text.drc);
  p.text_centered_y(TextRole::mono_small, right, h / 2.0F, text.drc, status_text_color(t, kinds.drc));
  if (!text.problems.empty()) {
    right -= 22.0F + p.text_width(TextRole::mono_small, text.problems);
    p.text_centered_y(TextRole::mono_small, right, h / 2.0F, text.problems, status_text_color(t, kinds.problems));
  }
  if (!text.message.empty() && right - 22.0F > x) {
    const Rgba color = kinds.message == StatusKind::info ? t.text_primary : status_text_color(t, kinds.message);
    // Ellipsized: the full message is the bar's tooltip and in Diagnostics.
    const auto measure = [&p](std::string_view str) { return p.text_width(TextRole::mono_small, str); };
    p.text_centered_y(TextRole::mono_small, x, h / 2.0F, ellipsize(text.message, right - 22.0F - x, measure), color);
  }
}

void canvas_overlay(ui::Painter& p, float w, float h, const OverlayState& s, ThemeMode mode) {
  // The canvas is dark in both themes: everything on it uses the dark tokens.
  const ThemeTokens& t = canvas_overlay_tokens(mode);
  const OverlayLayout l = overlay_layout(static_cast<double>(w), static_cast<double>(h));
  // Raven does not clip a painted widget to its bounds: layout-sized
  // annotations (box, crosses, labels) would spill over the side panels.
  p.push_clip(0.0F, 0.0F, w, h);
  dot_grid(p, w, h, s);
  contact_crosses(p, w, h, s);
  subcell_captions(p, s);
  labels(p, w, h, s);
  if (s.box) {
    const PixelRect r = world_to_pixels(s.camera, *s.box, s.pixel_ratio);
    p.dashed_rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h),
                  1.0F, 4.0F, 4.0F, white());
  }
  if (!s.empty_hint.empty()) {
    p.text_centered_y(TextRole::body, centered_x(p, TextRole::body, w, s.empty_hint), h / 2.0F, s.empty_hint,
                      t.text_secondary);
  }
  rulers(p, w, h, s, t);
  zoom_control(p, l, zoom_percent_text(s.camera.pixels_per_unit(), s.fit_pixels_per_unit), t);
  minimap(p, l, s, t);
  p.pop_clip();
}

}  // namespace hefesto::shell::paint
