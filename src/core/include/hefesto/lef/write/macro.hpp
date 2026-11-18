// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro construction (capability lef-write; openspec/changes/
// m6-lef-def-oasis/design.md D7): the steps of lefWriteMacro
// (lefWrite.c:1037-2190) over a flat copy of the cell:
//   1. flatten: db::flatten_to_new into a scratch library, with the labels
//      of the cell itself only;
//   2. type_sets: the route set R, hide set H, well set W, masterslice
//      planes P, gate set G and diffusion set D (lef-write "Macro source
//      geometry and layer masks"; W, G and D read extract::StyleView, and
//      are empty without an extraction style, Q-LEFDEF-40);
//   3. pins: the pin nets (connect::search_flat from each port label),
//      enumerated in port-index order;
//   4. antenna: the gate and diffusion areas of each pin;
//   5. geometry: the PORT, OBS and contact-cut records, tile by tile in
//      Magic's tile order (Q-LEFDEF-39).
// `-hide`, `-pinonly`, `-toplayer` and `-nomaster` are separate passes over
// the flat copy, between steps 3 and 5.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFW fills
// in the implementation in src/core/src/lef/write/ and may reshape the
// class (design D2).

#pragma once

#include <string>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lef {

/// The type sets of step 2 (lef-write "Macro source geometry and layer
/// masks").
struct TypeSets {
  tech::TypeMask route;        // R: types with a ROUTE or CUT record
  tech::TypeMask hide;         // H: types hidden by -hide
  tech::TypeMask well;         // W: masterslice types that are not substrate
  tech::TypeMask masterslice;  // P: the types on masterslice planes
  tech::TypeMask gate;         // G: MOSFET-class device types
  tech::TypeMask diffusion;    // D: antenna tie-down types
};

/// One macro under construction. Each step returns an error that stops the
/// write; SCAFFOLD: every step fails with CORE-NOTIMPL-006 ("not implemented
/// yet: lef write") until LEFW lands it.
class MacroBuilder {
 public:
  MacroBuilder(editor::EditorState& editor, db::CellDef& cell) noexcept : editor_(&editor), cell_(&cell) {}

  [[nodiscard]] diag::Status flatten();
  [[nodiscard]] diag::Status type_sets();
  [[nodiscard]] diag::Status pins();
  [[nodiscard]] diag::Status antenna();
  /// Appends the MACRO's PIN, OBS and geometry text to `out`.
  [[nodiscard]] diag::Status geometry(std::string& out);

  [[nodiscard]] editor::EditorState& editor() const noexcept { return *editor_; }
  [[nodiscard]] db::CellDef& cell() const noexcept { return *cell_; }
  [[nodiscard]] const TypeSets& sets() const noexcept { return sets_; }

 private:
  editor::EditorState* editor_;
  db::CellDef* cell_;
  TypeSets sets_;
};

}  // namespace hefesto::lef
