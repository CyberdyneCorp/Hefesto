// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtCompareStyle's units (see style_units.hpp).
// Owner: SPICE (openspec/changes/m7-extraction/design.md D3), shared with SIM.

#include "netlist/spice/style_units.hpp"

#include <cstdint>

namespace hefesto::netlist::detail {

std::optional<float> style_units(const editor::EditorState& editor, std::string_view name) {
  const auto& current = editor.extract.current;
  if (current != nullptr && current->name == name) {
    return current->units_per_lambda;  // strcmp(stylename, ExtCurStyle->exts_name) == 0
  }
  const auto parsed = editor.extract.styles.select(name);
  if (parsed == nullptr) {
    return std::nullopt;
  }
  // ExtLoadStyle: extTechFinalStyle's conversion, then ExtTechScale(DBLambda[0], DBLambda[1]).
  extract::ExtStyle style = parsed->micron_convert(editor.mask.output_styles.current_style());
  if (editor.grid.internal != editor.grid.lambda) {
    style = style.rescale(static_cast<std::int32_t>(editor.grid.lambda), static_cast<std::int32_t>(editor.grid.internal),
                          editor.drc.compat_settings());
  }
  return style.units_per_lambda;
}

}  // namespace hefesto::netlist::detail
