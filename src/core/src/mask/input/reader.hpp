// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// INPUT-private: the cifinput reader (CIFReadTechLine, CIFReadTechFinal and
// their helpers), shared by the files of src/core/src/mask/input/.
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask::input_detail {

/// Diagnostic codes of src/core/src/mask/input/codes.inc.
inline constexpr std::string_view kTechError = "MASK-INPUT-001";
inline constexpr std::string_view kPrefixShadow = "MASK-INPUT-002";
inline constexpr std::string_view kInputStyleScale = "MASK-INPUT-003";
inline constexpr std::string_view kAmbiguousStyle = "MASK-INPUT-004";
inline constexpr std::string_view kUnknownStyle = "MASK-INPUT-005";
inline constexpr std::string_view kDecimalUnit = "MASK-INPUT-006";
inline constexpr std::string_view kBoundaryInLayer = "MASK-INPUT-007";
inline constexpr std::string_view kTemplayerLabels = "MASK-INPUT-008";
inline constexpr std::string_view kScaleTruncated = "MASK-INPUT-009";
inline constexpr std::string_view kGridRedefined = "MASK-INPUT-010";
inline constexpr std::string_view kSnappedToGrid = "MASK-INPUT-011";
inline constexpr std::string_view kUnitsRescaled = "MASK-INPUT-012";
inline constexpr std::string_view kBoundaryRedefined = "MASK-INPUT-013";
inline constexpr std::string_view kEuclideanNote = "MASK-INPUT-014";
inline constexpr std::string_view kTaggedScaleNote = "MASK-INPUT-015";

/// The tech-load messages of one pass, located on the current line with the
/// `cifinput` section (the magic rendering prints TechPrintLine's context).
class Messages {
 public:
  void at(const tech::RawLine& line) { line_ = &line; }
  /// Magic's TechError (MASK-INPUT-001).
  void error(std::string text) { add(kTechError, std::move(text), false); }
  /// A Hefesto-only warning (dropped by the magic rendering).
  void native(std::string_view code, std::string text) { add(code, std::move(text), true); }
  /// A message printed by Magic without line context (TxPrintf).
  void plain(std::string_view code, std::string text);

  [[nodiscard]] std::vector<diag::Diagnostic> take() { return std::move(messages_); }

 private:
  void add(std::string_view code, std::string text, bool native);

  const tech::RawLine* line_ = nullptr;
  std::vector<diag::Diagnostic> messages_;
};

/// One load pass (CIFReadTechStyleInit, CIFReadTechLine for every line,
/// CIFReadTechFinal).
struct LoadResult {
  InputStyle style;
  std::vector<std::string> names;  // the style list seen by the pass
  bool section_failed = false;     // a statement before the first style line
  bool loaded = false;             // a style was started (cifCurReadStyle != NULL)
};

/// Loads the style `wanted` (empty: the first one) from `section`.
/// `list_messages`: also report the style-list warnings (the first pass).
[[nodiscard]] LoadResult load_style(const tech::Technology& technology, const tech::RawSection& section,
                                    const std::string& wanted, const compat::CompatSettings& compat,
                                    bool list_messages);

/// cifParseCalmaNums: the numbers of a `calma` list, -1 for `*`; nullopt
/// after Magic's error (reported to `messages`). `limit`: 255 entries are
/// already too many (Magic, Q-MASK-326 ON); nullopt for no limit (OFF).
[[nodiscard]] std::optional<std::vector<std::int32_t>> parse_calma_numbers(std::string_view text,
                                                                           std::optional<std::size_t> limit,
                                                                           Messages& messages);

/// The truncation warning of a normalisation step (magic-compat OFF only).
[[nodiscard]] std::optional<diag::Diagnostic> truncation_warning(const InputStyle& style, const InputScaleStep& step,
                                                                 const compat::CompatSettings& compat);

}  // namespace hefesto::mask::input_detail
