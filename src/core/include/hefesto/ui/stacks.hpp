// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The session-global box and edit stacks (capability
// ui-box-stack-and-measure, requirements "Box stack push", "Box stack pop
// and peek", "Edit stack push", "Edit stack pop and clear"). Magic keeps
// them as Tcl lists in tools.tcl; Hefesto keeps the same records natively
// in editor::EditorState so `pushbox`/`popbox`/`peekbox` and
// `pushstack`/`popstack`/`clearstack` behave identically without Tcl.
// (Magic ref: tcltk/tools.tcl:122-212)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string>
#include <vector>

namespace hefesto::ui {

/// Box records `{llx lly urx ury}` (internal units) or the verbatim argument
/// of `pushbox VALUES`, oldest first. `created` tells "never pushed" (`No
/// stack`) from "pushed and emptied again" (`Empty stack`), as the Tcl
/// global `boxstack` does.
struct BoxStack {
  bool created = false;
  std::vector<std::string> records;

  [[nodiscard]] bool empty() const noexcept { return records.empty(); }
};

/// Pairs of entries pushed by `pushstack`: the view (`view get` in internal
/// units), then the window's root cell name; oldest first.
using EditStack = std::vector<std::string>;

}  // namespace hefesto::ui
