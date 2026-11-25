// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `order <index>` (capability schematic-editing "Object order";
// schematic-command "Subcommand table"): the selected objects of each kind
// move, as one block in their present order, to position <index> of their
// kind's list (the end when <index> is past it). The list order is the file
// order, so it also changes the netlist instance order and the automatic
// net and instance numbering. One undo step; none when nothing moves.
//
// Owner: EDIT.

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

constexpr std::array<ObjectKind, 7> kKinds{ObjectKind::line, ObjectKind::box,  ObjectKind::arc,
                                           ObjectKind::polygon, ObjectKind::text, ObjectKind::wire,
                                           ObjectKind::instance};

// Moves the items at `picked` (ascending) of `list` to start at `index`
// (clamped to the end), keeping their order; returns where they start.
template <typename T>
std::size_t move_block(std::vector<T>& list, const std::vector<std::size_t>& picked, std::size_t index) {
  std::vector<T> block;
  std::vector<T> rest;
  block.reserve(picked.size());
  rest.reserve(list.size() - picked.size());
  std::size_t next = 0;
  for (std::size_t i = 0; i < list.size(); ++i) {
    const bool chosen = next < picked.size() && picked[next] == i;
    if (chosen) ++next;
    (chosen ? block : rest).push_back(std::move(list[i]));
  }
  const std::size_t first = std::min(index, rest.size());
  rest.insert(rest.begin() + static_cast<std::ptrdiff_t>(first), std::make_move_iterator(block.begin()),
              std::make_move_iterator(block.end()));
  list = std::move(rest);
  return first;
}

std::size_t move_kind(Document& doc, ObjectKind kind, const std::vector<std::size_t>& picked, std::size_t index) {
  switch (kind) {
    case ObjectKind::line:
      return move_block(doc.lines, picked, index);
    case ObjectKind::box:
      return move_block(doc.boxes, picked, index);
    case ObjectKind::arc:
      return move_block(doc.arcs, picked, index);
    case ObjectKind::polygon:
      return move_block(doc.polygons, picked, index);
    case ObjectKind::text:
      return move_block(doc.texts, picked, index);
    case ObjectKind::wire:
      return move_block(doc.wires, picked, index);
    case ObjectKind::instance:
      return move_block(doc.instances, picked, index);
  }
  return 0;
}

std::vector<std::size_t> picked_of(const Selection& selection, ObjectKind kind) {
  std::vector<std::size_t> picked;
  for (const ObjectRef& ref : selection) {
    if (ref.kind == kind) picked.push_back(ref.index);  // ascending: a Selection is ordered
  }
  return picked;
}

cmd::CommandResult order_body(EditContext& ec, std::span<const std::string> args) {
  const auto index = args.size() == 1 ? index_value(args[0]) : std::nullopt;
  if (!index) return usage("order");
  Selection moved;
  for (const ObjectKind kind : kKinds) {
    const std::vector<std::size_t> picked = picked_of(ec.selection(), kind);
    if (picked.empty()) continue;
    const std::size_t first = move_kind(ec.doc(), kind, picked, *index);
    for (std::size_t i = 0; i < picked.size(); ++i) moved.insert(ObjectRef{kind, first + i});
  }
  if (moved != ec.selection()) {  // the same indices: every block was already in place
    ec.selection() = std::move(moved);
    ec.changed();
  }
  return cmd::CommandResult::success(selected_objects(ec));
}

}  // namespace

cmd::CommandResult op_order(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  return run_edit(ctx, inv, order_body);
}

}  // namespace hefesto::schem::editimpl
