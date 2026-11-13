// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m5-mask-gds-cif/design.md D3): parse_scale
// answers nullopt and feed() reports CORE-NOTIMPL-005 until STYLES ports
// CIFParseScale and the style/variant state machine (tasks.md 3.1).
// Owner: STYLES.

#include "hefesto/mask/style/core.hpp"

#include <utility>

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::mask {

std::optional<ScaleValue> parse_scale(std::string_view /*text*/) {
  return std::nullopt;
}

StyleStateMachine::StyleStateMachine(std::string wanted) : wanted_(std::move(wanted)) {}

StyleStateMachine::LineKind StyleStateMachine::feed(const tech::RawLine& /*line*/, diag::DiagnosticSink& sink) {
  sink.emit(cmd::not_implemented_diagnostic("mask::StyleStateMachine::feed"));
  return LineKind::statement;
}

bool StyleStateMachine::applies() const noexcept {
  return applies_;
}

}  // namespace hefesto::mask
