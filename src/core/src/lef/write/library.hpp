// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF library header (capability lef-write "Library header", "SITE
// blocks", "Technology layers"; Q-LEFDEF-30, 31, 32, 33; header.cpp) and
// the output file naming (capability lef-write "Output file naming";
// Q-LEFDEF-43; write.cpp).
//
// Owner: LEFW (design D2).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/write/number.hpp"

namespace hefesto::lef::write_detail {

/// What lefWriteHeader reads.
struct HeaderInput {
  const db::CellDef* def = nullptr;           // the cell or root written (FIXED_BBOX of Q-LEFDEF-33 ON)
  std::vector<const db::CellDef*> cells;      // whose LEFproperties and LEFsite are collected
  const db::CellLibrary* library = nullptr;   // the site cells are looked up here
  const LayerTable* layers = nullptr;
  const ViaTable* vias = nullptr;
  bool tech = false;                          // -tech
  bool magic_compat = true;
  float oscale = 1.0F;
  LefUnits units;                             // the units this header computed
};

/// lefWriteHeader: VERSION and the fixed lines, UNITS (-tech),
/// PROPERTYDEFINITIONS, the SITE blocks and the LAYER blocks (-tech).
[[nodiscard]] std::string library_header(const HeaderInput& in, std::vector<diag::Diagnostic>& messages);

/// lefFileOpen's names, in the order they are tried: the base name as
/// given when its extension is `.lef`; as given, then with its extension
/// replaced by `.lef`, for another extension; with `.lef` appended for
/// none. Then the cell name plus `.lef`: with magic-compat ON whenever the
/// base name is not the cell name (Magic retries for any name), with it OFF
/// only when the base came from the cell's file path.
[[nodiscard]] std::vector<std::string> output_names(std::string_view base, std::string_view cell_name,
                                                    bool from_cell_file, bool magic_compat);

}  // namespace hefesto::lef::write_detail
