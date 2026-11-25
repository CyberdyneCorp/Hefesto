// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic canvas view model (schematic_canvas.hpp). Owner: SHELL.

#include "hefesto/shell/schematic_canvas.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <system_error>
#include <limits>
#include <string_view>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"

namespace hefesto::shell {

namespace {

using schem::Point;

constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

bool token_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string attribute(std::string_view props, std::string_view key) {
  return schem::get_token(props, key).value_or(std::string{});
}

// The symbol's file name without folder and `.sym`, as `@symname` shows it.
std::string symbol_name(std::string_view reference) {
  std::string_view name = reference.substr(reference.find_last_of('/') + 1);
  if (name.ends_with(".sym")) name.remove_suffix(4);
  return std::string{name};
}

// `@key` replaced by the instance's attribute `key` (`@symname` by the
// symbol's name); a token the instance does not set stays as written.
std::string substitute(std::string_view text, const schem::InstanceRecord& instance) {
  std::string out;
  std::size_t i = 0;
  while (i < text.size()) {
    if (text[i] != '@') {
      out += text[i++];
      continue;
    }
    std::size_t end = i + 1;
    while (end < text.size() && token_char(text[end])) ++end;
    const std::string_view key = text.substr(i + 1, end - i - 1);
    const auto value = key == "symname" ? std::optional<std::string>{symbol_name(instance.symbol)}
                                        : schem::get_token(instance.props, key);
    out += value && !key.empty() ? *value : std::string{text.substr(i, end - i)};
    i = end;
  }
  return out;
}

struct Placer {
  SchematicScene& scene;
  schem::Placement placement;
  SceneSource source = SceneSource::graphic;
  std::size_t index = 0;
  SceneBox* box = nullptr;  // the instance's box, also grown

  [[nodiscard]] Point at(Point p) const noexcept {
    return source == SceneSource::graphic || source == SceneSource::wire ? p
                                                                         : schem::transform_point(placement, p);
  }
  void grow(Point p) const noexcept {
    scene.box.add(p);
    if (box != nullptr) box->add(p);
  }

  void segment(const schem::LineRecord& line, bool highlighted = false) const {
    const SceneSegment s{at(line.p1), at(line.p2), line.layer, source, index, highlighted};
    grow(s.a);
    grow(s.b);
    scene.segments.push_back(s);
  }
  void rectangle(const schem::BoxRecord& rect) const {
    const bool pin = source == SceneSource::symbol && rect.layer == schem::kPinLayer;
    ScenePolygon poly{{}, rect.layer, pin ? SceneSource::pin : source, index, pin || filled(rect.props)};
    for (const Point corner : {rect.p1, Point{rect.p2.x, rect.p1.y}, rect.p2, Point{rect.p1.x, rect.p2.y}}) {
      poly.points.push_back(at(corner));
      grow(poly.points.back());
    }
    scene.polygons.push_back(std::move(poly));
  }
  void polygon(const schem::PolygonRecord& record) const {
    ScenePolygon poly{{}, record.layer, source, index, filled(record.props)};
    for (const Point p : record.points) {
      poly.points.push_back(at(p));
      grow(poly.points.back());
    }
    scene.polygons.push_back(std::move(poly));
  }
  void arc(const schem::ArcRecord& record) const {
    SceneArc a{at(record.centre), record.radius, record.start_angle, record.sweep_angle, record.layer, source, index};
    if (source == SceneSource::symbol) {
      a.start_angle = schem::transform_arc_start(placement, record.start_angle, record.sweep_angle);
    }
    grow(Point{a.centre.x - a.radius, a.centre.y - a.radius});
    grow(Point{a.centre.x + a.radius, a.centre.y + a.radius});
    scene.arcs.push_back(a);
  }
  void text(const schem::TextRecord& record, std::string shown) const {
    SceneText t{std::move(shown), at(record.origin), record.rotation, record.flip, record.y_scale, 0, source, index};
    if (source == SceneSource::symbol) {
      const schem::TextOrientation o = schem::transform_text(placement, record.rotation, record.flip);
      t.rotation = o.rotation;
      t.flip = o.flip;
    }
    t.layer = layer_of(record.props);
    grow(t.origin);
    scene.texts.push_back(std::move(t));
  }

  // The text's `layer` attribute; 0 when absent or not a number.
  static int layer_of(std::string_view props) {
    const std::string value = attribute(props, "layer");
    int layer = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), layer);
    return ec == std::errc{} && end == value.data() + value.size() ? layer : 0;
  }
  static bool filled(std::string_view props) {
    const auto fill = schem::get_token(props, "fill");
    return fill && schem::is_true(*fill);
  }
};

// The graphics of `document` (a schematic's own, or a placed symbol's).
void add_graphics(const Placer& placer, const schem::Document& document, const schem::InstanceRecord* instance) {
  for (const schem::LineRecord& line : document.lines) placer.segment(line);
  for (const schem::BoxRecord& rect : document.boxes) placer.rectangle(rect);
  for (const schem::PolygonRecord& poly : document.polygons) placer.polygon(poly);
  for (const schem::ArcRecord& arc : document.arcs) placer.arc(arc);
  for (const schem::TextRecord& text : document.texts) {
    placer.text(text, instance != nullptr ? substitute(text.text, *instance) : text.text);
  }
}

void add_instance(SchematicScene& scene, const schem::InstanceRecord& instance, std::size_t index,
                  const SymbolLookup& lookup, const SceneHighlight& highlight, const schem::Symbol* pending) {
  const schem::Symbol& symbol = lookup(instance);
  SceneInstance shown{attribute(instance.props, "name"), instance.symbol, {}, symbol.placeholder, &symbol == pending,
                      false};
  shown.highlighted = highlight.instances.contains(shown.name);
  const Placer placer{scene, schem::Placement{instance.origin, instance.rotation, instance.flip}, SceneSource::symbol,
                      index, &shown.box};
  add_graphics(placer, symbol.document, &instance);
  if (shown.box.empty) placer.grow(instance.origin);  // a symbol without graphics: its origin
  scene.instances.push_back(std::move(shown));
}

}  // namespace

void SceneBox::add(Point p) noexcept {
  if (empty) {
    *this = SceneBox{p.x, p.y, p.x, p.y, false};
    return;
  }
  x1 = std::min(x1, p.x);
  y1 = std::min(y1, p.y);
  x2 = std::max(x2, p.x);
  y2 = std::max(y2, p.y);
}

void SceneBox::add(const SceneBox& other) noexcept {
  if (other.empty) return;
  add(Point{other.x1, other.y1});
  add(Point{other.x2, other.y2});
}

bool SceneBox::contains(Point p) const noexcept {
  return !empty && p.x >= x1 && p.x <= x2 && p.y >= y1 && p.y <= y2;
}

std::size_t SchematicScene::instance_at(Point p) const noexcept {
  for (std::size_t i = instances.size(); i > 0; --i) {
    if (instances[i - 1].box.contains(p)) return i - 1;
  }
  return kNone;
}

SchematicScene build_scene(const schem::Document& document, const SymbolLookup& lookup,
                           const SceneHighlight& highlight, const schem::Symbol* pending) {
  SchematicScene scene;
  add_graphics(Placer{scene, {}, SceneSource::graphic, 0, nullptr}, document, nullptr);
  for (std::size_t i = 0; i < document.wires.size(); ++i) {
    const schem::WireRecord& wire = document.wires[i];
    const auto lab = schem::get_token(wire.props, "lab");
    const bool lit = lab && highlight.nets.contains(*lab);
    Placer{scene, {}, SceneSource::wire, i, nullptr}.segment(schem::LineRecord{0, wire.p1, wire.p2, {}}, lit);
  }
  for (std::size_t i = 0; i < document.instances.size(); ++i) {
    add_instance(scene, document.instances[i], i, lookup, highlight, pending);
  }
  return scene;
}

SymbolLookup session_symbol_lookup(schem::Session& session, const schem::Document& document,
                                   diag::DiagnosticSink* warnings) {
  return [&session, &document, warnings](const schem::InstanceRecord& instance) -> const schem::Symbol& {
    if (session.files == nullptr) return session.symbols.placeholder();
    const bool compat = session.compat == nullptr || session.compat->xschem_compat;
    const schem::ResolveContext context{session.search_path, *session.files, warnings, compat};
    return session.symbols.resolve(schem::Reference{instance.symbol}, document, context);
  };
}

// --- viewport ------------------------------------------------------------------------------

void SchematicViewport::set_size(double width, double height) noexcept {
  width_ = std::max(width, 0.0);
  height_ = std::max(height, 0.0);
}

CanvasPoint SchematicViewport::to_canvas(Point p) const noexcept {
  return CanvasPoint{width_ / 2.0 + (p.x - centre_.x) * zoom_, height_ / 2.0 + (p.y - centre_.y) * zoom_};
}

Point SchematicViewport::to_schematic(CanvasPoint p) const noexcept {
  return Point{centre_.x + (p.x - width_ / 2.0) / zoom_, centre_.y + (p.y - height_ / 2.0) / zoom_};
}

void SchematicViewport::pan_by(double dx, double dy) noexcept {
  centre_.x -= dx / zoom_;
  centre_.y -= dy / zoom_;
}

void SchematicViewport::zoom_at(double factor, CanvasPoint at) noexcept {
  if (!(factor > 0.0)) return;
  const Point anchor = to_schematic(at);
  zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
  const Point moved = to_schematic(at);
  centre_.x += anchor.x - moved.x;
  centre_.y += anchor.y - moved.y;
}

void SchematicViewport::fit(const SceneBox& box) noexcept {
  if (box.empty || width_ <= 0.0 || height_ <= 0.0) return;
  centre_ = Point{(box.x1 + box.x2) / 2.0, (box.y1 + box.y2) / 2.0};
  const double usable = 1.0 - 2.0 * kFitMargin;
  const double w = std::max(box.x2 - box.x1, 1.0);
  const double h = std::max(box.y2 - box.y1, 1.0);
  zoom_ = std::clamp(std::min(width_ * usable / w, height_ * usable / h), kMinZoom, kMaxZoom);
}

}  // namespace hefesto::shell
