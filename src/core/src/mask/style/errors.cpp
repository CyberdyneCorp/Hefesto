// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask generation error reporting (see errors.hpp).
//
// Ported from Magic VLSI 8.3.694 cif/CIFmain.c (CIFError), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/mask/style/errors.hpp"

namespace hefesto::mask {

std::string MaskErrorReporter::text(std::string_view message) const {
  const std::string cell = target_.cell != nullptr ? target_.cell->name() : std::string{};
  const std::string layer = layer_ < style_.layers.size() ? style_.layers[layer_].name : std::string{};
  return "CIF error in cell " + cell + ", layer " + layer + ": " + std::string{message};
}

bool MaskErrorReporter::report(const db::Rect& area, std::string_view message) {
  const bool dropped = style_.has_option(style_option::no_errors) || target_.cell == nullptr ||
                       target_.feedback == nullptr || target_.windows == nullptr;
  if (dropped || !target_.feedback->add(*target_.windows, *target_.cell, area, style_.scale.scale_factor,
                                        view::producer_style::kMaskGeneration, text(message), target_.highlights)) {
    ++suppressed_;
    return false;
  }
  ++reported_;
  return true;
}

}  // namespace hefesto::mask
