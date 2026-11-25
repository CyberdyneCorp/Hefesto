// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lvs` command, the layout source and cross-probing
// (openspec/changes/m9-lvs-simulation design.md D4, D16, "Area contracts"
// LVSCMD; capability lvs-command). The layout side is read through M7's
// entry points only (`ensure_extracted`, then `render_with_map` with the
// `lvs` preset); the user's `ext2spice` settings are never changed.
//
// Until LVSCMD lands, `lvs` fails with CORE-NOTIMPL-009. INTEGRATION calls
// register_lvs_commands from cmd::register_all_commands.
//
// Owner: LVSCMD (implementation team of the LVS clean room, design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/hier/run.hpp"
#include "hefesto/lvs/net/circuit.hpp"
#include "hefesto/netlist/spice/settings.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::lvs {

/// An object of a structured result to cross-probe from.
struct ObjectRef {
  enum class Kind : std::uint8_t { net, device, pin };
  Kind kind = Kind::net;
  std::size_t pair = 0;  // index in LvsResult::pairs
  int circuit = 1;       // 1 or 2
  std::string name;
};

/// A place in the layout: the use path from the root cell and a point on
/// the object.
struct LayoutRef {
  std::vector<std::string> use_path;
  db::Point point;
};

/// Adds `lvs` to `registry`; false on a duplicate (CMD-REGISTRY-001).
[[nodiscard]] bool register_lvs_commands(cmd::CommandRegistry& registry);

/// The layout side of `cell`: the `lvs`-preset netlist text as an in-memory
/// Source plus the netlister's name map.
[[nodiscard]] diag::Outcome<Circuit> layout_circuit(editor::EditorState& editor, std::string_view cell,
                                                    const netlist::spice::SpiceSettings& settings);

/// The layout places of `object` (report -> layout cross-probing).
[[nodiscard]] diag::Outcome<std::vector<LayoutRef>> cross_probe(const LvsResult& result, const ObjectRef& object);

}  // namespace hefesto::lvs
