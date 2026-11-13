// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3):
// mask_layer_check reports CORE-NOTIMPL-005 to the context's sink and
// checks nothing, and map_mask_error answers nullopt, until DRCCIF ports
// drcCifCheck (tasks.md section 8). Nothing calls them yet, so DRC results
// are unchanged. Owner: DRCCIF.

#include "hefesto/drc/check/mask_check.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::drc {

void mask_layer_check(const CheckInput& /*input*/, const db::Rect& /*check_area*/, ErrorSink& /*sink*/,
                      mask::GenerationContext& ctx) {
  if (ctx.sink != nullptr) {
    ctx.sink->emit(cmd::not_implemented_diagnostic("drc::mask_layer_check"));
  }
}

std::optional<db::Rect> map_mask_error(const db::Rect& /*mask_rect*/, const mask::OutputStyle& /*style*/,
                                       const compat::CompatSettings& /*compat*/) {
  return std::nullopt;
}

}  // namespace hefesto::drc
