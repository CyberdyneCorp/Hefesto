// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Flat netlist" (capability schematic-netlisting; Q-SCHEM-15, behavioural,
// design D9/D10): the subcircuit bodies substituted into the post-processed
// hierarchical netlist. Written from SPICE semantics: a device's nodes are
// its first positional fields (2 for R C L D V I B F H, 3 for J, 3 or 4 for
// Q, 4 for M E G, every positional field but the last for X and the other
// kinds), the rest are its arguments; an X line whose last positional field
// names a subcircuit defined in the netlist is replaced by that subcircuit's
// body.
//
// Owner: NETLIST.

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/schem/net/netlist.hpp"
#include "line_fields.hpp"

namespace hefesto::schem {

namespace {

using detail::ascii_lower;
using detail::iequals;
using detail::split_fields;

/// One `.subckt` definition of the hierarchical netlist.
struct Definition {
  std::vector<std::string> ports;                  // lower case
  std::map<std::string, std::string> defaults;     // header parameters, keys lower case
  std::vector<ProcessedLine> body;
};

using Definitions = std::map<std::string, Definition>;  // by lower-case name

bool is_parameter(std::string_view field) noexcept {
  return field.find('=') != std::string_view::npos;
}

std::string_view first_field(std::string_view line) {
  const auto fields = split_fields(line);
  return fields.empty() ? std::string_view{} : fields.front();
}

/// Nodes of a device line ("Flat netlist"; the same table as the canonical
/// comparator of design D10).
std::size_t node_count(char kind, std::size_t positional) noexcept {
  switch (detail::ascii_lower(kind)) {
    case 'r': case 'c': case 'l': case 'd': case 'v': case 'i': case 'b': case 'f': case 'h':
      return 2;
    case 'm': case 'e': case 'g':
      return 4;
    case 'j':
      return 3;
    case 'q':
      return positional >= 5 ? 4 : 3;
    default:
      return positional > 0 ? positional - 1 : 0;
  }
}

/// A device line split into its name, positional fields and `k=v` fields
/// (in their original order, kept in `fields`).
struct DeviceLine {
  std::vector<std::string> fields;
  std::size_t positional = 0;  // positional fields after the name
  std::size_t nodes = 0;

  /// The last positional field: the subcircuit of an X line.
  [[nodiscard]] std::optional<std::string> last_positional() const {
    for (auto it = fields.rbegin(); it + 1 != fields.rend(); ++it) {
      if (!is_parameter(*it)) {
        return *it;
      }
    }
    return std::nullopt;
  }
};

DeviceLine parse_device(std::string_view line) {
  DeviceLine device;
  for (const std::string_view field : split_fields(line)) {
    device.fields.emplace_back(field);
  }
  device.positional = static_cast<std::size_t>(
      std::count_if(device.fields.begin() + 1, device.fields.end(), [](const std::string& f) { return !is_parameter(f); }));
  device.nodes = std::min(node_count(device.fields.front().front(), device.positional), device.positional);
  return device;
}

/// The definition an X line instantiates, or nullptr.
const Definition* instantiated(const DeviceLine& device, const Definitions& definitions) {
  if (detail::ascii_lower(device.fields.front().front()) != 'x') {
    return nullptr;
  }
  const auto cell = device.last_positional();
  if (!cell) {
    return nullptr;
  }
  const auto it = definitions.find(ascii_lower(*cell));
  return it == definitions.end() ? nullptr : &it->second;
}

/// Splits the hierarchical lines into the top part (up to and including the
/// top `.ends` line) and the definitions after it; the `* expanding` comment
/// and its `sym_path`/`sch_path` lines go with their definition.
class NetlistSplitter {
 public:
  explicit NetlistSplitter(std::span<const ProcessedLine> lines) {
    for (const ProcessedLine& line : lines) {
      feed(line);
    }
  }

  std::vector<ProcessedLine> top;
  Definitions definitions;
  std::set<std::string> globals{"0"};

 private:
  void feed(const ProcessedLine& line) {
    const std::string head = ascii_lower(first_field(line.text));
    if (head == ".global") {
      const auto names = split_fields(line.text);
      for (std::size_t i = 1; i < names.size(); ++i) {
        globals.insert(ascii_lower(names[i]));
      }
    }
    if (current_ != nullptr) {
      in_definition(line, head);
    } else if (!top_done_) {
      top.push_back(line);
      top_done_ = head == ".ends" || head == "**.ends";
    } else if (head == ".subckt") {
      open(line.text);
    } else if (!definition_comment(line, head)) {
      top.push_back(line);
    }
  }

  void in_definition(const ProcessedLine& line, const std::string& head) {
    if (head == ".ends") {
      current_ = nullptr;
    } else {
      current_->body.push_back(line);
    }
  }

  bool definition_comment(const ProcessedLine& line, const std::string& head) {
    if (line.text.starts_with("* expanding   symbol:")) {
      after_expanding_ = true;
      return true;
    }
    const bool path = head == "**" && (line.text.starts_with("** sym_path:") || line.text.starts_with("** sch_path:"));
    if (after_expanding_ && path) {
      return true;
    }
    after_expanding_ = false;
    return false;
  }

  void open(std::string_view header) {
    const auto fields = split_fields(header);
    if (fields.size() < 2) {
      return;
    }
    Definition& definition = definitions[ascii_lower(fields[1])];
    definition = Definition{};
    for (const std::string_view field : std::span{fields}.subspan(2)) {
      const std::size_t eq = field.find('=');
      if (eq != std::string_view::npos) {
        definition.defaults[ascii_lower(field.substr(0, eq))] = std::string{field.substr(eq + 1)};
      } else if (!iequals(field, "params:")) {
        definition.ports.push_back(ascii_lower(field));
      }
    }
    current_ = &definition;
    after_expanding_ = false;
  }

  Definition* current_ = nullptr;
  bool top_done_ = false;
  bool after_expanding_ = false;
};

/// Where one subcircuit body is substituted: its path, its port nets in the
/// parent, its parameter values.
struct Scope {
  std::string path;  // instance names from the top joined with `_`
  std::map<std::string, std::string> ports;   // lower-case port -> parent net
  std::map<std::string, std::string> values;  // lower-case parameter -> value
  int depth = 0;
};

class Flattener {
 public:
  Flattener(const Definitions& definitions, const std::set<std::string>& globals)
      : definitions_{definitions}, globals_{globals} {}

  void line(const ProcessedLine& line, const Scope* scope) {
    const std::string_view head = first_field(line.text);
    if (line.architecture_code || head.empty() || head.starts_with('*') || head.starts_with('.')) {
      out_.push_back(line);
      return;
    }
    DeviceLine device = parse_device(line.text);
    if (scope != nullptr) {
      localise(device, *scope);
    }
    const Definition* definition = instantiated(device, definitions_);
    const int depth = scope != nullptr ? scope->depth + 1 : 1;
    if (definition == nullptr || depth > kMaxFlatDepth) {
      out_.push_back({join(device.fields), false});
      return;
    }
    const Scope inner = enter(device, *definition, scope, depth);
    for (const ProcessedLine& body : definition->body) {
      this->line(body, &inner);
    }
  }

  std::vector<ProcessedLine> take() { return std::move(out_); }

 private:
  static std::string join(const std::vector<std::string>& fields) {
    std::string out;
    for (const std::string& field : fields) {
      if (!out.empty()) {
        out += ' ';
      }
      out += field;
    }
    return out;
  }

  std::string node(const std::string& net, const Scope& scope) const {
    const std::string lower = ascii_lower(net);
    if (globals_.count(lower) != 0) {
      return net;
    }
    const auto port = scope.ports.find(lower);
    return port != scope.ports.end() ? port->second : scope.path + "_" + net;
  }

  /// A device of a substituted body: name suffixed, nodes mapped,
  /// parameters whose whole value names a subcircuit parameter replaced.
  void localise(DeviceLine& device, const Scope& scope) const {
    device.fields.front() += "_" + scope.path;
    std::size_t positional = 0;
    for (std::size_t i = 1; i < device.fields.size(); ++i) {
      std::string& field = device.fields[i];
      if (!is_parameter(field)) {
        if (positional++ < device.nodes) {
          field = node(field, scope);
        }
        continue;
      }
      const std::size_t eq = field.find('=');
      const auto value = scope.values.find(ascii_lower(std::string_view{field}.substr(eq + 1)));
      if (value != scope.values.end()) {
        field = field.substr(0, eq + 1) + value->second;
      }
    }
  }

  /// The scope of the body an X line (already localised) instantiates.
  static Scope enter(const DeviceLine& device, const Definition& definition, const Scope* parent, int depth) {
    Scope scope;
    std::string name = device.fields.front();
    if (parent != nullptr) {
      name.resize(name.size() - parent->path.size() - 1);  // the suffix localise() added
    }
    scope.path = parent != nullptr ? parent->path + "_" + name : name;
    scope.depth = depth;
    for (std::size_t i = 0, k = 0; i + 1 < device.fields.size() && k < definition.ports.size(); ++i) {
      const std::string& field = device.fields[i + 1];
      if (!is_parameter(field) && k < device.nodes) {
        scope.ports[definition.ports[k++]] = field;
      }
    }
    for (const auto& [key, value] : definition.defaults) {
      scope.values[key] = value;
      if (parent != nullptr) {
        if (const auto outer = parent->values.find(ascii_lower(value)); outer != parent->values.end()) {
          scope.values[key] = outer->second;
        }
      }
    }
    for (const std::string& field : device.fields) {
      const std::size_t eq = field.find('=');
      if (eq != std::string::npos) {
        scope.values[ascii_lower(std::string_view{field}.substr(0, eq))] = field.substr(eq + 1);
      }
    }
    return scope;
  }

  const Definitions& definitions_;
  const std::set<std::string>& globals_;
  std::vector<ProcessedLine> out_;
};

}  // namespace

std::vector<ProcessedLine> flatten_netlist(std::span<const ProcessedLine> lines) {
  const NetlistSplitter split{lines};
  Flattener flattener{split.definitions, split.globals};
  for (const ProcessedLine& line : split.top) {
    flattener.line(line, nullptr);
  }
  return flattener.take();
}

}  // namespace hefesto::schem
