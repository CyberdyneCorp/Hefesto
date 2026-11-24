// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFFlatBuild, EFFlatBuildOneLevel and the visits (capability
// ext-flattening-hierarchy "Container iteration order", "Flat node
// construction", "Node merge semantics", "Kills", "Global name merging",
// "Coupling capacitance flattening", "Visit orders for output", "Lumped
// node resistance", "Device length and width", "One-level flattening for
// hierarchical output"; ext-flattening-names "Instance and array element
// names", "Global and ground detection"): efFlatNodesStdCell, efFlatNodes,
// efFlatNodesDeviceless, efFlatKills, efFlatGlob, efFlatCaps, then
// EFVisitDevs, EFVisitResists, the coupling table and EFVisitNodes (or
// their EFHierVisit* forms).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/read.hpp"
#include "state.hpp"

namespace hefesto::extflat::detail {

/// What a flattening reads of the finished read, and where its messages go.
struct FlatContext {
  diag::DiagnosticSink& sink;
  const ReadOptions& options;
  const std::unordered_set<std::string>& globals;
  const DefTable& defs;
  const LayerNames& layers;
  std::optional<diag::Diagnostic> fatal;  // a cap or a cycle: the flattening stops

  [[nodiscard]] bool is_global(std::string_view leaf) const {
    return (options.bang_globals && !leaf.empty() && leaf.back() == '!') || globals.count(std::string{leaf}) != 0;
  }
  void message(std::string_view code, std::string text) const;
  /// Records the first fatal condition (later ones are not reported).
  void fail(std::string_view code, std::string text);
};

/// The settings of one flattening besides the context.
struct FlatPlan {
  FlatFlags flags = FlatFlags::none;
  FlattenOptions options;
  bool substrate_compat = true;        // EFCompat after the read
  std::vector<std::int32_t> resists;   // efResists
  bool scale_changed = false;          // efScaleChanged
};

/// The M6 entry: EF_FLATNODES | EF_NOFLATSUBCKT, nodes only, in
/// EFVisitNodes order (killed nodes left out). A cap or a use cycle sets
/// `st.fatal`.
[[nodiscard]] std::vector<FlatNode> flatten_nodes(ReadState& st, std::uint32_t root);

/// EFFlatBuild of `root` with the plan's flags and the visits.
[[nodiscard]] Netlist flatten_full(FlatContext& ctx, std::uint32_t root, const FlatPlan& plan);

/// EFFlatBuildOneLevel of `def` and the EFHierVisit* visits; `nodevices`
/// holds DEF_NODEVICES per def across flattenings, as Magic's def flags do.
[[nodiscard]] Netlist flatten_level(FlatContext& ctx, std::uint32_t def, const FlatPlan& plan,
                                    std::vector<char>& nodevices);

/// EFNodeResist over area/perimeter pairs and the class resistances.
[[nodiscard]] std::int32_t node_resistance(const std::vector<PerimArea>& pa, const std::vector<std::int32_t>& resists);

/// EFGetLengthAndWidth (with efDevFixLW's gate attributes and symbols).
struct LengthWidth {
  std::int32_t length = 0;
  std::int32_t width = 0;
};
[[nodiscard]] LengthWidth device_length_width(const Device& dev, const std::vector<DevTerm>& terms,
                                              const std::map<std::string, std::int32_t, std::less<>>& symbols);

}  // namespace hefesto::extflat::detail
