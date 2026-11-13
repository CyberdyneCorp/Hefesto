// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): the
// generators report CORE-NOTIMPL-005 (to the context's sink, or as the
// failed outcome) and generate nothing until GEN ports CIFgen/CIFhier
// (tasks.md section 6). Owner: GEN.

#include "hefesto/mask/gen/generate.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

namespace {

LayerPlanes not_generated(GenerationContext& ctx, std::string_view what) {
  if (ctx.sink != nullptr) {
    ctx.sink->emit(cmd::not_implemented_diagnostic(what));
  }
  return {};
}

}  // namespace

LayerPlanes generate_flat(editor::EditorState& /*editor*/, const db::CellDef& /*cell*/, const db::Rect& /*area*/,
                          const OutputStyle& /*style*/, GenerationContext& ctx) {
  return not_generated(ctx, "mask::generate_flat");
}

LayerPlanes generate_hierarchical(editor::EditorState& /*editor*/, const db::CellDef& /*cell*/,
                                  const OutputStyle& /*style*/, GenerationContext& ctx) {
  return not_generated(ctx, "mask::generate_hierarchical");
}

diag::Outcome<std::vector<db::Rect>> see(editor::EditorState& /*editor*/, const db::CellDef& /*cell*/,
                                         std::string_view /*layer*/, const db::Rect& /*area*/,
                                         GenerationContext& /*ctx*/) {
  return cmd::not_implemented_diagnostic("mask::see");
}

diag::Status paint(editor::EditorState& /*editor*/, const db::CellDef& /*cell*/, std::string_view /*layer*/,
                   const db::Rect& /*area*/, tech::TileType /*type*/, db::CellDef& /*target*/,
                   GenerationContext& /*ctx*/) {
  return cmd::not_implemented_status("mask::paint");
}

diag::Outcome<Coverage> coverage(editor::EditorState& /*editor*/, const db::CellDef& /*cell*/,
                                 std::string_view /*layer*/, const db::Rect& /*area*/, GenerationContext& /*ctx*/) {
  return cmd::not_implemented_diagnostic("mask::coverage");
}

std::string statistics(const GenerationStatistics& /*stats*/) {
  return {};
}

diag::Outcome<std::vector<db::Rect>> interaction_areas(const db::CellDef& /*cell*/, std::int64_t /*halo*/,
                                                       const compat::CompatSettings& /*compat*/) {
  return cmd::not_implemented_diagnostic("mask::interaction_areas");
}

}  // namespace hefesto::mask
