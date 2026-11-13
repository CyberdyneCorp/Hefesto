// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (openspec/changes/m5-mask-gds-cif/design.md D3):
// evaluate_layer reports CORE-NOTIMPL-005 to the context's sink and returns
// an empty plane until OPS ports CIFGenLayer (tasks.md 4.2). Owner: OPS.

#include "hefesto/mask/ops/evaluate.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

MaskPlane& evaluate_layer(const OutputStyle& /*style*/, std::size_t /*layer*/, const LayerInputs& /*inputs*/,
                          const db::Rect& /*area*/, OperatorContext& ctx) {
  if (ctx.sink != nullptr) {
    ctx.sink->emit(cmd::not_implemented_diagnostic("mask::evaluate_layer"));
  }
  return ctx.pool->acquire();
}

}  // namespace hefesto::mask
