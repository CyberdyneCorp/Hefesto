// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The general `.ext` read and flatten (capabilities ext-flattening-read,
// ext-flattening-hierarchy; openspec/changes/m7-extraction/design.md D5,
// D13): read() is EFReadFile over the whole grammar, keeping the
// hierarchy; flatten() is EFFlatBuild with the given flags, choosing the
// efFlatNodes or efFlatNodesStdCell path as Magic does, then EFVisitDevs,
// EFVisitResists, the coupling table and EFVisitNodes. flatten_one_level()
// is EFFlatBuildOneLevel with the EFHierVisit* visits for the hierarchical
// netlister. M6's read_flat() (read.hpp) stays a wrapper with
// EF_FLATNODES | EF_NOFLATSUBCKT, so DEF write is unchanged.
// Hierarchy::visit_defs and visit_uses are EFHierSrDefs and efHierSrUses
// for the hierarchical netlister; read_res_overlay() reads a `.res.ext`
// overlay's records ("Resistance-extraction overlay"); read() folds the
// overlays in itself when ReadOptions::resist_overlay is set.
//
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/read.hpp"

namespace hefesto::extflat {

/// EFFlatBuild's flags (extflat.h), as a bit set.
enum class FlatFlags : std::uint32_t {
  none = 0,
  flat_nodes = 1U << 0,       // EF_FLATNODES
  flat_caps = 1U << 1,        // EF_FLATCAPS
  flat_resists = 1U << 2,     // EF_FLATRESISTS
  no_flat_subckt = 1U << 3,   // EF_NOFLATSUBCKT
  no_name_merge = 1U << 4,    // EF_NONAMEMERGE
  warn_abstract = 1U << 5,    // EF_WARNABSTRACT
  trim_global = 1U << 6,      // EF_TRIMGLOB
  trim_local = 1U << 7,       // EF_TRIMLOCAL
};

[[nodiscard]] constexpr FlatFlags operator|(FlatFlags a, FlatFlags b) noexcept {
  return static_cast<FlatFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr bool has(FlatFlags set, FlatFlags flag) noexcept {
  return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(flag)) != 0;
}

namespace detail {
struct HierModel;  // the read circuit model (src/core/src/extflat/)
}  // namespace detail

/// One cell definition read from its `.ext` file (EFDef), opaque to the
/// netlisters except through the visitors.
struct HierDef {
  std::string name;
  bool subcircuit = false;  // a cell with ports, netlisted as a subcircuit
  bool abstract = false;    // the `abstract` record
  bool primitive = false;   // the `primitive` record
  std::size_t devices = 0;  // the number of device records kept
};

/// One `use` of a definition inside another (EFUse).
struct HierUse {
  std::string parent;
  std::string def;
  std::string id;  // without the array subscript
  /// The transform (a b c / d e f) and the array ranges (lo, hi, sep);
  /// lo == hi on an axis that is not arrayed.
  std::array<std::int32_t, 6> transform{1, 0, 0, 0, 1, 0};
  std::array<std::int32_t, 3> x_range{};
  std::array<std::int32_t, 3> y_range{};
};

/// The hierarchy read from a root cell's `.ext` files.
class Hierarchy {
 public:
  /// Calls `visitor` for each definition reachable from the root, children
  /// before parents (EFHierSrDefs order: each definition once, after the
  /// definitions it uses, uses in use-table order; the root last).
  void visit_defs(const std::function<void(const HierDef&)>& visitor) const {
    for (const HierDef& def : defs) {
      visitor(def);
    }
  }
  /// Calls `visitor` for each use of `parent`, in use-table order
  /// (efHierSrUses).
  void visit_uses(std::string_view parent, const std::function<void(const HierUse&)>& visitor) const {
    for (const HierUse& use : uses) {
      if (use.parent == parent) {
        visitor(use);
      }
    }
  }

  std::string root;
  std::vector<HierDef> defs;
  std::vector<HierUse> uses;
  /// The circuit model flatten() works on; null for a default-constructed
  /// hierarchy, which flattens to an empty netlist.
  std::shared_ptr<const detail::HierModel> model{};
};

/// Reads `<root>.ext` and every file it uses (EFReadFile). Read messages
/// go to `ctx.sink`.
[[nodiscard]] diag::Outcome<Hierarchy> read(cell::LookupContext& ctx, std::string_view root,
                                            const ReadOptions& options);

/// The flattening settings that are not EFFlatBuild flags.
struct FlattenOptions {
  /// EFCapThreshold (fF): couplings below it whose names do not resolve
  /// are dropped silently (efFlatSingleCap).
  double cap_threshold = 2.0;
  /// The `gnd` field of the project setting "global nets and power names"
  /// (Magic's $GND): with GND!, the substrate in compatibility mode.
  std::string gnd;
  /// The `-s`/`-S` symbols that `ext:l=`/`ext:w=` gate attributes of a
  /// legacy `fet` may name (efSymLook).
  std::map<std::string, std::int32_t, std::less<>> symbols;
  /// The DEF_SUBCIRCUIT flags the device, resistor and subcircuit visits
  /// see: as read; with the root's cleared (ext2spice after EFFlatBuild);
  /// or none (subcktUndef, when no subcircuit calls are written). With
  /// EF_NOFLATSUBCKT the root's flag is always cleared, as EFFlatBuild does.
  enum class Subcircuits : std::uint8_t { as_read, root_cleared, none };
  Subcircuits subcircuits = Subcircuits::as_read;
};

/// EFFlatBuild of the hierarchy with `flags`, then the visits. Flattening
/// messages (unresolved names, abstract views, split globals) go to `sink`.
/// Fails when a cap of ReadOptions::limits is reached or a use cycle is
/// found.
[[nodiscard]] diag::Outcome<Netlist> flatten(const Hierarchy& hierarchy, FlatFlags flags,
                                             const FlattenOptions& options, diag::DiagnosticSink& sink);

/// EFFlatBuild of the hierarchy with `flags` and the default options; the
/// messages are dropped and a failure gives an empty netlist.
[[nodiscard]] Netlist flatten(const Hierarchy& hierarchy, FlatFlags flags);

/// EFFlatBuildOneLevel of definition `def` (ext-flattening-hierarchy
/// "One-level flattening for hierarchical output") and the EFHierVisit*
/// visits: the definition's own devices and resistors, the couplings of it
/// and of its absorbed device-less children, and the flat nodes. Child
/// nodes carry no capacitance or area/perimeter (FLATNODE_HIER).
/// `devices_absent` receives DEF_NODEVICES of the definition.
[[nodiscard]] diag::Outcome<Netlist> flatten_one_level(const Hierarchy& hierarchy, std::string_view def,
                                                       FlatFlags flags, const FlattenOptions& options,
                                                       diag::DiagnosticSink& sink, bool* devices_absent = nullptr);

/// One record of a `.res.ext` overlay (`rnode`, `resist`, `device`
/// rewrites), kept as its tokens.
struct OverlayRecord {
  std::vector<std::string> tokens;
  std::uint32_t line = 0;  // the last physical line of the record
};

/// Reads `<cell>.res.ext` from the three places of `<cell>.ext`
/// (ext-flattening-read "Resistance-extraction overlay"), as efReadLine
/// splits it. Fails with EXTFLAT-READ-001 when no overlay is found.
[[nodiscard]] diag::Outcome<std::vector<OverlayRecord>> read_res_overlay(cell::LookupContext& ctx,
                                                                         std::string_view cell,
                                                                         const ReadOptions& options);

}  // namespace hefesto::extflat
