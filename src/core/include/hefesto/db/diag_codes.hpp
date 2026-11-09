// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the MODEL area (layout database). The table
// with severities and summaries is src/core/src/db/codes.cpp, documented in
// docs/diagnostics/codes/db.md (`hefesto-cli --diagnostic-codes=db`).
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2/D3).

#pragma once

#include <string_view>

namespace hefesto::db::codes {

// Geometry (layout-geometry-model)
inline constexpr std::string_view kGeometryOutsidePlane = "DB-GEOM-001";
// Cell namespace (layout-hierarchy)
inline constexpr std::string_view kCellExists = "DB-CELL-001";
inline constexpr std::string_view kCellInUse = "DB-CELL-002";
inline constexpr std::string_view kCellInternal = "DB-CELL-003";
inline constexpr std::string_view kCellVendor = "DB-CELL-004";
inline constexpr std::string_view kCellNotFound = "DB-CELL-005";
// Instances (layout-hierarchy)
inline constexpr std::string_view kUseRecursive = "DB-USE-001";
inline constexpr std::string_view kUseIdTaken = "DB-USE-002";
inline constexpr std::string_view kUseNotFound = "DB-USE-003";
inline constexpr std::string_view kUseIdInvalid = "DB-USE-004";
// Commands of the MODEL area
inline constexpr std::string_view kNoEditCell = "DB-CMD-001";
inline constexpr std::string_view kNoBox = "DB-CMD-002";
inline constexpr std::string_view kBadLayer = "DB-CMD-003";
inline constexpr std::string_view kBadArgument = "DB-CMD-004";
inline constexpr std::string_view kNoLabel = "DB-CMD-005";
// Labels and properties
inline constexpr std::string_view kLabelEmpty = "DB-LABEL-001";
inline constexpr std::string_view kPropertyValue = "DB-PROP-001";

// Command output that is not the result (Magic's TxPrintf / TxError text)
inline constexpr std::string_view kReport = "DB-REPORT-001";
inline constexpr std::string_view kWarning = "DB-REPORT-002";

}  // namespace hefesto::db::codes
