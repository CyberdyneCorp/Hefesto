// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro construction (capability lef-write; openspec/changes/
// m6-lef-def-oasis/design.md D7): the steps of lefWriteMacro
// (lefWrite.c:1149-2096) over a flat copy of the cell:
//   1. flatten: db::flatten_to_new of the cell's paint (every level, no
//      labels) into a scratch library; the labels are the cell's own;
//   2. type_sets: the route set R, hide set H, well set W, masterslice
//      planes P, boundary set and the antenna sets G and D (lef-write
//      "Macro source geometry and layer masks"; W, G and D read
//      extract::StyleView and are empty without an extraction style,
//      Q-LEFDEF-40);
//   3. write: the boundary (FIXED_BBOX, Q-LEFDEF-53) and the header lines,
//      then the pins in port-index order (Q-LEFDEF-50, 46), each from its
//      net (connect::search_hierarchical from the port label), with the
//      antenna areas, the `-hide`, `-pinonly` and `-toplayer` passes and
//      the PORT records tile by tile in Magic's order (Q-LEFDEF-39, 52);
//      then the obstructions (the hide passes, Q-LEFDEF-38) and the
//      PROPERTY lines.
// Nothing here touches the cell being written or the session: everything
// the write reads is in the MacroContext.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ and reshaped the class
// (design D2).

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lef/write/number.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::extract {
class StyleView;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::lef {

class LayerTable;

/// The type sets of step 2 (lef-write "Macro source geometry and layer
/// masks").
struct TypeSets {
  tech::TypeMask route;              // R: route, cut and (domaster) masterslice types, port label types
  tech::TypeMask hide;               // H: route types that also have an obstruction type
  tech::TypeMask well;               // W: masterslice types that are not substrate types
  tech::PlaneMask master_planes = 0;  // P: the planes of the masterslice types (domaster)
  tech::TypeMask boundary;           // the types of `bound` records
  tech::TypeMask gate;               // G: MOSFET-class device types
  tech::TypeMask diffusion;          // D: antenna tie-down types
};

/// Everything one macro write reads from the session.
struct MacroContext {
  const tech::Technology* tech = nullptr;
  const LayerTable* layers = nullptr;
  const drc::RuleSet* rules = nullptr;         // the DRC style (wide and default spacing); nullptr: none
  const extract::StyleView* style = nullptr;   // the extraction style; nullptr: none
  float oscale = 1.0F;                         // CIFGetOutputScale(1000)
  LefUnits units;                              // the rounding grid of every distance
  bool magic_compat = true;
  std::vector<std::string> nocheck;            // `lef nocheck`
  std::optional<std::string> vdd;              // "global nets and power names" (Q-LEFDEF-44)
  std::optional<std::string> gnd;
};

/// The abstract options of one macro (lefWriteMacro's arguments).
struct MacroOptions {
  db::Coord setback = -1;   // -hide: < 0 disabled
  db::Coord pin_only = -1;  // -pinonly: < 0 disabled
  bool top_layer = false;   // -toplayer
  bool do_master = true;    // -nomaster clears it
};

namespace write_detail {
struct MacroState;
}

/// One macro under construction. flatten() and type_sets() run before
/// write(); write() runs them when they have not run.
class MacroBuilder {
 public:
  MacroBuilder(const MacroContext& context, const db::CellDef& cell, const MacroOptions& options);
  ~MacroBuilder();
  MacroBuilder(const MacroBuilder&) = delete;
  MacroBuilder& operator=(const MacroBuilder&) = delete;

  /// Step 1: the flat copy of the cell's paint.
  [[nodiscard]] diag::Status flatten();
  /// Step 2: the type sets.
  void type_sets();
  /// Steps 3-6: appends the MACRO block, `MACRO <name>` to `END <name>`,
  /// to `out`; the write's messages go to `messages`.
  [[nodiscard]] diag::Status write(std::string& out, std::vector<diag::Diagnostic>& messages);

  [[nodiscard]] const TypeSets& sets() const noexcept;

 private:
  std::unique_ptr<write_detail::MacroState> state_;
};

}  // namespace hefesto::lef
