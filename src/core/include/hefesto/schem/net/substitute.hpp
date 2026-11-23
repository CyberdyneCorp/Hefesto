// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format string substitution and hierarchical parameters
// (openspec/changes/m8-schematic design.md D9; capability
// schematic-netlisting "Format string substitution", "Hierarchical
// parameters", Q-SCHEM-26).
//
// The element text of one instance is built from property sources (the
// instance, its symbol, its template and the parent instance) and the nets
// already attached to its pins. Property sources hide where the attributes
// come from: a property string read with the FORMAT grammar, or a map.
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"

namespace hefesto::schem {

/// Attributes looked up by key.
class PropertySource {
 public:
  PropertySource() = default;
  PropertySource(const PropertySource&) = default;
  PropertySource& operator=(const PropertySource&) = default;
  virtual ~PropertySource() = default;

  /// The value of `key`, nullopt when it is not defined.
  [[nodiscard]] virtual std::optional<std::string> get(std::string_view key, ValueForm form) const = 0;

  [[nodiscard]] bool defines(std::string_view key) const { return get(key, ValueForm::raw).has_value(); }

  /// The keys defines() is true for, each read once.
  [[nodiscard]] virtual std::vector<std::string> keys() const = 0;
};

/// A property string read with the property-string grammar (get_token).
class PropertyString final : public PropertySource {
 public:
  explicit PropertyString(std::string props) : props_{std::move(props)} {}
  [[nodiscard]] std::optional<std::string> get(std::string_view key, ValueForm form) const override;
  [[nodiscard]] std::vector<std::string> keys() const override;

 private:
  std::string props_;
};

/// Attributes given as decoded values (tests, computed attributes).
class PropertyMap final : public PropertySource {
 public:
  PropertyMap() = default;
  PropertyMap(std::initializer_list<std::pair<const std::string, std::string>> values) : values_{values} {}
  void set(std::string key, std::string value) { values_[std::move(key)] = std::move(value); }
  [[nodiscard]] std::optional<std::string> get(std::string_view key, ValueForm form) const override;
  [[nodiscard]] std::vector<std::string> keys() const override;

 private:
  std::map<std::string, std::string, std::less<>> values_;
};

/// "Boolean values" of capability schematic-file-format: true, false or
/// neither (nullopt).
[[nodiscard]] std::optional<bool> boolean_value(std::string_view value) noexcept;

/// One pin of the instance, in pin order.
struct ElementPin {
  std::string name;
  std::size_t width = 1;  // bus width of `name`
  /// Netlist text of the attached net (connectivity.hpp net_text); empty
  /// when the pin has no net.
  std::string net;
  bool spice_ignore = false;
  const PropertySource* attributes = nullptr;  // the pin box's attributes
};

struct ElementContext {
  const PropertySource* instance = nullptr;  // required
  const PropertySource* symbol = nullptr;    // the symbol property (K record)
  const PropertySource* templ = nullptr;     // the symbol's `template` attributes
  const PropertySource* parent_instance = nullptr;  // none at top level
  const PropertySource* parent_template = nullptr;  // the parent symbol's template
  std::vector<ElementPin> pins;
  std::size_t index = 0;    // instance index in its schematic (@index)
  std::string symref;       // symbol reference as written (@symref)
  /// `@symname` in place of the one derived from `symref`: the subcircuit
  /// name of a schematic variant (Q-SCHEM-28) or a renamed cell (Q-SCHEM-14).
  std::optional<std::string> symname;
  std::string schname;      // current schematic file name with extension
  std::string topschname;   // @topschname text
  TclContext tcl;           // `path` and `sch_basename` for tcleval
};

struct ElementOptions {
  bool lvs_netlist = false;
  bool spiceprefix = true;
  bool xschem_compat = true;
  std::string bus_replacement;  // option bus_replacement_char
  int ev_precision = 4;
  const TclBridge* tcl = nullptr;  // nullptr: every tcleval fails (`?` LF)
};

/// Run-wide state: the `__UNCONNECTED_PIN__` counter and the run's bus
/// expansion budget (Q-SCHEM-41; nullptr: none).
struct ElementRunState {
  std::size_t unconnected = 0;
  BusBudget* bus_budget = nullptr;
};

struct ElementText {
  /// The element text, LF-terminated; empty when the instance has no format.
  std::string text;
  /// The element's spice prefix from `@spiceprefix` (option spiceprefix on).
  std::string spice_prefix;
  std::vector<diag::Diagnostic> diagnostics;
};

/// The format string ("Format string substitution"): instance then symbol
/// `lvs_format` in LVS mode, then instance then symbol `format`.
[[nodiscard]] std::optional<std::string> select_format(const ElementContext& context, bool lvs_netlist);

/// The instance name: its `name`, else the template's.
[[nodiscard]] std::string instance_name(const ElementContext& context);

/// The value of `@key` by "Hierarchical parameters", before `expr`;
/// nullopt when no source defines `key`.
[[nodiscard]] std::optional<std::string> resolve_parameter(const ElementContext& context, std::string_view key);

/// `text` with `@`/`%` tokens substituted from `parents`, nearest first
/// (pin names, `lab` values and instance names; "Hierarchical parameters").
[[nodiscard]] std::string substitute_from_parents(std::string_view text,
                                                  std::span<const PropertySource* const> parents);

/// Builds the element text of one instance.
[[nodiscard]] ElementText build_element(const ElementContext& context, const ElementOptions& options,
                                        ElementRunState& state);

/// `@symname` of a symbol reference: without directory and extension, each
/// run of space, `.`, `(`, `)` and `,` replaced by `_`, a trailing `_`
/// removed.
[[nodiscard]] std::string symbol_name(std::string_view reference);

}  // namespace hefesto::schem
