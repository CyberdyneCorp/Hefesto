// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Object geometry of the editing operations (capability schematic-editing
// "Snap and grid", "Selection", "Move, copy and stretch", "Rotate and
// flip"). Coordinates are document coordinates (x right, y down); a
// rotation step turns 90 degrees clockwise on screen, as one step of the
// "Orientation transform" of schematic-file-format (transform.hpp), so the
// orientation of rotated instances and texts follows that transform.
//
// Owner: EDIT.

#include <algorithm>
#include <cmath>

#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "internal.hpp"

namespace hefesto::schem {

double snap_value(double value, double snap) noexcept {
  if (!(snap > 0.0) || !std::isfinite(snap)) return value;
  return std::round(value / snap) * snap;
}

Point snap_point(Point p, double snap) noexcept {
  return {snap_value(p.x, snap), snap_value(p.y, snap)};
}

void halve_snap(SnapSettings& settings) noexcept {
  settings.snap /= 2.0;
}

void double_snap(SnapSettings& settings) noexcept {
  settings.snap *= 2.0;
}

bool set_snap(SnapSettings& settings, double value) noexcept {
  if (!(value > 0.0) || !std::isfinite(value)) return false;
  settings.snap = value;
  return true;
}

}  // namespace hefesto::schem

namespace hefesto::schem::editimpl {

namespace {

// Rotation as the transform reads it: 0, 1 and 2 as stored, any other as 3.
int effective_rotation(int rotation) noexcept {
  return (rotation == 0 || rotation == 1 || rotation == 2) ? rotation : 3;
}

void rotate_orientation(int& rotation, int& flip, Motion::Kind kind) noexcept {
  const int r = effective_rotation(rotation);
  flip = flip != 0 ? 1 : 0;
  if (kind == Motion::Kind::rotate) {
    rotation = (r + 1) % 4;
    return;
  }
  // A mirror M after R^r F is R^-r M F: "Orientation transform" flips first.
  rotation = (4 - r) % 4;
  flip ^= 1;
}

void order_box(LineRecord& box) noexcept {
  const Point a = box.p1;
  const Point b = box.p2;
  box.p1 = {std::min(a.x, b.x), std::min(a.y, b.y)};
  box.p2 = {std::max(a.x, b.x), std::max(a.y, b.y)};
}

void move_segment(const Motion& motion, LineRecord& line) {
  line.p1 = apply(motion, line.p1);
  line.p2 = apply(motion, line.p2);
}

double arc_start(const Motion& motion, double start, double sweep) noexcept {
  switch (motion.kind) {
    case Motion::Kind::rotate:
      return transform_arc_start(Placement{{}, 1, 0}, start, sweep);
    case Motion::Kind::flip:
      return transform_arc_start(Placement{{}, 0, 1}, start, sweep);
    default:
      return start;
  }
}

void move_text(const Motion& motion, TextRecord& text) {
  text.origin = apply(motion, text.origin);
  if (motion.kind == Motion::Kind::translate) return;
  const Placement turn = motion.kind == Motion::Kind::rotate ? Placement{{}, 1, 0} : Placement{{}, 0, 1};
  const TextOrientation o = transform_text(turn, effective_rotation(text.rotation), text.flip != 0 ? 1 : 0);
  text.rotation = o.rotation;
  text.flip = o.flip;
}

void move_instance(const Motion& motion, InstanceRecord& instance) {
  instance.origin = apply(motion, instance.origin);
  if (motion.kind != Motion::Kind::translate) rotate_orientation(instance.rotation, instance.flip, motion.kind);
}

Point snapped(Point p, double snap) noexcept {
  return snap_point(p, snap);
}

// Liang-Barsky: whether the segment a-b has a point inside `r`.
bool clip(double p, double q, double& t0, double& t1) noexcept {
  if (p == 0.0) return q >= 0.0;
  const double t = q / p;
  if (p < 0.0) {
    if (t > t1) return false;
    t0 = std::max(t0, t);
  } else {
    if (t < t0) return false;
    t1 = std::min(t1, t);
  }
  return true;
}

bool segment_touches(Point a, Point b, const Rect& r) noexcept {
  double t0 = 0.0;
  double t1 = 1.0;
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  return clip(-dx, a.x - r.min.x, t0, t1) && clip(dx, r.max.x - a.x, t0, t1) && clip(-dy, a.y - r.min.y, t0, t1) &&
         clip(dy, r.max.y - a.y, t0, t1);
}

bool polygon_touches(const std::vector<Point>& points, const Rect& r) noexcept {
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    if (segment_touches(points[i], points[i + 1], r)) return true;
  }
  return points.size() == 1 && r.contains(points.front());
}

Rect arc_bounds(const ArcRecord& arc) noexcept {
  Rect r;
  r.add(Point{arc.centre.x - arc.radius, arc.centre.y - arc.radius});
  r.add(Point{arc.centre.x + arc.radius, arc.centre.y + arc.radius});
  return r;
}

Rect symbol_bounds(const Document& symbol) {
  Rect r;
  for (const auto& line : symbol.lines) r.add(rect_of(line.p1, line.p2));
  for (const auto& box : symbol.boxes) r.add(rect_of(box.p1, box.p2));
  for (const auto& arc : symbol.arcs) r.add(arc_bounds(arc));
  for (const auto& polygon : symbol.polygons) {
    for (const Point p : polygon.points) r.add(p);
  }
  return r;
}

Rect instance_bounds(const InstanceRecord& instance, const Symbol* symbol) {
  Rect r;
  r.add(instance.origin);
  if (symbol == nullptr) return r;
  const Rect local = symbol_bounds(symbol->document);
  if (local.empty) return r;
  const Placement placement{instance.origin, instance.rotation, instance.flip};
  Rect placed;
  placed.add(transform_point(placement, local.min));
  placed.add(transform_point(placement, local.max));
  return placed;
}

}  // namespace

// ---- Rect ------------------------------------------------------------------

void Rect::add(Point p) noexcept {
  if (empty) {
    min = p;
    max = p;
    empty = false;
    return;
  }
  min = {std::min(min.x, p.x), std::min(min.y, p.y)};
  max = {std::max(max.x, p.x), std::max(max.y, p.y)};
}

void Rect::add(const Rect& r) noexcept {
  if (r.empty) return;
  add(r.min);
  add(r.max);
}

bool Rect::contains(Point p) const noexcept {
  return !empty && p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

bool Rect::contains(const Rect& r) const noexcept {
  return !r.empty && contains(r.min) && contains(r.max);
}

bool Rect::overlaps(const Rect& r) const noexcept {
  return !empty && !r.empty && r.min.x <= max.x && r.max.x >= min.x && r.min.y <= max.y && r.max.y >= min.y;
}

Rect rect_of(Point a, Point b) noexcept {
  Rect r;
  r.add(a);
  r.add(b);
  return r;
}

// ---- motions ---------------------------------------------------------------

Point apply(const Motion& motion, Point p) noexcept {
  switch (motion.kind) {
    case Motion::Kind::rotate:  // 90 degrees clockwise on screen (y down)
      return {motion.pivot.x - (p.y - motion.pivot.y), motion.pivot.y + (p.x - motion.pivot.x)};
    case Motion::Kind::flip:  // mirror about the vertical line through the pivot
      return {motion.pivot.x - (p.x - motion.pivot.x), p.y};
    default:
      return {p.x + motion.delta.x, p.y + motion.delta.y};
  }
}

void apply(const Motion& motion, Document& doc, ObjectRef ref) {
  switch (ref.kind) {
    case ObjectKind::line:
      move_segment(motion, doc.lines[ref.index]);
      break;
    case ObjectKind::box:
      move_segment(motion, doc.boxes[ref.index]);
      if (motion.kind != Motion::Kind::translate) order_box(doc.boxes[ref.index]);
      break;
    case ObjectKind::arc: {
      ArcRecord& arc = doc.arcs[ref.index];
      arc.centre = apply(motion, arc.centre);
      arc.start_angle = arc_start(motion, arc.start_angle, arc.sweep_angle);
      break;
    }
    case ObjectKind::polygon:
      for (Point& p : doc.polygons[ref.index].points) p = apply(motion, p);
      break;
    case ObjectKind::text:
      move_text(motion, doc.texts[ref.index]);
      break;
    case ObjectKind::wire:
      doc.wires[ref.index].p1 = apply(motion, doc.wires[ref.index].p1);
      doc.wires[ref.index].p2 = apply(motion, doc.wires[ref.index].p2);
      break;
    case ObjectKind::instance:
      move_instance(motion, doc.instances[ref.index]);
      break;
  }
}

Point anchor(const Document& doc, ObjectRef ref) {
  switch (ref.kind) {
    case ObjectKind::line:
      return doc.lines[ref.index].p1;
    case ObjectKind::box:
      return doc.boxes[ref.index].p1;
    case ObjectKind::arc:
      return doc.arcs[ref.index].centre;
    case ObjectKind::polygon:
      return doc.polygons[ref.index].points.empty() ? Point{} : doc.polygons[ref.index].points.front();
    case ObjectKind::text:
      return doc.texts[ref.index].origin;
    case ObjectKind::wire:
      return doc.wires[ref.index].p1;
    case ObjectKind::instance:
      return doc.instances[ref.index].origin;
  }
  return {};
}

void align(Document& doc, ObjectRef ref, double snap) {
  switch (ref.kind) {
    case ObjectKind::line:
    case ObjectKind::box: {
      LineRecord& line = ref.kind == ObjectKind::line ? doc.lines[ref.index] : doc.boxes[ref.index];
      line.p1 = snapped(line.p1, snap);
      line.p2 = snapped(line.p2, snap);
      break;
    }
    case ObjectKind::arc:
      doc.arcs[ref.index].centre = snapped(doc.arcs[ref.index].centre, snap);
      break;
    case ObjectKind::polygon:
      for (Point& p : doc.polygons[ref.index].points) p = snapped(p, snap);
      break;
    case ObjectKind::text:
      doc.texts[ref.index].origin = snapped(doc.texts[ref.index].origin, snap);
      break;
    case ObjectKind::wire:
      doc.wires[ref.index].p1 = snapped(doc.wires[ref.index].p1, snap);
      doc.wires[ref.index].p2 = snapped(doc.wires[ref.index].p2, snap);
      break;
    case ObjectKind::instance:
      doc.instances[ref.index].origin = snapped(doc.instances[ref.index].origin, snap);
      break;
  }
}

Rect bounds(const Document& doc, ObjectRef ref, const Symbol* symbol) {
  switch (ref.kind) {
    case ObjectKind::line:
      return rect_of(doc.lines[ref.index].p1, doc.lines[ref.index].p2);
    case ObjectKind::box:
      return rect_of(doc.boxes[ref.index].p1, doc.boxes[ref.index].p2);
    case ObjectKind::arc:
      return arc_bounds(doc.arcs[ref.index]);
    case ObjectKind::polygon: {
      Rect r;
      for (const Point p : doc.polygons[ref.index].points) r.add(p);
      return r;
    }
    case ObjectKind::text:
      return rect_of(doc.texts[ref.index].origin, doc.texts[ref.index].origin);
    case ObjectKind::wire:
      return rect_of(doc.wires[ref.index].p1, doc.wires[ref.index].p2);
    case ObjectKind::instance:
      return instance_bounds(doc.instances[ref.index], symbol);
  }
  return {};
}

bool touches_area(const Document& doc, ObjectRef ref, const Rect& area, const Symbol* symbol) {
  switch (ref.kind) {
    case ObjectKind::line:
      return segment_touches(doc.lines[ref.index].p1, doc.lines[ref.index].p2, area);
    case ObjectKind::wire:
      return segment_touches(doc.wires[ref.index].p1, doc.wires[ref.index].p2, area);
    case ObjectKind::polygon:
      return polygon_touches(doc.polygons[ref.index].points, area);
    default:
      return area.overlaps(bounds(doc, ref, symbol));
  }
}

std::size_t object_count(const Document& doc, ObjectKind kind) noexcept {
  switch (kind) {
    case ObjectKind::line:
      return doc.lines.size();
    case ObjectKind::box:
      return doc.boxes.size();
    case ObjectKind::arc:
      return doc.arcs.size();
    case ObjectKind::polygon:
      return doc.polygons.size();
    case ObjectKind::text:
      return doc.texts.size();
    case ObjectKind::wire:
      return doc.wires.size();
    case ObjectKind::instance:
      return doc.instances.size();
  }
  return 0;
}

std::vector<Point> pin_points(const InstanceRecord& instance, const Symbol& symbol) {
  const Placement placement{instance.origin, instance.rotation, instance.flip};
  std::vector<Point> points;
  points.reserve(symbol.pins.size());
  for (const Pin& pin : symbol.pins) points.push_back(transform_point(placement, pin_point(pin.box)));
  return points;
}

}  // namespace hefesto::schem::editimpl
