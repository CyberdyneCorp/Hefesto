// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Schematic tab's Raven view (schematic_view.hpp). Owner: SHELL.

#include "schematic_view.hpp"

#include <cmath>
#include <numbers>
#include <string>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/manager_views.hpp"
#include "side_panels.hpp"

namespace hefesto::shell {

namespace {

constexpr float kStroke = 1.5F;
constexpr float kCrumbBar = 28.0F;
constexpr int kArcSteps = 24;

Rgba item_color(const ThemeTokens& t, SceneSource source, bool highlighted) {
  if (highlighted) return t.accent;
  switch (source) {
    case SceneSource::wire: return t.info;
    case SceneSource::pin: return t.danger;
    case SceneSource::symbol: return t.success;
    case SceneSource::graphic: break;
  }
  return t.icon;
}

}  // namespace

SchematicView::SchematicView(ScreenContext& ctx) : ctx_(ctx) {
  canvas_ = make_painted(ctx_, [this](ui::Painter& p, float w, float h, const ThemeTokens& t) { paint(p, w, h, t); });
  canvas_->on_click([this](float x, float y) { click(x, y); });
  canvas_->set_accessible_name("Schematic canvas");
  breadcrumb_.set_top("No schematic open");
}

SchematicView::~SchematicView() = default;

ui::Widget& SchematicView::widget() noexcept {
  return *canvas_;
}

void SchematicView::refresh() {
  schem::Session& session = ctx_.editor().schem;
  has_document_ = !session.documents.empty();
  if (!has_document_) {
    scene_ = {};
    breadcrumb_.set_top("No schematic open");
    breadcrumb_.set_path(".");
  } else {
    const schem::Document& doc = session.documents.back().document;  // the active one is last
    scene_ = build_scene(doc, session_symbol_lookup(session, doc, nullptr));
    breadcrumb_.set_top(doc.path.empty() ? std::string{"untitled.sch"} : doc.path.filename().string());
  }
  canvas_->invalidate();
}

void SchematicView::paint(ui::Painter& p, float w, float h, const ThemeTokens& t) {
  p.fill(0.0F, 0.0F, w, h, 0.0F, t.canvas);
  viewport_.set_size(static_cast<double>(w), static_cast<double>(h - kCrumbBar));
  viewport_.fit(scene_.box);
  const auto at = [this](schem::Point pt) {
    const CanvasPoint c = viewport_.to_canvas(pt);
    return std::pair<float, float>{static_cast<float>(c.x), static_cast<float>(c.y) + kCrumbBar};
  };
  const auto segment = [&](schem::Point a, schem::Point b, const Rgba& c) {
    const auto [x0, y0] = at(a);
    const auto [x1, y1] = at(b);
    p.line(x0, y0, x1, y1, kStroke, c);
  };
  for (const SceneSegment& s : scene_.segments) segment(s.a, s.b, item_color(t, s.source, s.highlighted));
  for (const ScenePolygon& poly : scene_.polygons) {
    for (std::size_t i = 0; i < poly.points.size(); ++i) {
      segment(poly.points[i], poly.points[(i + 1) % poly.points.size()], item_color(t, poly.source, false));
    }
  }
  for (const SceneArc& arc : scene_.arcs) {
    const double step = arc.sweep_angle / kArcSteps * std::numbers::pi / 180.0;
    const double start = arc.start_angle * std::numbers::pi / 180.0;
    for (int i = 0; i < kArcSteps; ++i) {
      // y down: positive angles turn clockwise on screen, as the file's arcs.
      const auto point = [&](int k) {
        const double a = start + step * k;
        return schem::Point{arc.centre.x + arc.radius * std::cos(a), arc.centre.y - arc.radius * std::sin(a)};
      };
      segment(point(i), point(i + 1), item_color(t, arc.source, false));
    }
  }
  for (const SceneText& text : scene_.texts) {
    const auto [x, y] = at(text.origin);
    p.text(TextRole::meta, x, y, text.text, t.text_secondary);
  }
  p.fill(0.0F, 0.0F, w, kCrumbBar, 0.0F, t.bar);
  p.text_centered_y(TextRole::body, 12.0F, kCrumbBar / 2.0F, breadcrumb_.text(),
                    has_document_ ? t.text_primary : t.text_tertiary);
}

void SchematicView::click(float x, float y) {
  if (!has_document_ || y < kCrumbBar) return;
  const CanvasPoint at{static_cast<double>(x), static_cast<double>(y - kCrumbBar)};
  const schem::Point pt = viewport_.to_schematic(at);
  const std::size_t hit = scene_.instance_at(pt);
  if (hit >= scene_.instances.size() || scene_.instances[hit].name.empty()) return;
  if (ctx_.run) ctx_.run(command_line({"schematic", "select", "instance", scene_.instances[hit].name}));
}

}  // namespace hefesto::shell
