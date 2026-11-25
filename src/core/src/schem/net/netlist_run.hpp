// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// One netlist run (capability schematic-netlisting "Netlist file
// structure", "Subcircuit body", "Subcircuit definitions", "Schematic
// resolution", "Device models", "Errors and warnings"). Internal to
// schem/net; the entry point is netlist() in netlist.hpp.
//
//   netlist.cpp      the run: options, file structure, top line, finish
//   level.cpp        one schematic: instances, connectivity, element texts,
//                    the body
//   definitions.cpp  the list of subcircuit definitions, schematic
//                    resolution, the `.subckt` header
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/connectivity.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/net/postprocess.hpp"
#include "hefesto/schem/net/substitute.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem::detail {

/// The instance and symbol template one schematic is netlisted under
/// ("Hierarchical parameters": the parent instance and the parent symbol's
/// template); both null at top level.
struct Parents {
  const PropertySource* instance = nullptr;
  const PropertySource* templ = nullptr;
};

/// One instance of a schematic with what netlisting reads from it.
struct Placed {
  std::size_t index = 0;
  const InstanceRecord* record = nullptr;
  const Symbol* symbol = nullptr;
  std::string type;
  PropertyString instance{std::string{}};
  PropertyString symprops{std::string{}};
  PropertyString templ{std::string{}};
  std::vector<PinIndex> order;  // pin order
  std::vector<PropertyString> pin_attributes;  // per pin, in pin order
  std::string name;  // instance name, substituted from the parents
  bool ignored = false;
  bool shorted = false;
};

/// A schematic with its instances placed and its nets named.
struct Level {
  const Document* document = nullptr;
  Parents parents;
  bool top = false;
  std::vector<Placed> instances;
  NetNaming naming;
};

/// An entry of the list of loaded symbols ("Subcircuit definitions"): a
/// symbol, or a variant made by an instance `schematic` attribute
/// (Q-SCHEM-28).
struct DefinitionEntry {
  const Symbol* symbol = nullptr;
  std::string reference;  // as written in the first instance
  std::string variant;    // the `schematic` value; empty for the symbol itself
  std::string variant_props;  // property string of the instance that made the variant
  platform::Path from_folder;  // folder of the schematic holding that instance

  [[nodiscard]] std::pair<const Symbol*, std::string> key() const { return {symbol, variant}; }
};

/// A definition the run writes.
struct PlannedDefinition {
  DefinitionEntry entry;
  std::string name;              // the subcircuit name (`<cell>`, `<cell>_<k>` or `<v>`)
  platform::Path schematic;      // resolved schematic file
  const Document* document = nullptr;  // nullptr: the schematic is missing
};

struct ExtraKeys;  // definitions.cpp

/// What scanning a symbol format for `.subckt` header ports reads.
struct HeaderScan {
  const Symbol& symbol;
  const Parents& parents;
  std::span<const PinIndex> order;   // pin order
  std::span<const PinIndex> listed;  // the pins `@pinlist` writes
  std::string_view format;
};

/// "Device models": values collected once per key, written by bucket.
class DeviceModels {
 public:
  void add(std::string value);
  [[nodiscard]] bool empty() const noexcept { return order_.empty(); }
  /// The values in bucket order, each followed by a LF.
  [[nodiscard]] std::string text() const;

 private:
  std::vector<std::string> order_;  // keys in first-insertion order
  std::map<std::string, std::string, std::less<>> values_;
};

class NetlistRun {
 public:
  NetlistRun(const NetlistEnvironment& environment, const Document& top, const NetlistOptions& options,
             bool xschem_compat);

  NetlistResult run();

 private:
  // netlist.cpp
  void apply_options_instances(const Level& top);
  void apply_options_instance(const Placed& instance);
  void top_line(const Level& top);
  std::string top_ports(const Level& top);
  std::optional<std::string> top_symbol_ports(const Level& top);
  void sort_top_ports(std::vector<const Placed*>& ports);
  void top_user_code(const Level& top, std::string_view place);
  void end_block(const Level& top);
  void globals_block();
  void emit_header(const Level& top);
  NetlistResult finish_text();
  std::string place_of(const Placed& instance) const;
  [[nodiscard]] std::string path_text(const platform::Path& file) const;
  void warn(diag::Diagnostic diagnostic);
  void error(diag::Diagnostic diagnostic);

  // level.cpp
  Level place(const Document& document, Parents parents, bool top);
  static NetlistResult interrupted_result();
  void place_one(Level& level, std::size_t index);
  void connect(Level& level);
  NetInstance net_instance(const Level& level, const Placed& instance);
  static void count_pin_uses(const NetNaming& naming, std::map<std::string, std::size_t>& uses);
  void record_naming(const Level& level);
  void body(Level& level);
  void port_lines(const Level& level);
  void instance_line(const Level& level, const Placed& instance);
  ElementText element(const Level& level, const Placed& instance);
  void collect_device_model(const Level& level, const Placed& instance);
  std::string substitute_parents(std::string_view text, const Parents& parents);
  std::string attribute(const Placed& instance, std::string_view key) const;
  std::string evaluate_value(const std::string& value, const Placed& instance) const;
  bool pininfo(const Level& level) const;
  void check_instance(const Level& level, const Placed& instance);
  void emit(std::string text, std::string prefix = {}, bool user_code = false);
  std::size_t width_of(const std::string& name);

  // definitions.cpp
  void add_used(const Level& level);
  void plan_definitions();
  bool skip_definition(const DefinitionEntry& entry) const;
  std::optional<std::string> definition_name(const DefinitionEntry& entry);
  std::optional<std::string> schematic_value(const DefinitionEntry& entry);
  platform::Path resolve_schematic(const DefinitionEntry& entry, const std::string& cell);
  const Document* load_schematic(const platform::Path& file);
  void write_definitions();
  void write_definition(const PlannedDefinition& definition);
  void schematic_user_code(const Document& document);
  void check_pins(const Symbol& symbol, const Level& level);
  std::string header_line(const PlannedDefinition& definition, const Parents& parents);
  std::string header_ports(const PlannedDefinition& definition, const Parents& parents, bool& reached_end);
  std::string header_token(const HeaderScan& scan, char sigil, const std::string& key, std::string_view source,
                           bool& stopped);
  std::string port_names(const Symbol& symbol, const Parents& parents, std::span<const PinIndex> pins);
  std::string header_template(const PlannedDefinition& definition);
  bool drops_template_key(const Symbol& symbol, std::string_view key, ExtraKeys& extra);
  std::string symbol_format(const Symbol& symbol) const;
  std::optional<std::string> symname_override(const Placed& instance) const;
  [[nodiscard]] std::string cell_name(const DefinitionEntry& entry) const;
  std::string variant_value(const Placed& instance) const;

  const NetlistEnvironment& env_;
  const Document& top_;
  NetlistOptions options_;
  bool compat_;
  bool pininfo_top_ = false;  // a netlist_options instance set top_is_subckt
  bool top_subckt_ = false;   // top level written as an active subcircuit
  ResolveContext resolve_;
  diag::CollectingSink resolve_warnings_;
  BusBudget bus_budget_;  // Q-SCHEM-41: the expanded bus text of this run

  NetlistResult result_;
  bool stopped_ = false;  // the environment's interrupted() fired
  std::vector<NetlistChunk> chunks_;
  ElementRunState element_state_;
  DeviceModels models_;
  std::vector<std::string> globals_;
  std::unordered_set<std::string> globals_seen_;
  std::set<const Symbol*> untyped_warned_;
  std::unordered_map<std::string, std::size_t> widths_;  // bus_width() by name, for this run

  std::vector<DefinitionEntry> entries_;
  std::set<std::pair<const Symbol*, std::string>> entry_keys_;
  std::vector<PlannedDefinition> planned_;
  std::map<std::pair<const Symbol*, std::string>, std::string> symnames_;
  std::map<platform::Path, std::unique_ptr<Document>> documents_;
  std::string topschname_;
};

}  // namespace hefesto::schem::detail
