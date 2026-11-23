// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Instance attributes, templates, unique instance names and floater texts
// (capability schematic-symbols-and-libraries "Templates and instance
// attributes", "Instance names", "Floater texts").
//
// Owner: SYMLIB.

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem {

/// The decoded `template` of the symbol property; empty when there is none.
[[nodiscard]] std::string symbol_template(const Symbol& symbol);

/// The value `@key` reads for an instance: the instance property string
/// first, then the symbol template. nullopt when neither defines `key`.
[[nodiscard]] std::optional<std::string> instance_attribute(std::string_view instance_props, const Symbol& symbol,
                                                            std::string_view key);

/// Unique instance names of one schematic ("Instance names"): names compare
/// ASCII case-insensitively after bus expansion, and a used name is renamed
/// to `<base><q><suffix>` from the last index used for that base and suffix.
///
/// The bits a namer keeps are taken from `budget` when one is given (the
/// editing session's Session::bus_budget, Q-SCHEM-41): past it a name is
/// kept as written, unexpanded, like a name over kMaxBusBytes, so the
/// names of a document stay bounded in total. `budget` must outlive the
/// namer; nullptr bounds each name by kMaxBusBytes only.
class InstanceNamer {
 public:
  explicit InstanceNamer(BusBudget* budget = nullptr) noexcept : budget_{budget} {}
  /// Records the `name` of every instance of `document`.
  explicit InstanceNamer(const Document& document, BusBudget* budget = nullptr);

  /// Records `name` as used (each bit of a bus name, under the budget).
  void add(std::string_view name);

  /// True when `name`, or one of its bits, is used.
  [[nodiscard]] bool used(std::string_view name) const;

  /// `name` when it is unused, else the renamed form; the result is recorded
  /// as used. An empty name stays empty.
  [[nodiscard]] std::string unique(std::string_view name);

  /// `props` with its `name` made unique by unique(); unchanged when it has
  /// no `name`.
  [[nodiscard]] std::string unique_properties(std::string_view props);

 private:
  BusBudget* budget_ = nullptr;  // not owned
  std::set<std::string> used_;   // ASCII-lowercased bit names
  std::map<std::pair<std::string, std::string>, long long> last_index_;
};

/// The template of `symbol` as the property string of a new instance
/// ("Templates and instance attributes"), with its `name`, when it has one,
/// made unique by `names`.
[[nodiscard]] std::string new_instance_properties(const Symbol& symbol, InstanceNamer& names);

/// The base and bracket suffix of an instance name ("Instance names" step 1):
/// the characters before the first digit or `[`, and the text from the first
/// `[` to the end (empty when there is none).
[[nodiscard]] std::pair<std::string, std::string> split_instance_name(std::string_view name);

/// What a schematic text is as a floater ("Floater texts").
struct FloaterInfo {
  /// The text has a `name` or `floater` key.
  bool floater = false;
  /// The owner instance name: the `name` value, else the `floater` value
  /// (`true` when that value is true).
  std::string owner;
};

[[nodiscard]] FloaterInfo floater_info(const TextRecord& text);

/// The owner of a floater ("Floater texts"): for an `owner` made only of
/// decimal digits, the instance with that 0-based index; otherwise the
/// instance whose `name` equals `owner` byte for byte (case-sensitive).
/// nullptr when there is none.
[[nodiscard]] const InstanceRecord* floater_owner(const Document& document, std::string_view owner);

/// For a floater with no owner instance: whether the text is still resolved
/// with no instance (it starts with `tcleval(` or contains `@` or `%`).
[[nodiscard]] bool resolve_without_owner(std::string_view text) noexcept;

}  // namespace hefesto::schem
