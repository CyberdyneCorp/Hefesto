// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Picking and highlight sets (openspec/changes/m10-3d-view/design.md D10).
// Scaffold stubs (M10 task 2.3): resolve finds nothing, to_selection
// changes nothing, the highlight sets are empty. INSPECT replaces them in
// tasks 9.3-9.5.
//
// Owner: INSPECT (design D3).

#include "hefesto/view3d/pick/pick.hpp"

namespace hefesto::view3d {

std::optional<PickTarget> resolve(const MeshModel& /*mesh*/, PickId /*id*/) {
  return std::nullopt;
}

diag::Status to_selection(editor::EditorState& /*editor*/, const PickTarget& /*target*/) {
  return diag::Done{};
}

HighlightSet from_selection(const MeshModel& /*mesh*/, const select::Selection& /*selection*/) {
  return HighlightSet{};
}

HighlightSet net_highlight(const MeshModel& /*mesh*/, const editor::EditorState& /*editor*/,
                           const PickTarget& /*seed*/) {
  return HighlightSet{};
}

}  // namespace hefesto::view3d
