// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchical circuit model the `.ext` reader builds (capability
// ext-flattening-read): one Def per cell with its nodes, names, uses,
// merge connections and kills (Magic's Def, Use, Connection, Kill of
// extflat/EFint.h). Devices keep only what decides whether a later record
// overlaps them; their parameters, capacitances and resistances are not
// kept (design D10).
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "graph.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::extflat::detail {

/// Magic's Transform (a b c / d e f): x' = a x + b y + c, y' = d x + e y + f.
struct Transform {
  std::int32_t a = 1, b = 0, c = 0, d = 0, e = 1, f = 0;
};

/// GeoTransTrans(first, second): `first` then `second`, saturated to 32 bits.
[[nodiscard]] Transform compose(const Transform& first, const Transform& second) noexcept;
/// GeoTransTranslate(x, y, t): translate by (x, y), then `t`.
[[nodiscard]] Transform translate_then(std::int64_t x, std::int64_t y, const Transform& t) noexcept;
/// GeoTransRect of the 1x1 rectangle at `ll`: the image's lower-left corner.
[[nodiscard]] db::Point transform_unit_rect(const Transform& t, db::Point ll) noexcept;

struct Use {
  std::uint32_t def = 0;
  std::string id;  // without the array subscript
  Transform trans;
  std::int32_t xlo = 0, xhi = 0, xsep = 0;
  std::int32_t ylo = 0, yhi = 0, ysep = 0;

  [[nodiscard]] bool is_array() const noexcept { return xlo != xhi || ylo != yhi; }
};

/// One subscript range of a connection name ([lo:hi]).
struct Range {
  std::int32_t lo = 0;
  std::int32_t hi = 0;
};

/// ConnName: a name whose subscript ranges were replaced by one `[%d]` or
/// `[%d,%d]` (efConnBuildName). Kept as the text around it instead of a
/// printf format, so a `%` in a name is plain text.
struct ConnName {
  std::string before;  // the name up to the first range
  std::string after;   // the rest after the ranges
  std::size_t nsubs = 0;
  std::array<Range, 2> subs{};

  /// The name of element (i, j) (j unused below two subscripts).
  [[nodiscard]] std::string element(std::int64_t i, std::int64_t j) const;
};

/// A `merge` record (Connection without its capacitance adjustments).
struct Conn {
  ConnName name1;
  ConnName name2;
};

/// What a device record keeps: enough to tell a compatible duplicate.
struct DeviceKey {
  std::uint8_t device_class = 0;
  std::string type;
  std::size_t terminals = 0;
};

struct Def {
  std::string name;
  bool available = false;   // DEF_AVAILABLE: its file was (being) read
  bool subcircuit = false;  // DEF_SUBCIRCUIT
  NodeGraph graph;
  /// def_nodes: name string -> name entry. Never iterated in an order
  /// that reaches the output.
  std::unordered_map<std::string, std::uint32_t> names;
  /// def_uses, iterated in Magic's hash-table order (requested size 32).
  util::MagicHashTable<std::uint32_t> use_table{32};
  std::vector<Use> uses;     // storage; use_table values index it
  std::vector<Conn> conns;   // file order (Magic prepends: walk backwards)
  std::vector<std::string> kills;  // file order (Magic prepends: walk backwards)
  std::unordered_map<std::string, DeviceKey> devices;
};

/// EFLayerNames: index 0 is "space".
class LayerNames {
 public:
  LayerNames() { add("space"); }
  std::uint32_t add(std::string_view name);
  [[nodiscard]] const std::string& at(std::uint32_t index) const { return names_[index]; }

 private:
  std::vector<std::string> names_;
  std::unordered_map<std::string, std::uint32_t> index_;
};

/// Every cell of one read (efDefLook / efDefNew), by name.
class DefTable {
 public:
  /// The def named `name`, created (unavailable) when absent.
  std::uint32_t lookup_or_create(std::string_view name);
  [[nodiscard]] std::optional<std::uint32_t> lookup(std::string_view name) const;
  [[nodiscard]] Def& at(std::uint32_t id) { return *defs_[id]; }
  [[nodiscard]] std::size_t size() const noexcept { return defs_.size(); }

 private:
  std::vector<std::unique_ptr<Def>> defs_;  // stable addresses
  std::unordered_map<std::string, std::uint32_t> index_;
};

}  // namespace hefesto::extflat::detail
