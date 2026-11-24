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
#include <set>
#include <string>

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

cmd::CommandResult select_word(EditContext& ec, std::size_t what, std::span<const std::string> rest) {
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
  if (what == kConnected) select_connected(ec);
  if (what == kClear) ec.selection().clear();
  return cmd::CommandResult::success();
}

cmd::CommandResult select_body(EditContext& ec, std::span<const std::string> args) {
  if (args.empty()) return usage("select");
  const auto what = cmdimpl::match_keyword(args[0], kSelectWhat);
  if (!what) return fail(what.error());
  cmd::CommandResult result = select_word(ec, what.value(), args.subspan(1));
  if (!result.ok()) return result;
  return cmd::CommandResult::success(selection_value(ec.doc(), ec.selection()));
}

template <typename T>
void erase_indices(std::vector<T>& list, const std::vector<std::size_t>& descending) {
  for (const std::size_t i : descending) list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
}

void erase_kind(Document& doc, ObjectKind kind, const std::vector<std::size_t>& descending) {
  switch (kind) {
    case ObjectKind::line:
      return erase_indices(doc.lines, descending);
    case ObjectKind::box:
      return erase_indices(doc.boxes, descending);
    case ObjectKind::arc:
      return erase_indices(doc.arcs, descending);
    case ObjectKind::polygon:
      return erase_indices(doc.polygons, descending);
    case ObjectKind::text:
      return erase_indices(doc.texts, descending);
    case ObjectKind::wire:
      return erase_indices(doc.wires, descending);
    case ObjectKind::instance:
      return erase_indices(doc.instances, descending);
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
    std::sort(indices.rbegin(), indices.rend());
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
