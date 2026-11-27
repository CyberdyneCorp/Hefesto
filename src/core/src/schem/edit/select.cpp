// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `select`, `selection` and `delete` (capability schematic-editing
// "Selection"; schematic-command "Subcommand table"). `select` adds to the
// selection of the active document (the command form of a modifier click);
// `select clear` empties it. Selecting creates no undo step.
//
// Owner: EDIT.

#include <algorithm>
#include <array>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "hefesto/schem/edit/wires.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr std::array<std::string_view, 7> kSelectWhat{"instance", "wire", "text", "all", "connected", "area", "clear"};
enum SelectWhat : std::size_t { kInstance, kWire, kText, kAll, kConnected, kArea, kClear };

constexpr std::array<ObjectKind, 7> kAllKinds{ObjectKind::line, ObjectKind::box,  ObjectKind::arc,
                                              ObjectKind::polygon, ObjectKind::text, ObjectKind::wire,
                                              ObjectKind::instance};

std::string_view kind_name(ObjectKind kind) {
  static constexpr std::array<std::string_view, 7> kNames{"line", "box", "arc", "polygon", "text", "wire", "instance"};
  return kNames.at(static_cast<std::size_t>(kind));
}

cmd::Value selection_value(const Document& doc, const Selection& selection) {
  cmd::Value::List list;
  for (const ObjectRef& ref : selection) {
    cmd::Value::List item{cmd::Value{std::string{kind_name(ref.kind)}}};
    if (ref.kind == ObjectKind::instance) {
      item.emplace_back(get_token(doc.instances[ref.index].props, "name").value_or(std::string{}));
    } else {
      item.emplace_back(static_cast<std::int64_t>(ref.index));
    }
    list.emplace_back(std::move(item));
  }
  return list;
}

cmd::CommandResult select_names(EditContext& ec, std::span<const std::string> names) {
  if (names.empty()) return usage("select");
  for (const std::string& name : names) {
    const auto index = cmdimpl::find_instance(ec.doc(), name);
    if (!index) return fail(cmdimpl::kNoObject, "schematic: no instance \"" + name + "\"");
    ec.selection().insert({ObjectKind::instance, *index});
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult select_indices(EditContext& ec, std::span<const std::string> words, ObjectKind kind) {
  if (words.empty()) return usage("select");
  for (const std::string& word : words) {
    const auto index = index_value(word);
    if (!index || *index >= object_count(ec.doc(), kind)) {
      return fail(cmdimpl::kNoObject, "schematic: no " + std::string{kind_name(kind)} + " \"" + word + "\"");
    }
    ec.selection().insert({kind, *index});
  }
  return cmd::CommandResult::success();
}

void select_all(EditContext& ec) {
  for (const ObjectKind kind : kAllKinds) {
    for (std::size_t i = 0; i < object_count(ec.doc(), kind); ++i) ec.selection().insert({kind, i});
  }
}

const Symbol* symbol_of(EditContext& ec, ObjectRef ref) {
  return ref.kind == ObjectKind::instance ? &ec.symbol(ec.doc().instances[ref.index]) : nullptr;
}

// "Selection": inside the rectangle, or touched by it when it was dragged
// right to left with "select touched" on.
cmd::CommandResult select_area(EditContext& ec, std::span<const std::string> args) {
  const auto c = numbers(args, 0, 4);
  if (args.size() != 4 || !c) return usage("select");
  const Rect area = rect_of({(*c)[0], (*c)[1]}, {(*c)[2], (*c)[3]});
  const bool touched = ec.session().edit.select_touched && (*c)[2] < (*c)[0];
  for (const ObjectKind kind : kAllKinds) {
    for (std::size_t i = 0; i < object_count(ec.doc(), kind); ++i) {
      const ObjectRef ref{kind, i};
      const Symbol* symbol = symbol_of(ec, ref);
      const bool inside = area.contains(bounds(ec.doc(), ref, symbol));
      if (inside || (touched && touches_area(ec.doc(), ref, area, symbol))) ec.selection().insert(ref);
    }
  }
  return cmd::CommandResult::success();
}

std::set<std::string> selected_nets(EditContext& ec, const DocumentNets& d) {
  std::set<std::string> nets;
  for (const ObjectRef& ref : ec.selection()) {
    if (ref.kind == ObjectKind::wire && ref.index < d.naming.wire_nets.size()) {
      nets.insert(d.naming.wire_nets[ref.index]);
    }
    if (ref.kind == ObjectKind::instance && ref.index < d.naming.pin_nets.size()) {
      nets.insert(d.naming.pin_nets[ref.index].begin(), d.naming.pin_nets[ref.index].end());
    }
  }
  nets.erase(std::string{});
  return nets;
}

// "Select connected": the wires, labels and ports on the nets of the
// selected wires, labels and pins.
void select_connected(EditContext& ec) {
  const DocumentNets d = document_nets(ec);
  const std::set<std::string> nets = selected_nets(ec, d);
  for (std::size_t i = 0; i < d.naming.wire_nets.size(); ++i) {
    if (nets.contains(d.naming.wire_nets[i])) ec.selection().insert({ObjectKind::wire, i});
  }
  for (std::size_t i = 0; i < d.naming.pin_nets.size(); ++i) {
    if (!is_naming_type(d.nets.instances[i].type)) continue;
    const auto& pins = d.naming.pin_nets[i];
    if (std::any_of(pins.begin(), pins.end(), [&nets](const std::string& net) { return nets.contains(net); })) {
      ec.selection().insert({ObjectKind::instance, i});
    }
  }
}

// The ends of the selected wires and the pins of the selected instances
// that are not junctions: where `select connected -stopjunction` goes on.
std::vector<Point> open_ends(EditContext& ec, const std::vector<Point>& junctions) {
  std::vector<Point> ends;
  const auto keep = [&](Point p) {
    if (std::find(junctions.begin(), junctions.end(), p) == junctions.end()) ends.push_back(p);
  };
  for (const ObjectRef& ref : ec.selection()) {
    if (ref.kind == ObjectKind::wire) {
      keep(ec.doc().wires[ref.index].p1);
      keep(ec.doc().wires[ref.index].p2);
    }
    if (ref.kind == ObjectKind::instance) {
      const InstanceRecord& instance = ec.doc().instances[ref.index];
      for (const Point p : pin_points(instance, ec.symbol(instance))) keep(p);
    }
  }
  return ends;
}

// "Select connected" with -stopjunction: from the selected objects, the
// wires that touch them end to end, wire after wire, up to the junctions
// (junction_points: three or more wire ends, or an end on another wire's
// interior), which it does not cross.
void select_connected_to_junction(EditContext& ec) {
  const std::vector<Point> junctions = junction_points(ec.doc());
  std::vector<Point> ends = open_ends(ec, junctions);
  while (!ends.empty()) {
    const Point at = ends.back();
    ends.pop_back();
    for (std::size_t i = 0; i < ec.doc().wires.size(); ++i) {
      const WireRecord& w = ec.doc().wires[i];
      if ((!(w.p1 == at) && !(w.p2 == at)) || !ec.selection().insert({ObjectKind::wire, i}).second) continue;
      const Point other = w.p1 == at ? w.p2 : w.p1;
      if (std::find(junctions.begin(), junctions.end(), other) == junctions.end()) ends.push_back(other);
    }
  }
}

// The options of `select`: `-remove` (any form but `clear`) and
// `-stopjunction` (`connected` only).
struct SelectOptions {
  bool remove = false;
  bool stop_junction = false;
  std::vector<std::string> words;  // the arguments without the options
};

SelectOptions select_options(std::span<const std::string> args) {
  SelectOptions options;
  for (const std::string& word : args) {
    if (word == "-remove") {
      options.remove = true;
    } else if (word == "-stopjunction") {
      options.stop_junction = true;
    } else {
      options.words.push_back(word);
    }
  }
  return options;
}

cmd::CommandResult select_word(EditContext& ec, std::size_t what, std::span<const std::string> rest,
                               bool stop_junction) {
  switch (what) {
    case kInstance:
      return select_names(ec, rest);
    case kWire:
      return select_indices(ec, rest, ObjectKind::wire);
    case kText:
      return select_indices(ec, rest, ObjectKind::text);
    case kArea:
      return select_area(ec, rest);
    default:
      break;
  }
  if (!rest.empty()) return usage("select");
  if (what == kAll) select_all(ec);
  if (what == kConnected && stop_junction) select_connected_to_junction(ec);
  if (what == kConnected && !stop_junction) select_connected(ec);
  if (what == kClear) ec.selection().clear();
  return cmd::CommandResult::success();
}

// `-remove`: the objects the same command would select are taken out of the
// selection. Each form picks into an empty selection, but `connected`,
// which starts from the selection (so its starting objects go too).
cmd::CommandResult remove_word(EditContext& ec, std::size_t what, std::span<const std::string> rest,
                               bool stop_junction) {
  const Selection before = ec.selection();
  if (what != kConnected) ec.selection().clear();
  cmd::CommandResult result = select_word(ec, what, rest, stop_junction);
  Selection kept;
  if (result.ok()) {
    std::set_difference(before.begin(), before.end(), ec.selection().begin(), ec.selection().end(),
                        std::inserter(kept, kept.end()));
  }
  ec.selection() = result.ok() ? std::move(kept) : before;
  return result;
}

cmd::CommandResult select_body(EditContext& ec, std::span<const std::string> args) {
  if (args.empty()) return usage("select");
  const auto what = cmdimpl::match_keyword(args[0], kSelectWhat);
  if (!what) return fail(what.error());
  const SelectOptions options = select_options(args.subspan(1));
  const bool bad_remove = options.remove && what.value() == kClear;
  if (bad_remove || (options.stop_junction && what.value() != kConnected)) return usage("select");
  const auto run = options.remove ? remove_word : select_word;
  cmd::CommandResult result = run(ec, what.value(), options.words, options.stop_junction);
  if (!result.ok()) return result;
  return cmd::CommandResult::success(selection_value(ec.doc(), ec.selection()));
}

// Removes the objects at `indices` in one pass (review of PR #101: one
// vector::erase per index made deleting n objects O(n^2)). remove_if tests
// each element at its original place, so its address gives its index.
template <typename T>
void erase_indices(std::vector<T>& list, const std::vector<std::size_t>& indices) {
  std::vector<bool> drop(list.size(), false);
  for (const std::size_t i : indices) {
    if (i < drop.size()) drop[i] = true;
  }
  const T* first = list.data();
  std::erase_if(list, [&](const T& item) { return drop[static_cast<std::size_t>(&item - first)]; });
}

void erase_kind(Document& doc, ObjectKind kind, const std::vector<std::size_t>& indices) {
  switch (kind) {
    case ObjectKind::line:
      return erase_indices(doc.lines, indices);
    case ObjectKind::box:
      return erase_indices(doc.boxes, indices);
    case ObjectKind::arc:
      return erase_indices(doc.arcs, indices);
    case ObjectKind::polygon:
      return erase_indices(doc.polygons, indices);
    case ObjectKind::text:
      return erase_indices(doc.texts, indices);
    case ObjectKind::wire:
      return erase_indices(doc.wires, indices);
    case ObjectKind::instance:
      return erase_indices(doc.instances, indices);
  }
}

cmd::CommandResult delete_body(EditContext& ec, std::span<const std::string> args) {
  if (!args.empty()) return usage("delete");
  if (ec.selection().empty()) return cmd::CommandResult::success();
  for (const ObjectKind kind : kAllKinds) {
    std::vector<std::size_t> indices;
    for (const ObjectRef& ref : ec.selection()) {
      if (ref.kind == kind) indices.push_back(ref.index);
    }
    erase_kind(ec.doc(), kind, indices);
  }
  ec.selection().clear();
  ec.changed();
  return cmd::CommandResult::success();
}

}  // namespace

cmd::Value selected_objects(EditContext& ec) {
  return selection_value(ec.doc(), ec.selection());
}

cmd::CommandResult op_select(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, select_body);
}

cmd::CommandResult op_selection(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_view(ctx, inv, [](EditContext& ec, std::span<const std::string> args) {
    if (!args.empty()) return usage("selection");
    return cmd::CommandResult::success(selected_objects(ec));
  });
}

cmd::CommandResult op_delete(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, delete_body);
}

}  // namespace hefesto::schem::editimpl
