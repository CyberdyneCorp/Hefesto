// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF read messages (capability cif-format-io "Read diagnostics"; Magic's
// CIFReadError and CIFReadWarning, cif/CIFrdutils.c:132-182). Errors and
// warnings are counted separately per read and printed with Magic's
// prefixes ("Error at line L of CIF file: ", or "CIF file read error: " when
// the line is 0). Under warning level `none` nothing is printed; under
// `limit` the first 99 of each kind are printed and the 100th prints the
// limit message; `default` and `align` print everything. Level `redirect`
// follows Q-MASK-331 (compat): magic-compat ON prints to the console as
// Magic does; OFF sends the messages to the redirect sink, or drops them
// when no redirect file was named. A message's prefix and body always go
// to the same sink, because they are one diagnostic.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2, D13).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/input/warnings.hpp"

namespace hefesto::cif {

/// The CIF-READ-* codes (src/core/src/cif/read/codes.inc).
namespace read_codes {
inline constexpr std::string_view kError = "CIF-READ-001";          // CIFReadError
inline constexpr std::string_view kWarning = "CIF-READ-002";        // CIFReadWarning
inline constexpr std::string_view kLimitReached = "CIF-READ-003";   // "Error/Warning limit set"
inline constexpr std::string_view kWireWarning = "CIF-READ-004";    // wire reversal or duplicate point
inline constexpr std::string_view kWireMitre = "CIF-READ-005";      // wire mitre limit exceeded
inline constexpr std::string_view kSymbolLimit = "CIF-READ-006";    // resource cap on symbols
}  // namespace read_codes

class ReadMessages {
 public:
  /// Messages are printed with Magic's limit rules; `redirect` (may be
  /// null) receives them instead of `console` under level `redirect` with
  /// magic-compat OFF.
  ReadMessages(mask::WarningLevel level, bool magic_compat, diag::DiagnosticSink& console,
               diag::DiagnosticSink* redirect = nullptr) noexcept;

  /// CIFReadError: counts and prints `text` (no trailing newline).
  void error(std::int64_t line, std::string_view text);
  /// CIFReadWarning: counts and prints `text`.
  void warning(std::int64_t line, std::string_view text);
  /// Magic's TxError outside the counters (the wire messages): always
  /// printed on the console.
  void console(std::string_view code, std::string text);
  /// A Hefesto diagnostic (DB-GEOM-001, a sink failure, a resource cap):
  /// printed on the console, not counted.
  void diagnostic(const diag::Diagnostic& diagnostic);

  [[nodiscard]] std::int64_t errors() const noexcept { return errors_; }
  [[nodiscard]] std::int64_t warnings() const noexcept { return warnings_; }

 private:
  enum class Kind : std::uint8_t { error, warning };
  void report(Kind kind, std::int64_t count, std::int64_t line, std::string_view text);
  [[nodiscard]] diag::DiagnosticSink* target() const noexcept;

  mask::WarningLevel level_;
  bool magic_compat_;
  diag::DiagnosticSink& console_;
  diag::DiagnosticSink* redirect_;
  std::int64_t errors_ = 0;
  std::int64_t warnings_ = 0;
};

}  // namespace hefesto::cif
