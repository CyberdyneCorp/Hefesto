// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GenerationContext (openspec/changes/m5-mask-gds-cif/design.md D4).
// Owner: GEN.

#include "hefesto/mask/gen/context.hpp"

namespace hefesto::mask {

OperatorContext GenerationContext::operator_context(const db::CellDef* cell, const compat::CompatSettings& compat) {
  return OperatorContext{compat, cell, &pool, progress, &statistics, sink};
}

}  // namespace hefesto::mask
