// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `global`, `convert` and `flatten` (lvs-setup-file "global and convert
// commands", "flatten command"; flattening as lvs-hierarchy "Flattening
// semantics" names the copied objects). Flattening a cell's contents stops
// at the device limit (kMaxDevicesPerRun, Q-LVS-36) so that a crafted
// hierarchy cannot grow a cell without bound.
//
// Owner: LVSSETUP.

#include <algorithm>
#include <set>

#include "hefesto/lvs/net/names.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

/// `cell` and every cell instanced below it, each once.
std::vector<CellId> hierarchy(const NetlistDb& db, CellId top) {
  std::vector<CellId> order{top};
  std::set<CellId> seen{top};
  for (std::size_t i = 0; i < order.size(); ++i) {
    for (const auto& instance : db.cells.at(order[i]).instances) {
      if (seen.insert(instance.cell).second) {
        order.push_back(instance.cell);
      }
    }
  }
  return order;
}

std::uint64_t make_global(Cell& cell, std::span<const std::string> patterns) {
  std::uint64_t changed = 0;
  for (auto& net : cell.nets) {
    if (net.global) {
      continue;
    }
    if (std::any_of(patterns.begin(), patterns.end(),
                    [&net](const std::string& pattern) { return glob_match(pattern, net.name); })) {
      net.global = true;
      ++changed;
    }
  }
  return changed;
}

/// `convert`: the global nets of `id` become local nets with new ports, and
/// every instance of `id` connects the new pins to its parent's global net.
void convert_cell(NetlistDb& db, CellId id) {
  std::vector<std::string> names;
  {
    Cell& cell = db.cells.at(id);
    for (NetId net = 1; net <= cell.nets.size(); ++net) {
      if (!cell.net(net).global) {
        continue;
      }
      cell.net(net).global = false;
      cell.ports.push_back(Port{cell.net(net).name, net, false});
      names.push_back(cell.net(net).name);
    }
  }
  if (names.empty()) {
    return;
  }
  for (CellId p = 0; p < db.cells.size(); ++p) {
    if (p == id) {
      continue;
    }
    Cell& parent = db.cells.at(p);
    for (auto& instance : parent.instances) {
      if (instance.cell != id) {
        continue;
      }
      for (const auto& name : names) {
        NetId net = parent.find_net(name);
        if (net == kNoNet) {
          net = parent.add_net(name, true);
        }
        instance.connections.push_back(net);
      }
    }
  }
}

std::string file_name(const Runtime& runtime, const Target& target) {
  const Netlist* netlist = runtime.netlist_of(*target.circuit, target.cell);
  return netlist != nullptr ? netlist->name : std::string{};
}

/// Flattens the instances of `parent` that `pick` selects; returns how many.
template <typename Pick>
std::size_t flatten_where(NetlistDb& db, CellId parent, Pick pick) {
  std::size_t count = 0;
  std::size_t index = 0;
  while (index < db.cells.at(parent).instances.size()) {
    const Instance& instance = db.cells.at(parent).instances[index];
    if (instance.cell != parent && pick(instance)) {
      flatten_instance(db, parent, index);
      ++count;
      continue;  // the copies were appended; index now names the next instance
    }
    ++index;
  }
  return count;
}

cmd::CommandResult flatten_contents(Runtime& runtime, const Resolved& cells) {
  for (const auto& target : cells.targets) {
    runtime.print("Flattening contents of cell " + target.get().name);
    NetlistDb& db = target.circuit->db;
    const auto stop = [&db, &target]() { return db.cells.at(target.cell).instances.size() > kMaxDevicesPerRun; };
    flatten_where(db, target.cell, [&](const Instance& instance) {
      const Target klass{target.circuit, target.side, instance.cell, target.number};
      return !stop() && flattenable(runtime, klass);
    });
    if (stop()) {
      return command_error("Flattening stopped: cell " + target.get().name + " exceeds the device limit");
    }
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult flatten_class(Runtime& runtime, const Resolved& cells) {
  for (const auto& target : cells.targets) {
    runtime.print("Flattening instances of " + target.get().name + " in file " + file_name(runtime, target));
    if (!flattenable(runtime, target)) {
      continue;
    }
    NetlistDb& db = target.circuit->db;
    const FileNumber file = target.get().file;
    for (CellId p = 0; p < db.cells.size(); ++p) {
      if (db.cells.at(p).file == file) {
        flatten_where(db, p, [&target](const Instance& i) { return i.cell == target.cell; });
      }
    }
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult flatten_in_parent(Runtime& runtime, const std::string& parent, const Resolved& cells) {
  for (const auto& target : cells.targets) {
    NetlistDb& db = target.circuit->db;
    const auto parent_id = db.find_cell(parent, target.get().file);
    if (!parent_id) {
      return command_error("No such cell.");
    }
    runtime.print("Flattening instances of " + target.get().name + " in cell " + db.cells.at(*parent_id).name +
                  " within file " + file_name(runtime, target));
    const bool allowed = flattenable(runtime, target);
    const std::size_t count =
        flatten_where(db, *parent_id, [&](const Instance& i) { return allowed && i.cell == target.cell; });
    if (count == 0) {
      return command_error("No instances found to flatten.");
    }
  }
  return cmd::CommandResult::success();
}

}  // namespace

bool flattenable(Runtime& runtime, const Target& klass) {
  const Cell& cell = klass.get();
  if (!is_flattenable(cell.device_class) || cell.flags.placeholder) {
    return false;
  }
  const auto& prohibited = runtime.state().no_flatten;
  return std::none_of(prohibited.begin(), prohibited.end(), [&](const SetupCellRef& ref) {
    return ref.circuit == klass.side && names_equal(ref.cell, cell.name, cell.flags.case_insensitive);
  });
}

void flatten_instance(NetlistDb& db, CellId parent_id, std::size_t index) {
  Cell& parent = db.cells.at(parent_id);
  Instance instance = std::move(parent.instances.at(index));
  parent.instances.erase(parent.instances.begin() + static_cast<std::ptrdiff_t>(index));
  const Cell& sub = db.cells.at(instance.cell);
  std::vector<NetId> map(sub.nets.size() + 1, kNoNet);
  for (std::size_t k = 0; k < sub.ports.size(); ++k) {
    const NetId net = sub.ports[k].net;
    if (net != kNoNet && k < instance.connections.size() && map[net] == kNoNet) {
      map[net] = instance.connections[k];
    }
  }
  for (NetId net = 1; net <= sub.nets.size(); ++net) {
    if (map[net] != kNoNet) {
      continue;
    }
    const Net& source = sub.net(net);
    if (source.global) {
      map[net] = parent.net_named(source.name);
      parent.net(map[net]).global = true;
    } else {
      map[net] = parent.add_net(instance.name + "/" + source.name);
    }
  }
  for (const auto& child : sub.instances) {
    Instance copy = child;
    copy.name = instance.name + "/" + child.name;
    for (auto& net : copy.connections) {
      net = net != kNoNet && net < map.size() ? map[net] : kNoNet;
    }
    parent.instances.push_back(std::move(copy));
  }
}

cmd::CommandResult global_command(Runtime& runtime, const Words& words) {
  if (words.size() < 2) {
    return wrong_args("global <valid_cellname> <netname> ...");
  }
  auto cells = runtime.resolve(words[0]);
  if (!cells) {
    return command_error(cells.error().text);
  }
  const std::span<const std::string> patterns{words.begin() + 1, words.end()};
  std::uint64_t changed = 0;
  for (const auto& target : cells.value().targets) {
    NetlistDb& db = target.circuit->db;
    for (const CellId id : hierarchy(db, target.cell)) {
      changed += make_global(db.cells.at(id), patterns);
    }
  }
  return cmd::CommandResult::success(static_cast<std::int64_t>(changed));
}

cmd::CommandResult convert_command(Runtime& runtime, const Words& words) {
  if (words.size() != 1) {
    return wrong_args("convert <valid_cellname>");
  }
  auto cells = runtime.resolve(words[0]);
  if (!cells) {
    return command_error(cells.error().text);
  }
  for (const auto& target : cells.value().targets) {
    convert_cell(target.circuit->db, target.cell);
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult flatten_command(Runtime& runtime, const Words& words) {
  if (words.empty() || words.size() > 3) {
    return wrong_args("flatten [class|prohibit] [<parent>] <valid_cellname>");
  }
  const std::string& kind = words[0];
  const bool keyword = words.size() > 1 && (kind == "class" || kind == "prohibit" || kind == "deny");
  if (!keyword && words.size() > 1) {
    return wrong_args("flatten [class|prohibit] [<parent>] <valid_cellname>");
  }
  auto cells = runtime.resolve(words.back());
  if (!cells) {
    return command_error(cells.error().text);
  }
  if (!keyword) {
    return flatten_contents(runtime, cells.value());
  }
  if (kind == "class") {
    return words.size() == 2 ? flatten_class(runtime, cells.value())
                             : flatten_in_parent(runtime, words[1], cells.value());
  }
  if (words.size() != 2) {
    return wrong_args("flatten prohibit <valid_cellname>");
  }
  for (const auto& target : cells.value().targets) {
    runtime.state().no_flatten.push_back(SetupCellRef{target.side, target.get().name});
    runtime.print("Will not flatten instances of " + target.get().name + " in file " + file_name(runtime, target));
  }
  return cmd::CommandResult::success();
}

}  // namespace hefesto::lvs::setup
