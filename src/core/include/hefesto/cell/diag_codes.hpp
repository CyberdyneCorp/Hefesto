// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the CELL area (cell files). The table with
// severities and summaries is src/core/src/cell/codes.cpp, documented in
// docs/diagnostics/codes/cell.md (`hefesto-cli --diagnostic-codes=cell`).
// The message texts follow Magic's where the specs quote them.
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2/D3).

#pragma once

#include <string_view>

namespace hefesto::cell::codes {

// .mag reading (cell-file-format)
inline constexpr std::string_view kFormatError = "CELL-READ-001";
inline constexpr std::string_view kBadFirstLine = "CELL-READ-002";
inline constexpr std::string_view kBadTechLine = "CELL-READ-003";
inline constexpr std::string_view kHeaderLine = "CELL-READ-004";
inline constexpr std::string_view kTechMismatch = "CELL-READ-005";
inline constexpr std::string_view kRecordSkipped = "CELL-READ-006";
inline constexpr std::string_view kRecordValue = "CELL-READ-007";
inline constexpr std::string_view kGeometryLost = "CELL-READ-008";
inline constexpr std::string_view kReadNote = "CELL-READ-009";
inline constexpr std::string_view kUseProblem = "CELL-READ-010";
inline constexpr std::string_view kZeroTimestamp = "CELL-READ-011";
inline constexpr std::string_view kCompressed = "CELL-READ-013";
// Extensions (hefesto-mag-extensions)
inline constexpr std::string_view kNewerExtension = "CELL-EXT-001";
// Library resolution (cell-library-resolution)
inline constexpr std::string_view kPathFallback = "CELL-PATH-001";
inline constexpr std::string_view kNotFound = "CELL-PATH-002";
inline constexpr std::string_view kUsePath = "CELL-PATH-003";
inline constexpr std::string_view kUndefinedVariable = "CELL-PATH-004";
inline constexpr std::string_view kConflictingLocation = "CELL-PATH-005";
inline constexpr std::string_view kLibraryPath = "CELL-PATH-006";
// I/O safety (cell-io-safety)
inline constexpr std::string_view kReadOnly = "CELL-IO-001";
inline constexpr std::string_view kWriteRefused = "CELL-IO-002";
inline constexpr std::string_view kWriteFailed = "CELL-IO-003";
inline constexpr std::string_view kRenamedReference = "CELL-IO-004";
inline constexpr std::string_view kOutOfDateStamp = "CELL-IO-005";
inline constexpr std::string_view kInvalidName = "CELL-IO-006";
inline constexpr std::string_view kSaved = "CELL-IO-007";
// Crash backups and archives (cell-io-safety)
inline constexpr std::string_view kBackupRead = "CELL-BACKUP-001";
inline constexpr std::string_view kBackupNote = "CELL-BACKUP-002";
// Timestamps (cell-timestamps)
inline constexpr std::string_view kStampMismatch = "CELL-STAMP-001";
// Commands
inline constexpr std::string_view kBadUsage = "CELL-CMD-001";
inline constexpr std::string_view kNoCell = "CELL-CMD-002";

}  // namespace hefesto::cell::codes
