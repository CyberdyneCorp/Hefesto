// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): the reader
// and selection report CORE-NOTIMPL-005 until INPUT ports CIFrdtech.c
// (tasks.md section 7). Owner: INPUT.

#include "hefesto/mask/input/style.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

void InputStyle::resolve(const compat::CompatSettings& /*compat*/) {
  resolved_ = InputResolved{};
}

const InputStyle* InputStyleSet::current_style() const noexcept {
  return current && *current < styles.size() ? styles[*current].get() : nullptr;
}

InputStyleSet read_input_styles(const tech::RawSection& /*section*/, const compat::CompatSettings& /*compat*/,
                                diag::DiagnosticSink& sink) {
  sink.emit(cmd::not_implemented_diagnostic("mask::read_input_styles"));
  return {};
}

diag::Status select_input_style(InputStyleSet& /*set*/, std::string_view /*name*/,
                                const compat::CompatSettings& /*compat*/) {
  return cmd::not_implemented_status("mask::select_input_style");
}

}  // namespace hefesto::mask
