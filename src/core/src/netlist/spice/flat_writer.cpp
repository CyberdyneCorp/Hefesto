// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The flat SPICE write (writer.hpp): header and scale card, the top-level
// `.subckt` wrapper (Q-NETLIST-37 per mode), node names per dialect with
// the HSPICE dictionary, coupling capacitors in the netlist's
// deterministic order (Q-NETLIST-1), resistors and node lines; plus the
// `.global` card and the subcircuit call line of hierarchical output.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:139-200
// (esFormatSubs), 1048-1180 (the flat branch of CmdExtToSpice), 965-1045
// (the global list), 1600-1790 (subcktVisit), 1857-2110 (topVisit),
// 3937-3993 (spcdevOutNode), 3995-4207 (spccapVisit, spcresistVisit,
// spcsubVisit, spcnodeVisit), 4249-4306 (nodeSpiceName), 4398-4512
// (nodeHspiceName, printSubcktDict) and extflat/EFvisit.c:685-732
// (EFVisitNodes' ground capacitance rule), Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include "netlist/spice/flat_writer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <strings.h>
#include <utility>

#include "netlist/spice/numbers.hpp"

namespace hefesto::netlist::spice {

namespace {

constexpr std::array<std::string_view, 5> kFormatNames{"SPICE2", "SPICE3", "HSPICE", "NGSPICE", "CDL"};
constexpr std::string_view kErrGnd = "errGnd!";

std::string default_convert(std::span<const std::string> components, Dialect format, extflat::NameFlags& flags,
                            const compat::CompatSettings& compat) {
  return extflat::name_to_spice(components, format, flags, compat);
}

std::string joined(std::span<const std::string> components) {
  std::string text;
  for (const std::string& part : components) {
    text += (text.empty() ? "" : "/") + part;
  }
  return text;
}

// Ad-hoc check of the global list (ext2spice.c:982-990).
bool good_global(std::string_view name) {
  if (name.starts_with("err")) {
    return false;
  }
  for (const std::string_view bad : {"error", "Error", "ERROR"}) {
    if (name.find(bad) != std::string_view::npos) {
      return false;
    }
  }
  return strcasecmp(std::string{name}.c_str(), "None") != 0;
}

void add_power_name(std::vector<std::string>& list, const std::optional<std::string>& name) {
  if (name && !name->empty() && name->back() == '!') {
    list.insert(list.begin(), *name);
  }
}

}  // namespace

namespace detail {

bool has_ext_attr(const std::optional<std::string>& attrs, std::string_view kind) {
  if (!attrs) {
    return false;
  }
  std::string lower = *attrs;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find("ext:" + std::string{kind}) != std::string::npos;
}

bool hier_sd_attr(const InputTerminal& term, bool hier_ap) {
  if (term.attributes) {
    if (has_ext_attr(term.attributes, "aph")) {
      return true;
    }
    if (has_ext_attr(term.attributes, "apf")) {
      return false;
    }
  }
  return hier_ap;
}

FlatWriter::FlatWriter(const SpiceInput& input, const SpiceSettings& run_settings, const RunFlags& run_flags,
                       const compat::CompatSettings& compat_settings, extflat::NameFlags& name_flags,
                       NameConverter converter)
    : in(input),
      settings(run_settings),
      flags(run_flags),
      compat(compat_settings),
      names(name_flags),
      convert(std::move(converter)),
      devices(input.devices),
      clients(input.nodes.size()),
      mult(input.devices.size(), 1.0F),
      killed(input.devices.size(), false),
      used_bit(input.resist_classes) {
  if (!convert) {
    const compat::CompatSettings copy = compat;
    convert = [copy](std::span<const std::string> components, Dialect format, extflat::NameFlags& f) {
      return default_convert(components, format, f, copy);
    };
  }
  node_caps.reserve(in.nodes.size());
  for (std::size_t i = 0; i < in.nodes.size(); ++i) {
    node_caps.push_back(in.nodes[i].cap);
    for (const std::string& alias : in.nodes[i].aliases) {
      by_alias.emplace(alias, i);
    }
    by_alias.emplace(joined(in.nodes[i].name), i);
  }
}

std::optional<std::size_t> FlatWriter::find_node(std::string_view alias) const {
  const auto it = by_alias.find(alias);
  if (it == by_alias.end()) {
    return std::nullopt;
  }
  return it->second;
}

void FlatWriter::init_client(std::size_t node) {
  NodeClient& client = clients[node];
  client.exists = true;
  client.visited.assign(used_bit + 1, false);
  if (init_used && !settings.distributed_junctions) {
    client.visited[used_bit] = true;
  }
}

std::string FlatWriter::converted_name(std::size_t node) {
  return convert(in.nodes[node].name, settings.format, names);
}

namespace {

// nodeHspiceName: `x<s>/<leaf>` per prefix, `z@<n>` past 15 characters.
std::string hspice_name(FlatWriter& w, const std::string& text) {
  std::string mapped = text;
  const std::size_t slash = text.rfind('/');
  if (slash != std::string::npos && slash != 0) {
    const std::string prefix = text.substr(0, slash);
    auto it = w.hspice_index.find(prefix);
    if (it == w.hspice_index.end()) {
      it = w.hspice_index.emplace(prefix, w.sbck_num++).first;
      w.hspice_prefixes.emplace_back(prefix, it->second);
    }
    mapped = "x" + std::to_string(it->second) + "/" + text.substr(slash + 1);
  }
  if (mapped.size() > 15) {
    mapped = "z@" + std::to_string(w.node_num++);
  }
  return mapped;
}

}  // namespace

const std::string& FlatWriter::spice_name(std::size_t node) {
  NodeClient& client = clients[node];
  if (!client.exists) {
    init_client(node);
  }
  if (!client.spice_name) {
    if (settings.format == Dialect::spice2) {
      client.spice_name = std::to_string(node_num++);
    } else if (settings.format == Dialect::hspice) {
      client.spice_name = hspice_name(*this, converted_name(node));
    } else {
      client.spice_name = converted_name(node);
    }
  }
  return *client.spice_name;
}

std::string FlatWriter::spice_name_of(std::string_view alias) {
  const auto node = find_node(alias);
  return node ? spice_name(*node) : std::string{kErrGnd};
}

void FlatWriter::mark_used(std::size_t node) {
  NodeClient& client = clients[node];
  if (!client.exists) {
    init_client(node);
  }
  client.connected = true;
  if (settings.distributed_junctions) {
    if (client.widths.empty()) {
      client.widths.assign(used_bit + 1, 0.0F);
    }
    client.widths[used_bit] += 1.0F;
  } else {
    client.visited[used_bit] = true;
  }
}

void FlatWriter::out_node(std::string_view alias) {
  const auto node = find_node(alias);
  if (!node) {
    out += " ";
    out += kErrGnd;
    return;
  }
  out += " " + spice_name(*node);
  mark_used(*node);
}

std::string format_subs(std::string name, const extflat::NameFlags& names) {
  if (!name.empty() && ((names.trim_global && name.back() == '!') || (names.trim_local && name.back() == '#'))) {
    name.pop_back();
  }
  for (char& c : name) {
    if (names.convert_comma && c == ',') {
      c = '|';
    } else if (names.convert_brackets && (c == '[' || c == ']')) {
      c = '_';
    } else if (names.convert_equal && c == '=') {
      c = ':';
    }
  }
  return name;
}

}  // namespace detail

namespace {

using detail::FlatWriter;

bool finite_cap(double cthresh) {
  return std::isfinite(cthresh) && static_cast<float>(cthresh) != 1.0E38F;
}

// The run's EFOutputFlags (ext2spice.c:1080-1088).
void apply_run_flags(extflat::NameFlags& names, Dialect format) {
  names.trim_global = true;
  names.convert_equal = true;
  names.convert_comma = true;
  if (format == Dialect::hspice) {
    names.trim_local = true;
  }
  if (format == Dialect::cdl) {
    names.trim_global = false;
  }
}

void write_header(FlatWriter& w) {
  w.out += "* " + std::string{kFormatNames[static_cast<std::size_t>(w.settings.format)]} + " file created from " +
           w.in.cell + ".ext - technology: " + w.in.tech + "\n\n";
  if (w.settings.scale) {
    w.out += ".option scale=" + detail::si(static_cast<float>(1.0E-6 * detail::dbl(w.in.scale) / 100.0)) + "\n\n";
    w.es_scale = -1.0F;
  } else {
    w.es_scale = static_cast<float>(detail::dbl(w.in.scale) / 100.0);
  }
}

// Q-NETLIST-37: with magic-compat ON flat mode reads without subcircuit
// marking, so only `top on` wraps; with it OFF `top auto` wraps a root
// with ports.
bool wraps(const FlatWriter& w) {
  switch (w.settings.subckt_top) {
    case TopSubckt::on:
      return true;
    case TopSubckt::automatic:
      return !w.compat.magic_compat && !w.in.ports.empty();
    case TopSubckt::off:
      break;
  }
  return false;
}

// topVisit's port numbering and order (ext2spice.c:1897-2060). The entries
// are the root's node-name table entries whose node is a port, in hash
// order; each name's port index is kept by name, as Magic keeps it in the
// EFNodeName shared by the entries.
class PortSorter {
 public:
  explicit PortSorter(FlatWriter& w) : w_(w) {
    for (const RootPort& p : w.in.ports) {
      port_of_.emplace(p.node, p.index);
      for (const extflat::PortName& n : p.rest) {
        port_of_.emplace(n.name, n.index);
      }
      if (p.canonical) {
        port_of_.emplace(p.canonical->name, p.canonical->index);
      }
    }
  }

  [[nodiscard]] const std::map<std::string, int, std::less<>>& ports() const { return port_of_; }

  std::vector<std::string> sorted() {
    number_implicit();
    sorted_.clear();
    for (const RootPort& p : w_.in.ports) {
      if (!place_canonical(p)) {
        place_from_chain(p);
      }
    }
    std::vector<std::string> out;
    out.reserve(sorted_.size());
    for (auto& [index, name] : sorted_) {
      out.push_back(std::move(name));
    }
    return out;
  }

 private:
  static const std::string& canonical(const RootPort& p) { return p.canonical ? p.canonical->name : p.node; }

  // First pass: the highest index, and the entries with none in their
  // chain or on the canonical name, numbered past it in reverse table
  // order (Magic prepends them to a list) unless the root is a black box.
  void number_implicit() {
    std::vector<const RootPort*> implicit;
    for (const RootPort& p : w_.in.ports) {
      bool explicit_port = see(port_of_[canonical(p)]);
      explicit_port = see(port_of_[p.node]) || explicit_port;
      for (const extflat::PortName& n : p.rest) {
        explicit_port = see(port_of_[n.name]) || explicit_port;
      }
      if (!explicit_port) {
        implicit.push_back(&p);
      }
    }
    if (w_.settings.blackbox && w_.in.root_abstract) {
      return;
    }
    for (auto it = implicit.rbegin(); it != implicit.rend(); ++it) {
      port_of_[(*it)->node] = ++portmax_;
    }
  }

  bool see(int index) {
    portmax_ = std::max(portmax_, index);
    return index != -1;
  }

  // The canonical name's index: the port named by the node (or, for an
  // abstract root, by the entry's own name).
  bool place_canonical(const RootPort& p) {
    const int index = port_of_[canonical(p)];
    if (index < 0 || sorted_.contains(index)) {
      return false;
    }
    const auto node = w_.find_node(canonical(p));
    if (!w_.in.root_abstract && !node) {
      return false;
    }
    std::string name = w_.in.root_abstract ? p.node : w_.spice_name(*node);
    if (const auto seen = names_.find(name); seen != names_.end()) {
      port_of_[canonical(p)] = seen->second;  // a repeat of an earlier port
    } else {
      names_.emplace(name, index);
      sorted_[index] = std::move(name);
    }
    return true;
  }

  // No index on the canonical name: the first name of the entry's chain
  // that has one.
  void place_from_chain(const RootPort& p) {
    std::vector<std::string> chain{p.node};
    for (const extflat::PortName& n : p.rest) {
      chain.push_back(n.name);
    }
    for (const std::string& alias : chain) {
      if (place_alias(p, alias)) {
        return;
      }
    }
  }

  bool place_alias(const RootPort& p, const std::string& alias) {
    const int index = port_of_[alias];
    if (index < 0) {
      return false;
    }
    const auto node = w_.find_node(w_.in.root_abstract ? alias : canonical(p));
    if (!node) {
      w_.info("Note:  Port " + alias + " was optimized out of " + w_.in.cell);
      return false;
    }
    std::string name = w_.in.root_abstract ? alias : w_.spice_name(w_.find_node(alias).value_or(*node));
    if (const auto seen = names_.find(name); seen != names_.end()) {
      port_of_[alias] = seen->second;
      return false;
    }
    names_.emplace(name, index);
    if (sorted_.contains(index)) {
      return false;
    }
    sorted_[index] = std::move(name);
    return true;
  }

  FlatWriter& w_;
  std::map<std::string, int, std::less<>> port_of_;
  std::map<std::string, int, std::less<>> names_;  // portNameTable
  // Sparse: the indices come from the `.ext` file's port records, so a
  // list sized by the highest one could be huge. ReadLimits::max_port_index
  // bounds them, so numbering the implicit ports past portmax_ cannot
  // overflow.
  std::map<int, std::string> sorted_;
  int portmax_ = -1;
};

// Appends ` <word>` with Magic's 80-column continuation.
void continued(std::string& out, std::size_t& tchars, const std::string& word) {
  if (tchars > 80) {
    out += "\n+";
    tchars = 1;
  }
  out += " " + word;
  tchars += word.size() + 1;
}

std::map<std::string, int, std::less<>> write_wrapper(FlatWriter& w) {
  const std::string name = subcircuit_name(w.in.cell);
  w.out += ".subckt " + name;
  std::size_t tchars = 8 + name.size();
  PortSorter sorter{w};
  for (const std::string& port : sorter.sorted()) {
    if (!port.empty()) {
      continued(w.out, tchars, port);
    }
  }
  // The cell parameters are looked up under the renamed cell, as Magic does.
  if (const auto* params = w.declared(":" + name)) {
    for (const ParamDecl& p : *params) {
      continued(w.out, tchars, p.name);
    }
  }
  w.out += "\n";
  return sorter.ports();
}

// spccapVisit, or spccapHierVisit, which compares |C| (Q-NETLIST-19).
void write_caps(FlatWriter& w) {
  for (const extflat::FlatCap& c : w.in.caps) {
    const double cap = static_cast<double>(static_cast<float>(c.cap)) / 1000;
    if ((w.hier ? std::fabs(cap) : cap) <= detail::dbl(static_cast<float>(w.settings.cthresh))) {
      continue;
    }
    // Arguments evaluated right to left, as Magic's GCC build does.
    const std::string second = w.spice_name_of(c.b);
    const std::string first = w.spice_name_of(c.a);
    w.out += "C" + std::to_string(w.cap_num++) + " " + first + " " + second + " " +
             detail::si(static_cast<float>(1.0E-15 * cap)) + "\n";
  }
}

void write_resists(FlatWriter& w) {
  for (const extflat::FlatResist& r : w.in.resists) {
    const std::string second = w.spice_name_of(r.b);
    const std::string first = w.spice_name_of(r.a);
    w.out += "R" + std::to_string(w.res_num++) + " " + first + " " + second + " " +
             detail::format_g(detail::dbl(static_cast<float>(r.resistance)) / 1000.) + "\n";
    for (const std::string* alias : {&r.a, &r.b}) {
      if (const auto node = w.find_node(*alias)) {
        w.mark_used(*node);
      }
    }
  }
}

bool is_gnd(const FlatWriter& w, const InputNode& node) {
  if (node.name.size() != 1) {
    return false;
  }
  return std::find(w.in.gnd_names.begin(), w.in.gnd_names.end(), node.name.front()) != w.in.gnd_names.end();
}

// EFVisitNodes: the ground capacitance of the substrate node (or of GND in
// compat substrate mode) is passed as zero.
double visit_cap(const FlatWriter& w, std::size_t index) {
  const InputNode& node = w.in.nodes[index];
  const bool zero = w.in.compat_substrate ? is_gnd(w, node) : node.substrate;
  return zero ? 0.0 : detail::dbl(w.node_caps[index]);
}

// The `connected` test of spcnodeVisit (Q-NETLIST-19 (d): flat rule). With
// magic-compat ON it is Magic's: a node is connected when its client exists
// without the used bit (ext2spice.c:4155-4161), which every device
// terminal sets; with it OFF it is the documented rule (touched by a
// device terminal or a resistor).
bool connected(const FlatWriter& w, std::size_t index) {
  const detail::NodeClient& client = w.clients[index];
  if (!w.compat.magic_compat && !w.hier) {
    return client.connected;
  }
  if (!client.exists) {
    return false;
  }
  if (w.hier) {
    // spcnodeHierVisit: any terminal or resistor touched the node.
    return w.settings.distributed_junctions
               ? !client.widths.empty()
               : std::any_of(client.visited.begin(), client.visited.end(), [](bool b) { return b; });
  }
  if (w.settings.distributed_junctions) {
    return !client.widths.empty();
  }
  return !client.visited[w.used_bit];
}

void write_node_attrs(FlatWriter& w, const InputNode& node, const std::string& nsn) {
  if (node.attributes.empty() || w.settings.no_attributes) {
    return;
  }
  const bool ng = w.settings.format == Dialect::ngspice;
  w.out += std::string{ng ? " ; " : ""} + "**nodeattr " + nsn + " :";
  std::string_view sep = " ";
  for (const std::string& a : node.attributes) {
    w.out += std::string{sep} + a;
    sep = ",";
  }
  w.out += "\n";
}

// The node's capacitance line, compared as |C| in hierarchical output
// (Q-NETLIST-19).
void write_node_cap(FlatWriter& w, std::size_t index, const std::string& nsn, const std::string& cap_node,
                    bool is_connected) {
  const double cap = (w.hier ? detail::dbl(w.node_caps[index]) : visit_cap(w, index)) / 1000;
  if ((w.hier ? std::fabs(cap) : cap) <= detail::dbl(static_cast<float>(w.settings.cthresh))) {
    return;
  }
  w.out += "C" + std::to_string(w.cap_num++) + " " + nsn + " " + cap_node + " " +
           detail::si(static_cast<float>(1.0E-15 * cap));
  if (!is_connected) {
    w.out += std::string{w.settings.format == Dialect::ngspice ? " ;" : ""} + " **FLOATING";
  }
  w.out += "\n";
}

void write_node(FlatWriter& w, std::size_t index, const std::string& cap_node) {
  bool is_connected = connected(w, index);
  if (!is_connected && w.flags.device_nodes_only) {
    return;
  }
  const InputNode& node = w.in.nodes[index];
  if (!is_connected && node.port) {
    is_connected = true;
  }
  const std::string nsn = w.spice_name(index);
  const bool ng = w.settings.format == Dialect::ngspice;
  if (w.settings.format == Dialect::spice2 || (w.settings.format == Dialect::hspice && nsn.starts_with("z@"))) {
    w.out += std::string{ng ? "; " : ""} + "** " + w.converted_name(index) + " == " + nsn + "\n";
  }
  write_node_cap(w, index, nsn, cap_node, is_connected);
  write_node_attrs(w, node, nsn);
}

void write_nodes(FlatWriter& w) {
  std::string cap_node = "0";
  for (std::size_t i = 0; i < w.in.nodes.size(); ++i) {
    if (w.in.nodes[i].substrate) {
      cap_node = w.spice_name(i);
      break;
    }
  }
  for (std::size_t i = 0; i < w.in.nodes.size(); ++i) {
    write_node(w, i, cap_node);
  }
}

void write_dictionary(FlatWriter& w) {
  w.out += "\n** hspice subcircuit dictionary\n";
  for (const auto& [prefix, number] : w.hspice_prefixes) {
    w.out += "* x" + std::to_string(number) + "\t" + prefix + "\n";
  }
}

void write_devices(FlatWriter& w) {
  w.init_used = !w.settings.distributed_junctions;
  if (w.merging()) {
    w.merge_devices();
    w.info("Devs merged: " + std::to_string(w.merged));
  } else if (w.settings.distributed_junctions) {
    for (const InputDevice& dev : w.devices) {
      w.distribute_junction(dev);
    }
  }
  for (std::size_t i = 0; i < w.devices.size(); ++i) {
    w.device(i);
  }
  w.init_used = false;
}

}  // namespace

namespace detail {

WriteResult FlatWriter::run() {
  if (!in.style_matches) {
    warn("Warning:  Current extraction style does not match .ext file!");
    warn("Area/Perimeter values and parasitic values will be zero.");
  }
  write_header(*this);
  apply_run_flags(names, settings.format);
  const bool wrapper = wraps(*this);
  if (wrapper) {
    (void)write_wrapper(*this);
  }
  write_devices(*this);
  if (finite_cap(settings.cthresh)) {
    write_caps(*this);
  }
  write_resists(*this);
  write_nodes(*this);
  if (wrapper) {
    out += ".ends\n";
  }
  if (settings.format == Dialect::hspice) {
    write_dictionary(*this);
  }
  return WriteResult{std::move(out), std::move(messages)};
}

void FlatWriter::write_file_header() {
  write_header(*this);
  apply_run_flags(names, settings.format);
}

std::map<std::string, int, std::less<>> FlatWriter::write_subckt_line() { return write_wrapper(*this); }

// esHierVisit after the calls: devices, resistors, couplings (always
// visited; |C| against the threshold), and the node lines, which follow
// Q-NETLIST-20: with magic-compat ON only when a `substrate` record was
// read (EFCompat FALSE), with it OFF always.
void FlatWriter::write_section_body() {
  write_devices(*this);
  write_resists(*this);
  write_caps(*this);
  if (in.compat_substrate && compat.magic_compat) {
    return;
  }
  std::string cap_node = "0";
  for (std::size_t i = 0; i < in.nodes.size(); ++i) {
    if (in.nodes[i].substrate) {
      cap_node = spice_name(i);
      break;
    }
  }
  for (std::size_t i = 0; i < in.nodes.size(); ++i) {
    write_node(*this, i, cap_node);
  }
}

}  // namespace detail

WriteResult write_flat(const SpiceInput& input, const SpiceSettings& settings, const RunFlags& flags,
                       const compat::CompatSettings& compat, extflat::NameFlags& names,
                       const NameConverter& convert) {
  detail::FlatWriter writer{input, settings, flags, compat, names, convert};
  return writer.run();
}

std::string subcircuit_name(std::string_view name) {
  if (!name.empty() && std::isalpha(static_cast<unsigned char>(name.front())) != 0) {
    return std::string{name};
  }
  return "x" + std::string{name};
}

std::string global_card(std::span<const extract::DeviceInfo> table, Dialect format, const PowerNames& power,
                        const extflat::NameFlags& names) {
  std::vector<std::string> list;
  for (const extract::DeviceInfo& info : table) {
    if (info.substrate_node.empty()) {
      continue;
    }
    std::string name = info.substrate_node;
    if (format != Dialect::cdl && name.back() == '!') {
      name.pop_back();
    }
    if (good_global(name) && std::find(list.begin(), list.end(), name) == list.end()) {
      list.insert(list.begin(), name);
    }
  }
  if (format == Dialect::cdl) {
    add_power_name(list, power.sub);
    add_power_name(list, power.vdd);
    add_power_name(list, power.gnd);
  }
  if (list.empty()) {
    return {};
  }
  std::string text = ".global ";
  for (const std::string& name : list) {
    text += detail::format_subs(name, names) + " ";
  }
  return text + "\n\n";
}

std::string instance_line(const InstanceCall& call, Dialect format, bool renumber, int& counter,
                          const extflat::NameFlags& names, const NameConverter& convert) {
  std::string out;
  std::size_t tchars = 0;
  if (call.path.empty() || renumber || format == Dialect::spice2) {
    out = "X" + std::to_string(counter++);
    tchars = 5;
  } else {
    // Only commas are converted in instance names.
    extflat::NameFlags flags = names;
    flags.trim_global = false;
    flags.trim_local = false;
    flags.convert_equal = false;
    flags.convert_brackets = false;
    flags.convert_comma = true;
    const std::string name = convert ? convert(call.path, format, flags) : joined(call.path);
    out = "X" + name;
    tchars = 1 + name.size();
  }
  for (const std::string& node : call.nodes) {
    continued(out, tchars, node);
  }
  if (tchars > 80) {
    out += "\n+";
  }
  if (format == Dialect::cdl) {
    out += " /";
  }
  out += " " + subcircuit_name(call.cell);
  for (const std::string& p : call.params) {
    continued(out, tchars, p);
  }
  return out + "\n";
}

}  // namespace hefesto::netlist::spice
