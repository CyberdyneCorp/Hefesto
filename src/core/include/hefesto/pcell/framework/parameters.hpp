// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The parameter dictionary of a generated device
// (openspec/changes/m4-scripting/design.md D9; capability pcell-toolkit
// "Parameter dictionary model"): an ordered Tcl dictionary of parameter
// name -> string value, in the insertion order Tcl's `dict` keeps (which
// the naming hash and the saved properties depend on).
//
// Like a Tcl dict object it remembers the string it was parsed from (its
// string representation) until it is modified: Tcl keeps a value's
// original text through `dict merge` with an empty dictionary, and the
// `parameters` property stores that text verbatim (toolkit.tcl
// `property parameters $parameters`). to_tcl() therefore returns the
// original text while it is valid and Tcl's canonical form otherwise.
//
// Owner: PCELL (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::pcell {

class ParameterDict {
 public:
  using Entry = std::pair<std::string, std::string>;

  ParameterDict() = default;
  explicit ParameterDict(std::vector<Entry> entries);

  /// A dictionary from its Tcl string form (`dict create {*}$list` of the
  /// list `text`, keeping `text` as its string representation). Errors:
  /// PCELL-LIST-001 with Tcl's message (malformed list, or `missing value to
  /// go with key` for an odd count).
  [[nodiscard]] static diag::Outcome<ParameterDict> from_tcl(std::string_view text);

  /// The Tcl string form: the parsed text while unmodified, otherwise the
  /// canonical form (keys and values list-quoted as Tcl does).
  [[nodiscard]] std::string to_tcl() const;

  /// Sets `key` (keeping its position when present, appending otherwise),
  /// as Tcl's `dict set` (the string representation is dropped).
  void set(std::string_view key, std::string value);
  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const;
  [[nodiscard]] bool contains(std::string_view key) const;
  void erase(std::string_view key);
  /// `dict merge` of this and `other` (other's values win, new keys
  /// appended). Merging an empty `other` keeps this dictionary's string
  /// representation, as Tcl does.
  [[nodiscard]] ParameterDict merged(const ParameterDict& other) const;

  [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

  /// Same keys and values in the same order (the string representation is
  /// not compared).
  friend bool operator==(const ParameterDict& a, const ParameterDict& b) { return a.entries_ == b.entries_; }

 private:
  std::vector<Entry> entries_;
  std::optional<std::string> text_;  // the string representation, while valid
};

}  // namespace hefesto::pcell
