// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS cells (OASIS_FILE and OASIS_CELL, capability oasis-read
// "Read-only OASIS vendor views") in OASIS and GDS output (oasis-write
// "Read-only OASIS vendor cells", gds-write "Read-only OASIS vendor cells";
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D13, Q-OASIS-35, Q-OASIS-40):
// - decode_vendor_library decodes the cells a write needs from a vendor
//   OASIS file, names resolved, for both outputs;
// - VendorCells serves one OASIS write. While its Scope is alive on a
//   thread, gds-write (gds/write/vendor_oasis.cpp) checks each read-only
//   OASIS cell it reaches (file, cell and unit, with gds-write's lookup and
//   messages) and writes an empty placeholder structure for it instead of
//   transcoding it to GDS; the OASIS translation then re-encodes the cell
//   from its source records under the placeholder's name.
//
// Owner: OASW (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/write/write.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/oasis/command/settings.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

/// Whether `def` is a read-only OASIS cell: OASIS_FILE and OASIS_CELL hold
/// strings.
[[nodiscard]] bool is_oasis_vendor_cell(const db::CellDef& def);

/// The cells of a vendor OASIS file that a write asked for.
struct VendorLibrary {
  double unit = 0.0;  // START unit (grid steps per micron)
  NameTables names;   // every name table, for resolving references
  bool has_xnames = false;
  /// The records of each requested cell after its CELL record (elements
  /// and the properties attached to the cell or its elements), in file
  /// order; a requested cell the file does not define is absent.
  std::map<std::string, std::vector<Record>, std::less<>> cells;
};

/// Decodes `source` (from its first byte) for the cells named `wanted`:
/// a first pass reads the name tables, a second keeps the wanted cells'
/// records. An error when the file cannot be decoded.
[[nodiscard]] diag::Outcome<VendorLibrary> decode_vendor_library(gds::ByteSource& source,
                                                                const std::set<std::string, std::less<>>& wanted);

/// The name of a referenced cell or text string of `library` (`#<n>` for a
/// reference number the file does not define, as oasis-read names it).
[[nodiscard]] std::string resolved_name(const std::map<std::uint64_t, std::string>& table, const Element& e);

/// `property` with its name and string references resolved in `names`.
[[nodiscard]] Property resolved_property(const Property& property, const NameTables& names);

/// The read-only OASIS cells of one OASIS write.
class VendorCells {
 public:
  VendorCells(const gds::PreparedWrite& prepared, const OasisSettings& settings, diag::DiagnosticSink& sink);
  ~VendorCells();
  VendorCells(const VendorCells&) = delete;
  VendorCells& operator=(const VendorCells&) = delete;

  /// Makes `cells` the active object of this thread while alive.
  class Scope {
   public:
    explicit Scope(VendorCells& cells);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

   private:
    VendorCells* previous_;
  };
  /// The object of the innermost live Scope on this thread, or nullptr.
  [[nodiscard]] static VendorCells* active() noexcept;

  /// The placeholder stage, from gds-write: `source` is `def`'s opened
  /// OASIS_FILE (found at `path`).
  enum class Check : std::uint8_t {
    ready,     // write the placeholder; the cell is re-encoded
    fallback,  // generate the cell from its layout (an error was reported)
    missing,   // the cell is not in the file: as an unopenable file
  };
  [[nodiscard]] Check prepare(const db::CellDef& def, gds::ByteSource& source, const std::string& path);

  /// Writes the body of the read-only OASIS cell whose structure name
  /// payload is `payload` (a placeholder) to `out`, under output name
  /// `output`; `name_of` gives the output name of a structure name
  /// payload. nullopt when `payload` is no prepared read-only OASIS cell.
  [[nodiscard]] std::optional<diag::Status> reencode(std::string_view payload, const std::string& output, Encoder& out,
                                                     const std::function<std::string(std::string_view)>& name_of);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::oasis
