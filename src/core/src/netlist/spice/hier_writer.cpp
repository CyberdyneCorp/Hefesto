// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchical SPICE write (writer.hpp write_hier; capability
// netlist-spice-hierarchy "Section layout", "Subcircuit calls in
// hierarchical mode", "Hierarchical device and parasitic differences"):
// the file header and `.global` card, then one section per definition in
// the order the caller gives (children first, the root last), each with
// its `.subckt` line or top-level comment, its calls, devices, resistors,
// couplings and node lines, and the counters reset after it (the voltage
// source counter per Q-NETLIST-35).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2hier.c:591-640
// (subcktHierVisit), 2300-2473 (esHierVisit) and ext2spice/ext2spice.c:
// 1600-1790 (subcktVisit), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "netlist/spice/flat_writer.hpp"
#include "netlist/spice/numbers.hpp"

namespace hefesto::netlist::spice {

namespace {

using PortIndex = std::map<std::string, int, std::less<>>;

// The port index of each name of a definition: the numbering its own
// `.subckt` line gave, else the `port` records' indices.
PortIndex port_index(const HierCall& call, const std::map<std::string, PortIndex, std::less<>>& numbered) {
  if (const auto it = numbered.find(call.cell); it != numbered.end()) {
    return it->second;
  }
  PortIndex out;
  for (const extflat::PortEntry& e : call.ports) {
    for (const extflat::PortName& n : e.chain) {
      out.emplace(n.name, n.index);
    }
    out.emplace(e.canonical.name, e.canonical.index);
  }
  return out;
}

int index_of(const PortIndex& ports, const std::string& name) {
  const auto it = ports.find(name);
  return it == ports.end() ? -1 : it->second;
}

// subcktVisit's port list: by the canonical name's index, else the first
// name of the entry's chain that has one.
std::vector<std::string> call_ports(const HierCall& call, const PortIndex& ports) {
  int portmax = -1;
  for (const auto& [name, index] : ports) {
    portmax = std::max(portmax, index);
  }
  std::vector<std::string> list(static_cast<std::size_t>(portmax + 1));
  for (const extflat::PortEntry& e : call.ports) {
    const int canonical = index_of(ports, e.canonical.name);
    if (canonical >= 0 && list[static_cast<std::size_t>(canonical)].empty()) {
      list[static_cast<std::size_t>(canonical)] = e.canonical.name;
      continue;
    }
    for (const extflat::PortName& n : e.chain) {
      const int index = index_of(ports, n.name);
      if (index >= 0 && list[static_cast<std::size_t>(index)].empty()) {
        list[static_cast<std::size_t>(index)] = n.name;
        break;
      }
    }
  }
  return list;
}

// subcktHierVisit: a device-less cell that is no stub has been absorbed
// into its parent and gets no call.
bool has_call(const HierCall& call) { return !call.devices_absent || call.stub; }

class HierWriter {
 public:
  HierWriter(const SpiceSettings& settings, const RunFlags& flags, const compat::CompatSettings& compat,
             extflat::NameFlags& names, NameConverter convert)
      : settings_(settings), flags_(flags), compat_(compat), names_(names), convert_(std::move(convert)) {}

  WriteResult run(const HierHead& top, const std::vector<HierSection>& sections) {
    detail::FlatWriter head{top.root, settings_, flags_, compat_, names_, convert_};
    head.hier = true;
    if (!top.root.style_matches) {
      head.warn("Warning:  Current extraction style does not match .ext file!");
      head.warn("Area/Perimeter values and parasitic values will be zero.");
    }
    head.write_file_header();
    out_ = std::move(head.out);
    messages_ = std::move(head.messages);
    out_ += global_card(top.table, settings_.format, top.power, names_);
    for (const HierSection& section : sections) {
      write_section(section);
    }
    return WriteResult{std::move(out_), std::move(messages_)};
  }

 private:
  void write_section(const HierSection& section) {
    detail::FlatWriter w{section.input, settings_, flags_, compat_, names_, convert_};
    w.hier = true;
    // Q-NETLIST-35: with magic-compat ON the voltage-source counter runs
    // on across the sections of a run.
    w.volt_num = compat_.magic_compat ? volt_num_ : 0;
    w.es_scale = settings_.scale ? -1.0F : static_cast<float>(detail::dbl(section.input.scale) / 100.0);
    if (section.stub) {
      w.out += "* Black-box entry subcircuit for " + section.input.cell + " abstract view\n";
    }
    if (section.wrapped) {
      numbered_[section.input.cell] = w.write_subckt_line();
    } else {
      w.out += "\n* Top level circuit " + section.input.cell + "\n\n";
    }
    if (!section.stub) {
      write_calls(w, section);
      w.write_section_body();
    }
    w.out += section.wrapped ? ".ends\n\n" : ".end\n\n";
    volt_num_ = w.volt_num;
    out_ += w.out;
    for (WriteMessage& m : w.messages) {
      messages_.push_back(std::move(m));
    }
  }

  void write_calls(detail::FlatWriter& w, const HierSection& section) {
    for (const HierCall& call : section.calls) {
      if (!has_call(call)) {
        continue;
      }
      const PortIndex ports = port_index(call, numbered_);
      InstanceCall line{call.path, {}, call.cell, {}};
      std::string prefix;
      for (const std::string& part : call.path) {
        prefix += part + "/";
      }
      for (const std::string& name : call_ports(call, ports)) {
        if (name.empty()) {
          continue;
        }
        line.nodes.push_back(w.spice_name_of(prefix + name));
        if (const auto node = w.find_node(prefix + name)) {
          w.mark_used(*node);
        }
      }
      if (const auto* params = w.declared(":" + subcircuit_name(call.cell))) {
        for (const ParamDecl& p : *params) {
          line.params.push_back(p.name);
        }
      }
      w.out += instance_line(line, settings_.format, settings_.renumber, w.sbck_num, names_, w.convert);
    }
  }

  const SpiceSettings& settings_;
  const RunFlags& flags_;
  const compat::CompatSettings& compat_;
  extflat::NameFlags& names_;
  NameConverter convert_;
  std::string out_;
  std::vector<WriteMessage> messages_;
  std::map<std::string, PortIndex, std::less<>> numbered_;
  int volt_num_ = 0;
};

}  // namespace

WriteResult write_hier(const HierHead& head, const std::vector<HierSection>& sections, const SpiceSettings& settings,
                       const RunFlags& flags, const compat::CompatSettings& compat, extflat::NameFlags& names,
                       const NameConverter& convert) {
  return HierWriter{settings, flags, compat, names, convert}.run(head, sections);
}

}  // namespace hefesto::netlist::spice
