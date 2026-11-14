// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF read messages (Magic's cif/CIFrdutils.c:132-182). Owner: CIF.

#include "hefesto/cif/read/messages.hpp"

#include <utility>

namespace hefesto::cif {

namespace {

constexpr std::int64_t kLimitCount = 100;  // CIFReadError's "< 100" test

}  // namespace

ReadMessages::ReadMessages(mask::WarningLevel level, bool magic_compat, diag::DiagnosticSink& console,
                           diag::DiagnosticSink* redirect) noexcept
    : level_(level), magic_compat_(magic_compat), console_(console), redirect_(redirect) {}

diag::DiagnosticSink* ReadMessages::target() const noexcept {
  if (level_ == mask::WarningLevel::redirect && !magic_compat_) {
    return redirect_;  // Q-MASK-331 OFF: the redirect file, or nowhere
  }
  return &console_;
}

void ReadMessages::report(Kind kind, std::int64_t count, std::int64_t line, std::string_view text) {
  diag::DiagnosticSink* sink = target();
  if (level_ == mask::WarningLevel::none || sink == nullptr) {
    return;
  }
  const bool is_error = kind == Kind::error;
  const std::string_view title = is_error ? "Error" : "Warning";
  const std::string_view lower = is_error ? "error" : "warning";
  if (count < kLimitCount || level_ != mask::WarningLevel::limit) {
    std::string prefix = line > 0 ? std::string{title} + " at line " + std::to_string(line) + " of CIF file: "
                                  : "CIF file read " + std::string{lower} + ": ";
    sink->emit(diag::make(is_error ? read_codes::kError : read_codes::kWarning, prefix + std::string{text}));
  } else if (count == kLimitCount) {
    sink->emit(diag::make(read_codes::kLimitReached, std::string{title} + " limit set:  Remaining " +
                                                         std::string{lower} + "s will not be reported."));
  }
}

void ReadMessages::error(std::int64_t line, std::string_view text) {
  ++errors_;
  report(Kind::error, errors_, line, text);
}

void ReadMessages::warning(std::int64_t line, std::string_view text) {
  ++warnings_;
  report(Kind::warning, warnings_, line, text);
}

void ReadMessages::console(std::string_view code, std::string text) {
  console_.emit(diag::make(code, std::move(text)));
}

void ReadMessages::diagnostic(const diag::Diagnostic& diagnostic) {
  console_.emit(diagnostic);
}

}  // namespace hefesto::cif
