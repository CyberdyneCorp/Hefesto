// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the state of one flat SPICE write (writer.hpp write_flat):
// ext2spice's counters, the per-node client records (nodeClient: SPICE
// name, visit mask or junction widths), the node capacitances the resistor
// pi split moves, the HSPICE prefix dictionary and the per-device merge
// multipliers (Q-NETLIST-6: keyed by device). The member functions are
// split by topic over flat_writer.cpp (names and nodes), device_lines.cpp
// (spcdevVisit), device_params.cpp (parameters, areas, substrate) and
// merge.cpp (devMergeVisit).
//
// Owner: SPICE.

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/netlist/spice/writer.hpp"

namespace hefesto::netlist::spice::detail {

/// nodeClient of one flat node.
struct NodeClient {
  bool exists = false;                  // efnode_client allocated
  std::optional<std::string> spice_name;
  std::vector<bool> visited;            // visitMask: resist classes + the "used" bit
  std::vector<float> widths;            // -d: junction widths per class (+1)
  bool connected = false;               // magic-compat OFF: touched by a device or resistor
};

/// The terminals of a device as spcdevVisit picks them.
struct Terminals {
  std::size_t gate = 0;
  std::size_t source = 0;
  std::size_t drain = 0;
  bool has_source = false;
};

class FlatWriter {
 public:
  FlatWriter(const SpiceInput& input, const SpiceSettings& settings, const RunFlags& flags,
             const compat::CompatSettings& compat, extflat::NameFlags& names, NameConverter convert);

  WriteResult run();

  // --- flat_writer.cpp: names, nodes, parasitics -------------------------
  [[nodiscard]] std::optional<std::size_t> find_node(std::string_view alias) const;
  /// nodeSpiceName: the node's SPICE name, created on first use.
  const std::string& spice_name(std::size_t node);
  /// nodeSpiceName of an alias; `errGnd!` when it does not resolve.
  std::string spice_name_of(std::string_view alias);
  /// markVisited(…, efNumResistClasses), or update_w with -d.
  void mark_used(std::size_t node);
  void init_client(std::size_t node);
  /// spcdevOutNode: ` <name>`, marking the node used.
  void out_node(std::string_view alias);
  [[nodiscard]] std::string converted_name(std::size_t node);

  // --- device_lines.cpp ---------------------------------------------------
  void device(std::size_t index);

  // --- device_params.cpp --------------------------------------------------
  [[nodiscard]] const DeviceTypeInfo& type_info(const std::string& type) const;
  [[nodiscard]] const std::vector<ParamDecl>* declared(std::string_view key) const;
  /// spcdevSubstrate; returns the flat node when it was looked up.
  std::optional<std::size_t> substrate(const InputDevice& dev, bool print);
  void write_value(const InputDevice& dev);
  bool write_sub_param(const InputDevice& dev);
  void write_params(InputDevice& dev, std::int64_t l, std::int64_t w, float m, bool subdone);
  void terminal_ap(InputTerminal& term, std::optional<std::size_t> node, std::int32_t res_class, float scale,
                   std::string_view aname, std::string_view pname, float m, std::int64_t w);
  void substrate_ap(std::size_t node, std::int32_t res_class, float scale, float m);
  void geometry(std::int64_t l, std::int64_t w, float scale, int dscale);

  // --- merge.cpp ------------------------------------------------------------
  void merge_devices();
  /// devDistJunctVisit: adds the device's width to its terminal nodes.
  void distribute_junction(const InputDevice& dev);

  // Shared state.
  const SpiceInput& in;
  const SpiceSettings& settings;
  const RunFlags& flags;
  const compat::CompatSettings& compat;
  extflat::NameFlags& names;
  NameConverter convert;

  std::string out;
  std::vector<WriteMessage> messages;
  std::vector<InputDevice> devices;      // a copy: spcdevVisit swaps and blanks
  std::vector<float> node_caps;          // efnode_cap, changed by the pi split
  std::vector<NodeClient> clients;
  std::map<std::string, std::size_t, std::less<>> by_alias;
  std::vector<std::pair<std::string, int>> hspice_prefixes;  // dictionary in assignment order
  std::map<std::string, int, std::less<>> hspice_index;
  std::vector<float> mult;               // per device (Q-NETLIST-6)
  std::vector<bool> killed;
  std::size_t used_bit = 0;              // efNumResistClasses
  bool init_used = false;                // initMask has the used bit (device phase)
  float es_scale = 0.0F;                 // esScale: < 0 in scale mode

  int cap_num = 0;
  int volt_num = 0;
  int dev_num = 1000;
  int res_num = 0;
  int diode_num = 0;
  int sbck_num = 0;
  int node_num = 10;
  int merged = 0;

  [[nodiscard]] bool merging() const { return settings.merge != MergeMode::none; }
  void warn(std::string text) { messages.push_back({true, std::move(text)}); }
  void info(std::string text) { messages.push_back({false, std::move(text)}); }
};

/// esFormatSubs: a global name with the run's conversions (only the last
/// character is trimmed).
[[nodiscard]] std::string format_subs(std::string name, const extflat::NameFlags& names);

/// extHierSDAttr: hierarchical source/drain areas for this terminal.
[[nodiscard]] bool hier_sd_attr(const InputTerminal& term, bool hier_ap);
/// Magic's Match() of `*ext:<kind>*`, case-insensitive.
[[nodiscard]] bool has_ext_attr(const std::optional<std::string>& attrs, std::string_view kind);

}  // namespace hefesto::netlist::spice::detail
