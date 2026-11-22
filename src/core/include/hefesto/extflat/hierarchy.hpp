// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The general `.ext` read and flatten (capabilities ext-flattening-read,
// ext-flattening-hierarchy; openspec/changes/m7-extraction/design.md D5,
// D13): read() is EFReadFile over the whole grammar, keeping the
// hierarchy; flatten() is EFFlatBuild with the given flags, choosing the
// efFlatNodes or efFlatNodesStdCell path as Magic does. M6's read_flat()
// (read.hpp) stays a wrapper with EF_FLATNODES | EF_NOFLATSUBCKT, so DEF
// write is unchanged. Hierarchy::visit_defs and visit_uses are EFHierVisit*
// for the hierarchical netlister; read_res_overlay() reads a `.res.ext`
// overlay ("Resistance-extraction overlay").
//
// Scaffold state (tasks.md 3.3): read() and read_res_overlay() report
// CORE-NOTIMPL-008, flatten() returns an empty netlist and the visitors
// visit nothing until EXTFLAT lands them.
//
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/outcome.hpp"
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

/// One cell definition read from its `.ext` file (EFDef), opaque to the
/// netlisters except through the visitors.
struct HierDef {
  std::string name;
  bool subcircuit = false;  // a cell with ports, netlisted as a subcircuit
  bool abstract = false;    // the `abstract` record
};

/// One `use` of a definition inside another (EFUse).
struct HierUse {
  std::string parent;
  std::string def;
  std::string id;
};

/// The hierarchy read from a root cell's `.ext` files.
class Hierarchy {
 public:
  /// Calls `visitor` for each definition, children before parents
  /// (EFHierVisitSubcircuits order).
  void visit_defs(const std::function<void(const HierDef&)>& visitor) const {
    for (const HierDef& def : defs) {
      visitor(def);
    }
  }
  /// Calls `visitor` for each use of `parent`, in the file's order.
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
};

/// Reads `<root>.ext` and every file it uses (EFReadFile).
[[nodiscard]] diag::Outcome<Hierarchy> read(cell::LookupContext& ctx, std::string_view root,
                                            const ReadOptions& options);

/// EFFlatBuild of the hierarchy with `flags`.
[[nodiscard]] Netlist flatten(const Hierarchy& hierarchy, FlatFlags flags);

/// One record of a `.res.ext` overlay (`rnode`, `resist`, `device`
/// rewrites), kept as its tokens.
struct OverlayRecord {
  std::vector<std::string> tokens;
};

/// Reads `<cell>.res.ext` (ext-flattening-read "Resistance-extraction
/// overlay").
[[nodiscard]] diag::Outcome<std::vector<OverlayRecord>> read_res_overlay(cell::LookupContext& ctx,
                                                                         std::string_view cell,
                                                                         const ReadOptions& options);

}  // namespace hefesto::extflat
