// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The six-word form `equate classes <cellA> <pinsA> <cellB> <pinsB>`
// (lvs-setup-file "equate classes", "Pin lists"): cell B's pins take the
// names of cell A's paired pins; with `{name index}` elements B's pins are
// also renumbered to A's indices, and every instance of B in its database
// follows the new port order.
//
// Owner: LVSSETUP.

#include <algorithm>

#include "hefesto/lvs/net/permute.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

using Items = std::vector<std::vector<std::string>>;

bool black_box_or_subcircuit(DeviceClass c) noexcept {
  return c == DeviceClass::module || c == DeviceClass::subcircuit;
}

diag::Outcome<Items> parse_items(const std::string& text) {
  auto list = split_list(text);
  if (!list) {
    return list.error();
  }
  Items items;
  for (const auto& element : list.value()) {
    auto item = split_list(element);
    if (!item) {
      return item.error();
    }
    items.push_back(std::move(item).value());
  }
  return items;
}

/// The shape errors of the two lists, or empty.
std::string shape_error(const Items& a, const Items& b, const Cell& cell) {
  if (a.size() != b.size()) {
    return "Pin lists are different length, cannot match pins!";
  }
  if (a.size() != cell.ports.size()) {
    return "List length does not match  number of pins in cell.";
  }
  const std::size_t entries = a.empty() ? 1 : a.front().size();
  for (const Items* items : {&a, &b}) {
    for (const auto& item : *items) {
      if (item.empty() || item.size() > 2) {
        return "All list items must have one or two entries.";
      }
      if (item.size() != entries) {
        return "All list items must have the same number of entries.";
      }
    }
  }
  return {};
}

/// Moves the ports of `klass` (and the connections of every instance of it
/// in `db`) so that old position i goes to order[i].
void reorder(NetlistDb& db, CellId klass, const std::vector<std::size_t>& order) {
  Cell& cell = db.cells.at(klass);
  std::vector<Port> ports(cell.ports.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    ports.at(order[i]) = std::move(cell.ports[i]);
  }
  cell.ports = std::move(ports);
  for (auto& parent : db.cells) {
    for (auto& instance : parent.instances) {
      if (instance.cell != klass || instance.connections.size() != order.size()) {
        continue;
      }
      std::vector<NetId> connections(order.size(), kNoNet);
      for (std::size_t i = 0; i < order.size(); ++i) {
        connections.at(order[i]) = instance.connections[i];
      }
      instance.connections = std::move(connections);
    }
  }
}

/// Renames (and with indices, renumbers) the pins of B.
std::string apply(const Target& b, const Items& a_items, const Items& b_items) {
  Cell& cell = b.get();
  const bool indexed = !a_items.empty() && a_items.front().size() == 2;
  std::vector<std::size_t> order(cell.ports.size());
  std::vector<bool> taken(cell.ports.size(), false);
  for (std::size_t i = 0; i < b_items.size(); ++i) {
    const auto position = port_position(cell, b_items[i][0]);
    if (!position) {
      return "No pin order information  for the cell.";
    }
    std::size_t target = *position;
    if (indexed) {
      const auto index = parse_integer(a_items[i][1]);
      if (!index || *index < 0 || static_cast<std::size_t>(*index) >= cell.ports.size() ||
          taken.at(static_cast<std::size_t>(*index))) {
        return "No pin order information  for the cell.";
      }
      target = static_cast<std::size_t>(*index);
      taken.at(target) = true;
    }
    order.at(*position) = target;
    cell.ports.at(*position).name = a_items[i][0];
  }
  if (indexed) {
    reorder(b.circuit->db, b.cell, order);
  }
  return {};
}

}  // namespace

cmd::CommandResult pin_lists(Runtime& runtime, std::span<const std::string> args) {
  auto a = runtime.resolve(args[0]);
  auto b = runtime.resolve(args[2]);
  if (!a || !b) {
    runtime.print("Cell to equate does not exist.");
    return command_error("0");
  }
  const Target& ta = a.value().targets.front();
  const Target& tb = b.value().targets.front();
  const DeviceClass ca = ta.get().device_class;
  const DeviceClass cb = tb.get().device_class;
  if (!black_box_or_subcircuit(ca) || !black_box_or_subcircuit(cb)) {
    return command_error("Device class is not black box or subcircuit!");
  }
  if (ca != cb) {
    return command_error("Device classes are different, cannot match pins!");
  }
  auto a_items = parse_items(args[1]);
  auto b_items = parse_items(args[3]);
  if (!a_items || !b_items) {
    return command_error(!a_items ? a_items.error().text : b_items.error().text);
  }
  std::string error = shape_error(a_items.value(), b_items.value(), tb.get());
  if (error.empty()) {
    error = apply(tb, a_items.value(), b_items.value());
  }
  if (!error.empty()) {
    return command_error(std::move(error));
  }
  return cmd::CommandResult::success(1);
}

}  // namespace hefesto::lvs::setup
