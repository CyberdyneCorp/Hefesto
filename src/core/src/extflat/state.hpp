// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The state shared by one read and flatten (Magic's extflat globals:
// EFLayerNames, efNumResistClasses, locScale, efReadFileName and
// efReadLineNum), and its diagnostics.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/read.hpp"
#include "model.hpp"

namespace hefesto::extflat::detail {

namespace codes {
inline constexpr std::string_view kCannotRead = "EXTFLAT-READ-001";
inline constexpr std::string_view kReadError = "EXTFLAT-READ-002";
inline constexpr std::string_view kTechMismatch = "EXTFLAT-READ-003";
inline constexpr std::string_view kUnknownStyle = "EXTFLAT-READ-004";
inline constexpr std::string_view kDuplicateUse = "EXTFLAT-READ-005";
inline constexpr std::string_view kShortedPorts = "EXTFLAT-READ-006";
inline constexpr std::string_view kNoSuchNode = "EXTFLAT-READ-007";
inline constexpr std::string_view kGlobalSplit = "EXTFLAT-READ-008";
inline constexpr std::string_view kLimit = "EXTFLAT-READ-009";
inline constexpr std::string_view kUseCycle = "EXTFLAT-READ-010";
inline constexpr std::string_view kTooManyTokens = "EXTFLAT-READ-011";
inline constexpr std::string_view kDeviceOverlap = "EXTFLAT-READ-012";
}  // namespace codes

/// The most tokens efReadLine's argv holds.
inline constexpr std::size_t kMaxTokens = 128;

struct ReadState {
  ReadState(diag::DiagnosticSink& sink_, const ReadOptions& options_)
      : sink(sink_), options(options_), globals(options_.global_nets.begin(), options_.global_nets.end()) {}

  diag::DiagnosticSink& sink;
  const ReadOptions& options;
  std::unordered_set<std::string> globals;  // EFHNIsGlob's `globals` array
  DefTable defs;
  LayerNames layers;

  // EFread.c globals.
  float loc_scale = 1.0F;                    // locScale
  std::optional<float> units_per_lambda;     // ExtCurStyle->exts_unitsPerLambda
  std::optional<std::size_t> resist_classes; // efNumResistClasses (set by the first record)
  std::vector<std::int32_t> resists;         // efResists
  std::unordered_set<std::string> device_params;  // efDevParamTable keys
  std::string file;                          // efReadFileName
  std::uint32_t line = 0;                    // efReadLineNum

  std::optional<std::string> selected_style;
  std::vector<FileStamp> stamps;
  std::optional<diag::Diagnostic> fatal;    // a cap or a cycle: the read stops
  std::optional<diag::Diagnostic> failure;  // the first reason EFReadFile returns FALSE

  /// Keeps the first reason the read fails (reading goes on).
  void note_failure(diag::Diagnostic d) {
    if (!failure) {
      failure = std::move(d);
    }
  }

  [[nodiscard]] bool is_global(std::string_view leaf) const { return globals.count(std::string{leaf}) != 0; }

  /// efReadError: "<file>, line <n>: <text>".
  void read_error(std::string_view text);
  /// A message without the line prefix (TxError, TxPrintf), at its code's
  /// default severity.
  void message(std::string_view code, std::string text);
  /// Records the first fatal condition (later ones are not reported).
  void fail(std::string_view code, std::string text);
};

}  // namespace hefesto::extflat::detail
