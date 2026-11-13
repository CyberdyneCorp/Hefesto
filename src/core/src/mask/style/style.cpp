// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): the reader
// and selection report CORE-NOTIMPL-005; the queries answer the values of a
// style without operators until STYLES ports CIFtech.c (tasks.md 3.1-3.4).
// Owner: STYLES.

#include "hefesto/mask/style/style.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

std::int64_t OutputStyle::halo() const noexcept {
  return 0;
}

std::optional<std::int64_t> OutputStyle::contact_size(tech::TileType /*type*/) const noexcept {
  return std::nullopt;
}

std::shared_ptr<const OutputStyle> OutputStyle::rescale(std::int32_t /*n*/, std::int32_t /*d*/) const {
  return std::make_shared<const OutputStyle>(*this);
}

std::optional<std::size_t> OutputStyle::find_layer(std::string_view layer) const noexcept {
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].name == layer) {
      return i;
    }
  }
  return std::nullopt;
}

const OutputStyle* OutputStyleSet::current_style() const noexcept {
  return current && *current < styles.size() ? styles[*current].get() : nullptr;
}

std::shared_ptr<const OutputStyle> OutputStyleSet::current_shared() const noexcept {
  return current && *current < styles.size() ? styles[*current] : nullptr;
}

OutputStyleSet read_output_styles(const tech::RawSection& /*section*/, diag::DiagnosticSink& sink) {
  sink.emit(cmd::not_implemented_diagnostic("mask::read_output_styles"));
  return {};
}

diag::Status select_output_style(OutputStyleSet& /*set*/, std::string_view /*name*/,
                                 const compat::CompatSettings& /*compat*/) {
  return cmd::not_implemented_status("mask::select_output_style");
}

}  // namespace hefesto::mask
