// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.sim`, `.al` and `.nodes` writer of `ext2sim`
// (netlist/sim/writer.hpp; capability netlist-sim).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2sim/ext2sim.c (simdevVisit,
// simdevSubstrate, simnAP, simnAPHier, simdevOutNode, simcapVisit,
// simresistVisit, simnodeVisit, simmkDevMerge, parallelDevs,
// simmergeVisit), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SIM (design D3).

#include "hefesto/netlist/sim/writer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <utility>

namespace hefesto::netlist::sim {

namespace {

constexpr int kNoResClass = -1;  // NO_RESCLASS

// One printf conversion of Magic's fprintf calls, into a string.
std::string fmt_g(double v) {
  std::array<char, 64> b{};
  const int n = std::snprintf(b.data(), b.size(), "%g", v);
  return std::string(b.data(), static_cast<std::size_t>(std::max(n, 0)));
}

std::string fmt_f(double v) {
  // %f of a large value can be long: size the buffer from the result.
  const int n = std::snprintf(nullptr, 0, "%f", v);
  std::string s(static_cast<std::size_t>(std::max(n, 0)) + 1, '\0');
  std::snprintf(s.data(), s.size(), "%f", v);
  s.resize(static_cast<std::size_t>(std::max(n, 0)));
  return s;
}

std::string fmt_1f(double v) {
  const int n = std::snprintf(nullptr, 0, "%.1f", v);
  std::string s(static_cast<std::size_t>(std::max(n, 0)) + 1, '\0');
  std::snprintf(s.data(), s.size(), "%.1f", v);
  s.resize(static_cast<std::size_t>(std::max(n, 0)));
  return s;
}

// Match("*[Ee][Xx][Tt]:[Aa][Pp]<c>*", attrs): `ext:ap<c>` anywhere, any case.
bool has_ap_attr(std::string_view attrs, char kind) {
  const std::string needle = std::string{"ext:ap"} + kind;
  if (attrs.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= attrs.size(); ++i) {
    bool same = true;
    for (std::size_t k = 0; k < needle.size() && same; ++k) {
      same = std::tolower(static_cast<unsigned char>(attrs[i + k])) == needle[k];
    }
    if (same) {
      return true;
    }
  }
  return false;
}

bool equal_ci(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

bool is_diode(DeviceClass c) {
  return c == DeviceClass::diode || c == DeviceClass::pdiode || c == DeviceClass::ndiode;
}

bool is_subckt_family(DeviceClass c) {
  switch (c) {
    case DeviceClass::subckt:
    case DeviceClass::rsubckt:
    case DeviceClass::msubckt:
    case DeviceClass::csubckt:
    case DeviceClass::dsubckt:
    case DeviceClass::veriloga:
      return true;
    default:
      return false;
  }
}

bool is_fet(DeviceClass c) {
  return c == DeviceClass::fet || c == DeviceClass::mosfet || c == DeviceClass::asymmetric;
}

char first_char(std::string_view s) {
  return s.empty() ? '\0' : s.front();
}

// The fetInfo entry of a device type: the last ExtGetDevInfo entry with
// that name (efBuildAddStr returns the existing index, so later entries
// overwrite earlier ones), with its tile type's long name.
struct FetInfo {
  const extract::DeviceInfo* info = nullptr;
  std::string_view long_name = "space";  // DBTypeLongNameTbl[TT_SPACE]
};

FetInfo fet_info(const WriteOptions& options, std::string_view type) {
  FetInfo found;
  for (std::size_t i = 0; i < options.device_info.size(); ++i) {
    if (options.device_info[i].model == type) {
      found.info = &options.device_info[i];
      found.long_name = i < options.type_long_names.size() ? std::string_view{options.type_long_names[i]} : "space";
    }
  }
  return found;
}

// One entry of the merge pre-pass (devMerge).
struct MergeEntry {
  std::int64_t l = 0;
  std::int64_t w = 0;
  std::optional<std::size_t> g, s, d;
  std::optional<std::string> b;  // the local substrate node (dev_subsnode)
  const SimDevice* device = nullptr;
  std::size_t index = 0;  // esFMIndex
};

class Writer {
 public:
  Writer(const SimInput& input, const WriteOptions& options, extflat::NameFlags& flags,
         const compat::CompatSettings& compat)
      : in_(input), opt_(options), set_(options.settings), flags_(flags), compat_(compat) {
    if (set_.alias) {
      out_.alias.emplace();
    }
    if (set_.labels) {
      out_.labels.emplace();
    }
    touched_.assign(in_.nodes.size(), false);
  }

  SimOutput run() {
    out_.sim = "| units: " + fmt_g(static_cast<double>(in_.scale)) + " tech: " + in_.tech + " format: " +
               format_name() + "\n";
    if (merging()) {
      merge_pass();
    }
    merge_index_ = 0;
    for (const SimDevice& device : in_.devices) {
      device_line(device);
    }
    if (opt_.flat_caps) {
      for (const SimCap& cap : in_.caps) {
        cap_line(cap);
      }
    }
    for (const SimResist& resist : in_.resists) {
      out_.sim += "r " + name(resist.a) + " " + name(resist.b) +
                  " " + fmt_g(static_cast<double>(resist.resistance) / 1000.) + "\n";
    }
    for (std::size_t i = 0; i < in_.nodes.size(); ++i) {
      node_lines(i);
    }
    return std::move(out_);
  }

 private:
  const char* format_name() const {
    switch (set_.format) {
      case SimFormat::lbl:
        return "LBL";
      case SimFormat::su:
        return "SU";
      case SimFormat::mit:
        break;
    }
    return "MIT";
  }

  bool merging() const { return set_.merge != SimMerge::none; }

  std::string name(const HierName& hier) { return extflat::name_to_sim(hier, flags_, compat_); }

  std::string node_name(std::size_t node) { return name(in_.nodes[node].names.front().name); }

  // simdevOutNode: ` <canonical name>`, or ` GND` when the name does not resolve.
  void out_node(std::string& line, const std::optional<std::size_t>& node) {
    if (!node || *node >= in_.nodes.size() || in_.nodes[*node].names.empty()) {
      line += " GND";
      return;
    }
    line += " " + node_name(*node);
    touched_[*node] = true;
  }

  // ----- merge pre-pass (simmergeVisit) -----

  bool parallel(const MergeEntry& a, const MergeEntry& b) const {
    if (a.g != b.g || a.b != b.b || a.l != b.l || (set_.merge != SimMerge::aggressive && a.w != b.w)) {
      return false;
    }
    if (!compat_.magic_compat &&
        (a.device->device_class != b.device->device_class || a.device->type != b.device->type)) {
      return false;  // Q-NETLIST-17 OFF: also equal class and type, as ext2spice
    }
    return (a.d == b.d && a.s == b.s) || (a.s == b.d && a.d == b.s);
  }

  void merge_pass() {
    std::vector<MergeEntry> list;  // newest first, as devMergeList
    int merged = 0;
    for (const SimDevice& device : in_.devices) {
      if (device.terminals.size() < 2) {
        out_.console.push_back(ConsoleLine{true, "outPremature"});
        continue;
      }
      MergeEntry entry = merge_entry(device);
      mult_.push_back(1.0F);
      const auto found = std::find_if(list.begin(), list.end(), [&](const MergeEntry& e) { return parallel(entry, e); });
      if (found == list.end()) {
        list.insert(list.begin(), entry);
        continue;
      }
      const float m = mult_[found->index] + (static_cast<float>(entry.w) / static_cast<float>(found->w));
      mult_[entry.index] = -1.0F;  // DEV_KILLED
      mult_[found->index] = m;
      ++merged;
    }
    out_.console.push_back(ConsoleLine{false, "Devices merged: " + std::to_string(merged)});
  }

  MergeEntry merge_entry(const SimDevice& device) const {
    MergeEntry e;
    e.l = static_cast<std::int64_t>(static_cast<float>(device.length) * device.scale);
    e.w = static_cast<std::int64_t>(static_cast<float>(device.width) * device.scale);
    e.g = device.terminals[0].node;
    e.s = device.terminals[1].node;
    e.d = device.terminals.size() >= 3 ? device.terminals[2].node : e.s;
    if (device.substrate) {
      e.b = device.substrate->local_node;
    }
    e.device = &device;
    e.index = mult_.size();
    return e;
  }

  // ----- device lines (simdevVisit) -----

  // The class letter; sets `subckt` for the x-class devices.
  std::string class_letter(const SimDevice& device, bool& subckt) {
    const DeviceClass c = device.device_class;
    if (c == DeviceClass::bjt) {
      return "b";
    }
    if (is_diode(c) || is_subckt_family(c)) {
      subckt = true;
      return "x";
    }
    if (c == DeviceClass::res) {
      return "r";
    }
    if (c == DeviceClass::cap || c == DeviceClass::caprev) {
      return "c";
    }
    if (is_fet(c)) {
      return fet_letter(device);
    }
    return std::string(1, first_char(device.type));
  }

  std::string fet_letter(const SimDevice& device) {
    const char t = first_char(device.type);
    if (t == 'n' || t == 'p') {
      return std::string(1, t);
    }
    const char l = first_char(fet_info(opt_, device.type).long_name);
    if (l == 'n' || l == 'p') {
      return std::string(1, l);
    }
    out_.console.push_back(ConsoleLine{true, "Error: MOSFET device type \"" + device.type +
                                                 "\" does not start with \"n\" or \"p\" as required for the .sim format"});
    return "n";
  }

  void device_line(const SimDevice& device) {
    if (device.terminals.size() < 2) {
      return;
    }
    float mult = 1.0F;
    if (merging()) {
      mult = mult_[merge_index_++];
      if (mult <= 0.0F) {
        return;  // devIsKilled
      }
    }
    bool subckt = false;
    std::string line = class_letter(device, subckt);
    DeviceClass cls = device.device_class;
    terminal_names(line, device, cls);
    if (!is_diode(cls) && device.substrate && (subckt || set_.format == SimFormat::lbl)) {
      line += " " + substrate_text(device, 0.0F, false);
    }
    std::int64_t w = device.width;
    if (merging()) {
      w = static_cast<std::int64_t>(static_cast<float>(w) * mult);
    }
    values(line, device, cls, subckt, w);
    if (subckt) {
      line += " " + device.type;
    }
    out_.sim += line + "\n";
  }

  void terminal_names(std::string& line, const SimDevice& device, DeviceClass& cls) {
    const std::vector<SimTerminal>& t = device.terminals;
    if (cls != DeviceClass::res) {
      out_node(line, t[0].node);
    }
    out_node(line, t[1].node);
    if (first_char(device.type) == 'b') {
      cls = DeviceClass::bjt;  // the BiCMOS hack of ext2sim.c
    }
    if (cls == DeviceClass::bjt && device.substrate) {
      out_node(line, device.substrate->node);
    } else if (t.size() > 2) {
      out_node(line, t[2].node);
    }
    for (std::size_t i = 3; i < t.size(); ++i) {
      out_node(line, t[i].node);
    }
  }

  double scaled(std::int64_t v, float scale) const {
    return static_cast<double>(static_cast<float>(v) * scale);
  }

  void values(std::string& line, const SimDevice& device, DeviceClass cls, bool subckt, std::int64_t w) {
    const float s = device.scale;
    const double x = scaled(device.lower_left.x, s);
    const double y = scaled(device.lower_left.y, s);
    if (cls == DeviceClass::bjt) {
      line += bjt_values(device) + " " + fmt_g(x) + " " + fmt_g(y);
    } else if (cls == DeviceClass::res) {
      line += " " + fmt_f(device.resistance);
    } else if (cls == DeviceClass::cap || cls == DeviceClass::caprev) {
      line += " " + fmt_f(device.capacitance);
    } else if (subckt) {
      subckt_values(line, device, w, x, y);
    } else if (!is_diode(cls)) {
      line += " " + fmt_g(scaled(device.length, s)) + " " + fmt_g(scaled(w, s)) + " " + fmt_g(x) + " " + fmt_g(y);
      if (!set_.no_attrs) {
        attributes(line, device);
      }
    }
  }

  // The bipolar L and W from the substrate's area and half-perimeter.
  // Magic dereferences a missing substrate and truncates a NaN; both modes
  // write 0 there instead (X-2 tier 1).
  std::string bjt_values(const SimDevice& device) const {
    double ca = 0.0;
    double chp = 0.0;
    if (device.substrate) {
      for (const AreaPerim& pa : device.substrate->local_area_perim) {
        ca += static_cast<double>(pa.area);
        chp += 0.5 * static_cast<double>(pa.perim);
      }
    }
    const double cl = 0.5 * (chp + std::sqrt(chp * chp - 4 * ca));
    const double cw = ca / cl;
    const auto trunc = [](double v) { return std::isfinite(v) ? static_cast<int>(v) : 0; };
    return " " + std::to_string(trunc(cl)) + " " + std::to_string(trunc(cw));
  }

  void subckt_values(std::string& line, const SimDevice& device, std::int64_t w, double x, double y) {
    const std::vector<SimTerminal>& t = device.terminals;
    if (!t[1].attributes.empty()) {
      line += " s=" + t[1].attributes;
    }
    if (t.size() >= 3 && !t[2].attributes.empty()) {
      line += " d=" + t[2].attributes;
    }
    line += " l=" + fmt_g(scaled(device.length, device.scale)) + " w=" + fmt_g(scaled(w, device.scale)) + " x=" +
            fmt_g(x) + " y=" + fmt_g(y);
  }

  // ----- transistor attributes -----

  void attributes(std::string& line, const SimDevice& device) {
    const std::vector<SimTerminal>& t = device.terminals;
    const SimTerminal& gate = t[0];
    const SimTerminal& source = t[1];
    const SimTerminal& drain = t.size() >= 3 ? t[2] : t[1];
    const bool su = set_.format == SimFormat::su;
    const FetInfo info = fet_info(opt_, device.type);
    if (!gate.attributes.empty()) {
      line += " g=" + gate.attributes;
    }
    if (su) {
      const bool sub_ap = !gate.attributes.empty() && has_ap_attr(gate.attributes, 's');
      line += gate.attributes.empty() ? " g=" : ",";
      line += substrate_text(device, device.scale, sub_ap);
    }
    terminal_attrs(line, device, source, " s=", info.info != nullptr ? info.info->source_class : kNoResClass);
    terminal_attrs(line, device, drain, " d=", info.info != nullptr ? info.info->drain_class : kNoResClass);
  }

  void terminal_attrs(std::string& line, const SimDevice& device, const SimTerminal& term, const char* key,
                      int res_class) {
    bool hier = set_.hier_ap;
    if (!term.attributes.empty()) {
      line += key + term.attributes;
      if (has_ap_attr(term.attributes, 'h')) {
        hier = true;
      } else if (has_ap_attr(term.attributes, 'f')) {
        hier = false;
      }
    }
    if (set_.format != SimFormat::su) {
      return;
    }
    line += term.attributes.empty() ? key : ",";
    line += hier ? ap_hier(term, device.prefix, res_class, device.scale) : ap(term.node, res_class, device.scale);
  }

  static std::string ap_text(const std::vector<AreaPerim>& pa, int res_class, float scale) {
    const auto index = static_cast<std::size_t>(res_class);
    const AreaPerim v = index < pa.size() ? pa[index] : AreaPerim{};
    const auto a = std::max(0, static_cast<int>(static_cast<float>(v.area) * scale * scale));
    const auto p = std::max(0, static_cast<int>(static_cast<float>(v.perim) * scale));
    return "A_" + std::to_string(a) + ",P_" + std::to_string(p);
  }

  // simnAP: a flat node's area and perimeter of one class, once per run.
  std::string ap(const std::optional<std::size_t>& node, int res_class, float scale) {
    if (!node || *node >= in_.nodes.size() || res_class == kNoResClass ||
        !visited_.insert({*node, res_class}).second) {
      return "A_0,P_0";
    }
    touched_[*node] = true;
    return ap_text(in_.nodes[*node].area_perim, res_class, scale);
  }

  // simnAPHier: the terminal's local node, once per prefix and class.
  std::string ap_hier(const SimTerminal& term, const std::string& prefix, int res_class, float scale) {
    auto& [last_prefix, classes] = hier_visited_[term.local_node];
    if (last_prefix != prefix) {
      classes.clear();
      last_prefix = prefix;
    }
    if (res_class == kNoResClass || !classes.insert(res_class).second) {
      return "A_0,P_0";
    }
    return ap_text(term.local_area_perim, res_class, scale);
  }

  // simdevSubstrate.
  std::string substrate_text(const SimDevice& device, float scale, bool do_ap) {
    if (!device.substrate) {
      return "errGnd!";  // Magic dereferences the missing substrate (X-2 tier 1)
    }
    const SimDevice::Substrate& sub = *device.substrate;
    const FetInfo info = fet_info(opt_, device.type);
    const bool su = set_.format == SimFormat::su;
    if (info.info != nullptr && !info.info->substrate_node.empty() &&
        equal_ci(sub.local_name, info.info->substrate_node)) {
      return (su ? "S_" : "") + trimmed(sub.local_name);
    }
    if (!sub.node || *sub.node >= in_.nodes.size()) {
      return "errGnd!";
    }
    std::string text;
    if (su && do_ap) {
      text += substrate_ap(device, info, *sub.node, scale);
    }
    return text + (su ? "S_" : "") + node_name(*sub.node);
  }

  std::string substrate_ap(const SimDevice& device, const FetInfo& info, std::size_t node, float scale) {
    const int res_class = info.info != nullptr ? info.info->substrate_class : kNoResClass;
    if (res_class < 0) {
      out_.console.push_back(ConsoleLine{true, "Error: subap for devtype " + std::to_string(type_index(device.type)) +
                                                   " required but not specified on command line"});
      return "A_0,P_0,";
    }
    return ap(node, res_class, scale) + ",";
  }

  int type_index(std::string_view type) const {
    for (std::size_t i = opt_.device_info.size(); i-- > 0;) {
      if (opt_.device_info[i].model == type) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  std::string trimmed(std::string name) const {
    if (!name.empty() && ((flags_.trim_global && name.back() == '!') || (flags_.trim_local && name.back() == '#'))) {
      name.pop_back();
    }
    return name;
  }

  // ----- parasitics and nodes -----

  void cap_line(const SimCap& cap) {
    const double c = cap.cap / 1000;
    if (c <= static_cast<double>(opt_.cap_threshold)) {
      return;
    }
    out_.sim += "C " + name(cap.a) + " " + name(cap.b) + " " + fmt_1f(c) + "\n";
  }

  void node_lines(std::size_t index) {
    const SimNode& node = in_.nodes[index];
    if ((set_.dev_nodes_only && !touched_[index]) || node.names.empty()) {
      return;
    }
    const std::string canonical = node_name(index);
    const double cap = node.cap / 1000;
    const std::int64_t res = (node.resist + 500) / 1000;
    if (cap > static_cast<double>(opt_.cap_threshold)) {
      // Q-NETLIST-15: `C <name>0 <cap>` ON, `C <name> GND <cap>` OFF.
      out_.sim += "C " + canonical + (compat_.magic_compat ? "0 " : " GND ") + fmt_1f(cap) + "\n";
    }
    if (res > opt_.resist_threshold) {
      out_.sim += "R " + canonical + " " + std::to_string(res) + "\n";
    }
    if (!node.attributes.empty() && !set_.no_attrs) {
      out_.sim += "A " + canonical;
      const char* sep = " ";
      for (const std::string& attr : node.attributes) {
        out_.sim += sep + attr;
        sep = ",";
      }
      out_.sim += "\n";
    }
    aliases(node, canonical);
    if (out_.labels) {
      *out_.labels += canonical + " " + std::to_string(node.location.x) + " " + std::to_string(node.location.y) +
                      " " + node.layer + "\n";
    }
  }

  void aliases(const SimNode& node, const std::string& canonical) {
    const bool global = node.names.front().global;
    std::string& target = out_.alias ? *out_.alias : out_.sim;
    for (std::size_t i = 1; i < node.names.size(); ++i) {
      if (global && node.names[i].global) {
        continue;
      }
      target += "= " + canonical + " " + name(node.names[i].name) + "\n";
    }
  }

  const SimInput& in_;
  const WriteOptions& opt_;
  const SimSettings& set_;
  extflat::NameFlags& flags_;
  const compat::CompatSettings& compat_;
  SimOutput out_;
  std::vector<float> mult_;  // esFMult
  std::size_t merge_index_ = 0;
  std::vector<bool> touched_;  // efnode_client set (the -F test)
  std::set<std::pair<std::size_t, int>> visited_;
  std::map<std::string, std::pair<std::string, std::set<int>>> hier_visited_;
};

}  // namespace

std::optional<DeviceClass> device_class(std::string_view keyword) noexcept {
  static constexpr std::array<std::pair<std::string_view, DeviceClass>, 17> kClasses{{
      {"fet", DeviceClass::fet},
      {"mosfet", DeviceClass::mosfet},
      {"asymmetric", DeviceClass::asymmetric},
      {"bjt", DeviceClass::bjt},
      {"devres", DeviceClass::res},
      {"devcap", DeviceClass::cap},
      {"devcaprev", DeviceClass::caprev},
      {"vsource", DeviceClass::vsource},
      {"diode", DeviceClass::diode},
      {"pdiode", DeviceClass::pdiode},
      {"ndiode", DeviceClass::ndiode},
      {"subckt", DeviceClass::subckt},
      {"rsubckt", DeviceClass::rsubckt},
      {"msubckt", DeviceClass::msubckt},
      {"csubckt", DeviceClass::csubckt},
      {"dsubckt", DeviceClass::dsubckt},
      {"veriloga", DeviceClass::veriloga},
  }};
  for (const auto& [name, cls] : kClasses) {
    if (name == keyword) {
      return cls;
    }
  }
  return std::nullopt;
}

SimOutput write_sim(const SimInput& input, const WriteOptions& options, extflat::NameFlags& flags,
                    const compat::CompatSettings& compat) {
  return Writer(input, options, flags, compat).run();
}

}  // namespace hefesto::netlist::sim
