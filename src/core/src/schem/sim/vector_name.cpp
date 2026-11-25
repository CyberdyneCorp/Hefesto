// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Simulator vector names" (capability schematic-simulation-integration;
// design D13, M8 part: the name mapping). A net seen at a hierarchy path is
// resolved upwards while it is a port of the schematic shown at that level,
// through the session's descents (Session::edit.hierarchy) that match the
// path: the parent's connectivity (schem/net name_nets) gives the net on the
// instance's pin. The result is prefixed with the path components below the
// level reached (each starting with `x`) and lowercased.
//
// Limits (recorded in tasks.md 11.1): a port of a vector instance
// (`xinv[3:0]`) is not resolved bit by bit (the name stays at its level), and
// a path that does not follow the session's descents is only prefixed.
//
// Owner: SIMINT.

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/connectivity.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sim/deck.hpp"
#include "hefesto/schem/sym/symbol.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "schem/sim/text.hpp"

namespace hefesto::schem {

namespace {

// The symbols of one vector_name() call: a fresh cache over the session's
// search path, as netlist(const Session&, ...) uses.
class Symbols {
 public:
  explicit Symbols(const Session& session)
      : fs_(session.files != nullptr ? *session.files : local_),
        context_{session.search_path, fs_, nullptr, session.compat == nullptr || session.compat->xschem_compat} {}

  const Symbol& of(const InstanceRecord& record, const Document& from) {
    return cache_.resolve(Reference{record.symbol}, from, context_);
  }

 private:
  platform::LocalFileSystem local_;
  platform::FileSystem& fs_;
  ResolveContext context_;
  SymbolCache cache_;
};

// The connectivity input of one instance (the reduction schem/edit makes
// for net highlighting, without parent substitution).
NetInstance net_instance(const InstanceRecord& record, const Symbol& symbol, BusBudget& budget) {
  NetInstance n;
  n.name = get_token(record.props, "name").value_or(std::string{});
  n.type = symbol_type(symbol);
  const auto ignore = get_token(record.props, "spice_ignore");
  n.ignored = symbol.placeholder || (ignore && is_true(*ignore));
  n.shorted = ignore == "short";
  if (is_naming_type(n.type) || n.type == "bus_tap") n.lab = get_token(record.props, "lab");
  auto global = get_token(record.props, "global");
  if (!global) global = get_token(symbol.property(), "global");
  n.global = global && (is_true(*global) || *global == "ground");
  n.vector_width = bus_width(n.name, &budget);
  const Placement placement{record.origin, record.rotation, record.flip};
  for (const PinIndex k : pin_order(symbol)) {
    const Pin& pin = symbol.pins[k];
    n.pins.push_back({pin.name, transform_point(placement, pin_point(pin.box)), bus_width(pin.name, &budget)});
  }
  return n;
}

struct LevelNets {
  SchematicNets nets;
  NetNaming naming;
};

LevelNets level_nets(const Document& document, Symbols& symbols) {
  LevelNets level;
  BusBudget budget;  // one connectivity pass, as one netlist run
  level.nets.bus_budget = &budget;
  for (const WireRecord& wire : document.wires) {
    const auto ignore = get_token(wire.props, "spice_ignore");
    level.nets.wires.push_back({wire.p1, wire.p2, ignore && is_true(*ignore)});
  }
  for (const InstanceRecord& record : document.instances) {
    level.nets.instances.push_back(net_instance(record, symbols.of(record, document), budget));
  }
  level.naming = name_nets(level.nets);
  level.nets.bus_budget = nullptr;
  return level;
}

const Document* document_of(const Session& session, DocumentId id) {
  const auto it = std::find_if(session.documents.begin(), session.documents.end(),
                               [id](const OpenDocument& open) { return open.id == id; });
  return it == session.documents.end() ? nullptr : &it->document;
}

bool is_port_type(std::string_view type) {
  return type == "ipin" || type == "opin" || type == "iopin";
}

bool is_port(const LevelNets& level, std::string_view net) {
  return std::any_of(level.nets.instances.begin(), level.nets.instances.end(), [net](const NetInstance& n) {
    return is_port_type(n.type) && n.lab && *n.lab == net;
  });
}

// The net on pin `pin` of the scalar instance `instance` of `parent`; empty
// when there is none (or the instance is a vector instance).
std::string net_on_pin(const LevelNets& parent, std::string_view instance, std::string_view pin) {
  for (std::size_t i = 0; i < parent.nets.instances.size() && i < parent.naming.pin_nets.size(); ++i) {
    const NetInstance& n = parent.nets.instances[i];
    if (n.name != instance || n.vector_width != 1) continue;
    for (std::size_t p = 0; p < n.pins.size() && p < parent.naming.pin_nets[i].size(); ++p) {
      if (n.pins[p].name == pin) return parent.naming.pin_nets[i][p];
    }
  }
  return {};
}

std::string without_hash(std::string net) {
  if (net.starts_with('#')) net.erase(0, 1);
  return net;
}

// The descents of the session that follow `path` from the top; all of
// them, or none when the session is elsewhere.
std::vector<const HierFrame*> descents(const Session& session, const HierPath& path) {
  const std::vector<HierFrame>& stack = session.edit.hierarchy;
  std::vector<const HierFrame*> frames;
  for (std::size_t i = 0; i < path.elements.size() && i < stack.size(); ++i) {
    if (stack[i].element != path.elements[i]) break;
    frames.push_back(&stack[i]);
  }
  if (frames.size() != path.elements.size()) frames.clear();
  return frames;
}

// Resolves `net` upwards from the level `frames.size()`; returns the level
// reached and leaves the resolved name in `net`.
class Resolver {
 public:
  Resolver(const Session& session, std::vector<const HierFrame*> frames)
      : session_(session), symbols_(session), frames_(std::move(frames)) {}

  std::size_t resolve(std::string& net) {
    std::size_t level = frames_.size();
    if (level > 0) {
      if (const Document* shown = document_of(session_, frames_.back()->child)) (void)nets(*shown);
    }
    while (level > 0 && !global(net)) {
      const HierFrame& frame = *frames_[level - 1];
      const Document* parent = document_of(session_, frame.parent);
      const Document* child = document_of(session_, frame.child);
      if (parent == nullptr || child == nullptr) break;
      std::optional<std::string> up = step(*parent, *child, frame, net);
      if (!up) break;
      net = std::move(*up);
      --level;
    }
    return level;
  }

  [[nodiscard]] bool global(std::string_view net) const { return net == "0" || globals_.contains(std::string{net}); }

 private:
  // The parent-level name of `net` (rule 2), or nullopt when it stays.
  std::optional<std::string> step(const Document& parent, const Document& child, const HierFrame& frame,
                                  const std::string& net) {
    if (net.starts_with('@')) return attribute(parent, frame.instance, net.substr(1));
    const LevelNets& inside = nets(child);
    if (!is_port(inside, net)) return std::nullopt;
    const std::string outside = without_hash(net_on_pin(nets(parent), frame.instance, net));
    return outside.empty() ? std::nullopt : std::optional<std::string>{outside};
  }

  static std::optional<std::string> attribute(const Document& parent, std::string_view instance,
                                              std::string_view key) {
    for (const InstanceRecord& record : parent.instances) {
      if (get_token(record.props, "name") == instance) return get_token(record.props, key);
    }
    return std::nullopt;
  }

  const LevelNets& nets(const Document& document) {
    auto [it, added] = cache_.try_emplace(&document);
    if (added) {
      it->second = level_nets(document, symbols_);
      globals_.insert(it->second.naming.globals.begin(), it->second.naming.globals.end());
    }
    return it->second;
  }

  const Session& session_;
  Symbols symbols_;
  std::vector<const HierFrame*> frames_;
  std::map<const Document*, LevelNets> cache_;
  std::set<std::string> globals_;
};

// Rule 3: the path components below `level`, each starting with `x`, each
// followed by `.`.
std::string prefix(const HierPath& path, std::size_t level) {
  std::string out;
  for (std::size_t i = 0; i < level && i < path.elements.size(); ++i) {
    const std::string& element = path.elements[i];
    if (element.empty() || (element.front() != 'x' && element.front() != 'X')) out += 'x';
    out += element + '.';
  }
  return out;
}

}  // namespace

std::string vector_name(const Session& session, const HierPath& path, std::string_view net) {
  std::string name{net};
  std::vector<const HierFrame*> frames = descents(session, path);
  const bool resolvable = !frames.empty();
  Resolver resolver(session, std::move(frames));
  const std::size_t level = resolvable ? resolver.resolve(name) : path.elements.size();
  if (resolver.global(name)) return sim_text::lower(name);
  return sim_text::lower(prefix(path, level) + without_hash(name));
}

std::string device_vector_name(const HierPath& path, std::string_view device) {
  return sim_text::lower(prefix(path, path.elements.size()) + std::string{device});
}

}  // namespace hefesto::schem
