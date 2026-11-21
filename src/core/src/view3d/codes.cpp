// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "view3d" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m10-3d-view/design.md
// D2):
//
//   view3d/stack/codes.inc      VIEW3D-STACK-*    TECH
//   view3d/mesh/codes.inc       VIEW3D-MESH-*     MESH
//   view3d/camera/codes.inc     VIEW3D-CAMERA-*   CAMERA
//   view3d/client/codes.inc     VIEW3D-CLIENT-*   WIND3D
//   view3d/pick/codes.inc       VIEW3D-PICK-*     INSPECT
//   view3d/section/codes.inc    VIEW3D-SECTION-*  INSPECT (P2)
//   view3d/export/codes.inc     VIEW3D-EXPORT-*   EXPORT (P2)
//   view3d/overlay/codes.inc    VIEW3D-OVERLAY-*  OVERLAY (P3)
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_VIEW3D_CODE("VIEW3D-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/view3d.md with `hefesto-cli --diagnostic-codes=view3d`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_VIEW3D_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "view3d/stack/codes.inc"
#include "view3d/mesh/codes.inc"
#include "view3d/camera/codes.inc"
#include "view3d/client/codes.inc"
#include "view3d/pick/codes.inc"
#include "view3d/section/codes.inc"
#include "view3d/export/codes.inc"
#include "view3d/overlay/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_VIEW3D_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a VIEW3D- code is declared twice");

}  // namespace

std::span<const CodeInfo> view3d() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
