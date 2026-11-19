// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the LEF reader's MACRO and SITE sections need of the session
// (capability lef-read "MACRO and SITE cell creation", "Macro completion",
// "Annotation of existing cells"; design D6): cell lookup and creation, a
// notice before an existing cell is first changed (the undo step of the
// read keeps its state, Q-LEFDEF-22), and the end of each macro
// (redisplay, DRC). lef::read implements it over the editor session; the
// tests implement it over a bare cell library.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "hefesto/db/geometry.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::lef {

class MacroHost {
 public:
  MacroHost() = default;
  MacroHost(const MacroHost&) = delete;
  MacroHost& operator=(const MacroHost&) = delete;
  virtual ~MacroHost() = default;

  /// DBCellLookDef: the cell named `name`; nullptr when there is none.
  [[nodiscard]] virtual db::CellDef* find(std::string_view name) = 0;

  /// lefFindCell, DBCellClearDef and DBCellSetAvail for a name with no
  /// cell: a new empty, available cell; nullptr when it cannot be made.
  [[nodiscard]] virtual db::CellDef* create(std::string_view name) = 0;

  /// Called once per read before the reader first changes the existing
  /// cell `cell` (annotation mode).
  virtual void annotating(db::CellDef& cell) = 0;

  /// The end of a macro (DBWAreaChanged over the cell's bounding box) and,
  /// with `drc_area`, DRCCheckThis of that area (a created macro without
  /// SIZE).
  virtual void finished(db::CellDef& cell, std::optional<db::Rect> drc_area) = 0;
};

/// The `lef read` settings a MACRO or SITE section uses.
struct MacroOptions {
  bool annotate = false;                  // -annotate
  std::optional<std::int64_t> datestamp;  // `lef datestamp`; nullopt: default
};

}  // namespace hefesto::lef
