// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `move`, `copy`, `rotate`, `flip`, `flipv` and `align` on the selection
// (capability schematic-editing "Snap and grid", "Move, copy and stretch",
// "Rotate and flip"). Each is one undo step, and a move with stretched or
// inserted wires is one step too ("Undo and redo").
//
// Pivot. `rotate`/`flip` take the pivot as `<x> <y>` (the snapped pointer of
// the interactive command); without it, the centre of the selection's
// bounding box snapped to the snap; with `-inplace` each object turns
// around its own anchor.
//
// Owner: EDIT.

#include <array>
#include <map>
#include <string>
#include <vector>

#include "hefesto/schem/sym/instance.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

using cmdimpl::FlagSpec;

bool same(Point a, Point b) noexcept {
  return a == b;
}

bool contains_point(const std::vector<Point>& points, Point p) {
  for (const Point q : points) {
    if (same(p, q)) return true;
  }
  return false;
}

// The pin points of the selected instances and the endpoints of the
// selected wires, before the move.
std::vector<Point> moved_points(EditContext& ec) {
  std::vector<Point> points;
  for (const ObjectRef& ref : ec.selection()) {
    if (ref.kind == ObjectKind::wire) {
      points.push_back(ec.doc().wires[ref.index].p1);
      points.push_back(ec.doc().wires[ref.index].p2);
    }
    if (ref.kind == ObjectKind::instance) {
      const InstanceRecord& instance = ec.doc().instances[ref.index];
      for (const Point p : pin_points(instance, ec.symbol(instance))) points.push_back(p);
    }
  }
  return points;
}

std::vector<Point> selected_pins(EditContext& ec) {
  std::vector<Point> points;
  for (const ObjectRef& ref : ec.selection()) {
    if (ref.kind != ObjectKind::instance) continue;
    const InstanceRecord& instance = ec.doc().instances[ref.index];
    for (const Point p : pin_points(instance, ec.symbol(instance))) points.push_back(p);
  }
  return points;
}

// "Stretch move": endpoints of unselected wires on a moved point follow it.
std::vector<Point> stretch(EditContext& ec, Point delta) {
  const std::vector<Point> points = moved_points(ec);
  std::vector<Point> kept;
  for (std::size_t i = 0; i < ec.doc().wires.size(); ++i) {
    if (ec.selection().contains({ObjectKind::wire, i})) continue;
    WireRecord& wire = ec.doc().wires[i];
    for (Point* end : {&wire.p1, &wire.p2}) {
      if (!contains_point(points, *end)) continue;
      kept.push_back(*end);
      *end = {end->x + delta.x, end->y + delta.y};
    }
  }
  return kept;
}

// Whether `p` touches an unselected wire or an unselected instance's pin.
bool connected_outside(EditContext& ec, Point p) {
  for (std::size_t i = 0; i < ec.doc().wires.size(); ++i) {
    const WireRecord& wire = ec.doc().wires[i];
    if (!ec.selection().contains({ObjectKind::wire, i}) && touches(p, wire.p1, wire.p2)) return true;
  }
  for (std::size_t i = 0; i < ec.doc().instances.size(); ++i) {
    if (ec.selection().contains({ObjectKind::instance, i})) continue;
    const InstanceRecord& instance = ec.doc().instances[i];
    if (contains_point(pin_points(instance, ec.symbol(instance)), p)) return true;
  }
  return false;
}

// "Move with wire insertion": a wire (two for a diagonal move: horizontal,
// then vertical) from each pin that leaves a connection to its new place.
std::vector<WireRecord> inserted_wires(EditContext& ec, Point delta, const std::vector<Point>& stretched) {
  std::vector<WireRecord> wires;
  for (const Point p : selected_pins(ec)) {
    if (contains_point(stretched, p) || !connected_outside(ec, p)) continue;
    const Point to{p.x + delta.x, p.y + delta.y};
    const Point corner{to.x, p.y};
    if (delta.x != 0.0 && delta.y != 0.0) {
      wires.push_back({p, corner, {}});
      wires.push_back({corner, to, {}});
    } else {
      wires.push_back({p, to, {}});
    }
  }
  return wires;
}

void apply_to_selection(EditContext& ec, const Motion& motion) {
  for (const ObjectRef& ref : ec.selection()) apply(motion, ec.doc(), ref);
}

cmd::CommandResult move_body(EditContext& ec, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-stretch"}, FlagSpec{"-insertwires"}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, "move");
  if (!parsed) return fail(parsed.error());
  const auto d = numbers(parsed.value().positional, 0, 2);
  if (parsed.value().positional.size() != 2 || !d) return usage("move");
  const Point delta{(*d)[0], (*d)[1]};
  if (ec.selection().empty() || (delta.x == 0.0 && delta.y == 0.0)) return cmd::CommandResult::success();
  std::vector<Point> stretched;
  if (parsed.value().has(0)) stretched = stretch(ec, delta);
  std::vector<WireRecord> added;
  if (parsed.value().has(1)) added = inserted_wires(ec, delta, stretched);
  apply_to_selection(ec, Motion{Motion::Kind::translate, delta, {}});
  for (WireRecord& wire : added) ec.doc().wires.push_back(std::move(wire));
  ec.changed();
  return cmd::CommandResult::success();
}

// Appends a displaced copy of `ref` and returns the copy's reference.
template <typename T>
ObjectRef copy_into(std::vector<T>& list, ObjectRef ref) {
  T item = list[ref.index];  // copied first: push_back may reallocate
  list.push_back(std::move(item));
  return {ref.kind, list.size() - 1};
}

ObjectRef duplicate(Document& doc, ObjectRef ref) {
  switch (ref.kind) {
    case ObjectKind::line:
      return copy_into(doc.lines, ref);
    case ObjectKind::box:
      return copy_into(doc.boxes, ref);
    case ObjectKind::arc:
      return copy_into(doc.arcs, ref);
    case ObjectKind::polygon:
      return copy_into(doc.polygons, ref);
    case ObjectKind::text:
      return copy_into(doc.texts, ref);
    case ObjectKind::wire:
      return copy_into(doc.wires, ref);
    case ObjectKind::instance:
      return copy_into(doc.instances, ref);
  }
  return ref;
}

cmd::CommandResult copy_body(EditContext& ec, std::span<const std::string> args) {
  const auto d = numbers(args, 0, 2);
  if (args.size() != 2 || !d) return usage("copy");
  if (ec.selection().empty()) return cmd::CommandResult::success();
  const Motion motion{Motion::Kind::translate, {(*d)[0], (*d)[1]}, {}};
  InstanceNamer names(ec.doc(), &ec.session().bus_budget);
  Selection copies;
  for (const ObjectRef& ref : ec.selection()) {
    const ObjectRef copy = duplicate(ec.doc(), ref);
    apply(motion, ec.doc(), copy);
    if (copy.kind == ObjectKind::instance) {
      std::string& props = ec.doc().instances[copy.index].props;
      props = names.unique_properties(props);
    }
    copies.insert(copy);
  }
  ec.selection() = std::move(copies);
  ec.changed();
  return cmd::CommandResult::success();
}

Point selection_centre(EditContext& ec) {
  Rect r;
  for (const ObjectRef& ref : ec.selection()) {
    const Symbol* symbol = ref.kind == ObjectKind::instance ? &ec.symbol(ec.doc().instances[ref.index]) : nullptr;
    r.add(bounds(ec.doc(), ref, symbol));
  }
  return snap_point({(r.min.x + r.max.x) / 2.0, (r.min.y + r.max.y) / 2.0}, ec.session().edit.snap.snap);
}

// `rotate`, `flip` and `flipv`: `steps` of `kind` (flipv is a flip and two
// rotation steps).
cmd::CommandResult turn_body(EditContext& ec, std::span<const std::string> args, std::string_view option,
                             std::span<const Motion::Kind> steps) {
  static constexpr std::array kFlags{FlagSpec{"-inplace"}};
  const auto parsed = cmdimpl::parse_args(args, kFlags, option);
  if (!parsed) return fail(parsed.error());
  const auto& positional = parsed.value().positional;
  const auto xy = numbers(positional, 0, 2);
  if ((positional.size() != 0 && positional.size() != 2) || (!positional.empty() && !xy)) return usage(option);
  if (ec.selection().empty()) return cmd::CommandResult::success();
  const bool in_place = parsed.value().has(0);
  const Point pivot = xy ? Point{(*xy)[0], (*xy)[1]} : selection_centre(ec);
  for (const ObjectRef& ref : ec.selection()) {
    const Point p = in_place ? anchor(ec.doc(), ref) : pivot;
    for (const Motion::Kind kind : steps) apply(Motion{kind, {}, p}, ec.doc(), ref);
  }
  ec.changed();
  return cmd::CommandResult::success();
}

constexpr std::array kRotate{Motion::Kind::rotate};
constexpr std::array kFlip{Motion::Kind::flip};
constexpr std::array kFlipV{Motion::Kind::flip, Motion::Kind::rotate, Motion::Kind::rotate};

cmd::CommandResult align_body(EditContext& ec, std::span<const std::string> args) {
  if (!args.empty()) return usage("align");
  if (ec.selection().empty()) return cmd::CommandResult::success();
  for (const ObjectRef& ref : ec.selection()) align(ec.doc(), ref, ec.session().edit.snap.snap);
  ec.changed();
  return cmd::CommandResult::success();
}

}  // namespace

cmd::CommandResult op_move(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, move_body);
}

cmd::CommandResult op_copy(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, copy_body);
}

cmd::CommandResult op_rotate(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, [](EditContext& ec, auto args) { return turn_body(ec, args, "rotate", kRotate); });
}

cmd::CommandResult op_flip(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, [](EditContext& ec, auto args) { return turn_body(ec, args, "flip", kFlip); });
}

cmd::CommandResult op_flipv(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, [](EditContext& ec, auto args) { return turn_body(ec, args, "flipv", kFlipV); });
}

cmd::CommandResult op_align(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, align_body);
}

}  // namespace hefesto::schem::editimpl
