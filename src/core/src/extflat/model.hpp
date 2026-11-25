// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchical circuit model the `.ext` reader builds (capability
// ext-flattening-read): one Def per cell with its nodes, names, uses,
// merge connections, couplings, resistors, devices and kills (Magic's Def,
// Use, Connection, Dev, Kill of extflat/EFint.h).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

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
/// GeoTransRect: the image of `r`, normalised, saturated to 32 bits.
[[nodiscard]] db::Rect transform_rect(const Transform& t, const db::Rect& r) noexcept;

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

/// A `merge`, `cap` or `resist` record (Connection): the two names, the
/// capacitance delta or coupling (aF), the area/perimeter deltas of a
/// merge and the resistance of a resistor (mohm).
struct Conn {
  ConnName name1;
  ConnName name2;
  float cap = 0.0F;
  std::vector<PerimArea> pa;
  float res = 0.0F;
};

/// DevTerm: one device terminal.
struct DevTerm {
  std::uint32_t node = kNil;  // local node (NodeGraph::find gives the live one)
  std::int32_t length = 0;
  std::optional<std::string> attrs;  // nullopt for the token "0"
};

/// Dev: one device record.
struct Device {
  std::uint8_t device_class = 0;
  std::string type;
  db::Rect rect;  // the 1x1 rectangle's corners after locScale
  float cap = 0.0F;
  float res = 0.0F;
  std::int32_t area = 0;
  std::int32_t perim = 0;
  std::int32_t length = 0;
  std::int32_t width = 0;
  std::uint32_t subs = kNil;  // dev_subsnode (local node)
  std::vector<DevTerm> terms;
  std::vector<std::string> params;  // dev_params: verbatim, newest first
  std::int32_t tile_type = 0;       // the device table key's type index
};

/// DevParam of a `parameters` record.
struct DevParam {
  std::string type;  // parm_type: the key's first one or two characters
  std::string name;  // parm_name
  double scale = 1.0;
  std::int32_t offset = 0;
};

struct Def {
  std::string name;
  bool available = false;   // DEF_AVAILABLE: its file was (being) read
  bool subcircuit = false;  // DEF_SUBCIRCUIT
  bool abstract = false;    // DEF_ABSTRACT
  bool primitive = false;   // DEF_PRIMITIVE
  float scale = 1.0F;       // def_scale
  NodeGraph graph;
  /// def_nodes: name string -> name entry. Never iterated in an order
  /// that reaches the output.
  std::unordered_map<std::string, std::uint32_t> names;
  /// def_nodes' iteration order (Magic's string-keyed table, requested size
  /// INITNODESIZE 32): every key of `names`, inserted when the name is
  /// first entered. topVisit walks the ports in this order.
  util::MagicHashTable<char> name_order{32};
  /// def_uses, iterated in Magic's hash-table order (requested size 32).
  util::MagicHashTable<std::uint32_t> use_table{32};
  std::vector<Use> uses;     // storage; use_table values index it
  std::vector<Conn> conns;   // file order (Magic prepends: walk backwards)
  std::vector<Conn> caps;    // file order (Magic prepends: walk backwards)
  std::vector<Conn> resistors;     // file order (Magic prepends: walk backwards)
  std::vector<std::string> kills;  // file order (Magic prepends: walk backwards)
  /// def_devs: "<x>x<y>_<t>" -> index into `devices`, iterated in Magic's
  /// hash-table order (requested size 32).
  util::MagicHashTable<std::uint32_t> dev_table{32};
  std::vector<Device> devices;

  /// The devices in def_devs iteration order.
  template <typename F>
  void for_each_device(F&& visit) const {
    dev_table.for_each([&](const auto& entry) {
      if (entry.value) {
        visit(devices[*entry.value]);
      }
    });
  }
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
  [[nodiscard]] const Def& at(std::uint32_t id) const { return *defs_[id]; }
  [[nodiscard]] std::size_t size() const noexcept { return defs_.size(); }

 private:
  std::vector<std::unique_ptr<Def>> defs_;  // stable addresses
  std::unordered_map<std::string, std::uint32_t> index_;
};

}  // namespace hefesto::extflat::detail
