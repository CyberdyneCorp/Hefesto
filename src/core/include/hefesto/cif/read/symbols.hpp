// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-read CIF symbol table (capability cif-format-io "Symbol-to-cell
// naming on read"; Q-MASK-318, fix in both modes; design D13). It belongs
// to one read, never to the library:
//   - symbol n, in `DS n` and in a forward `C n` alike, always maps to a cell
//     created by this read; a cell already in memory is never looked up by
//     the name "n", reused or cleared;
//   - a new cell is named "n", or "n_k" with the smallest k >= 1 not in use;
//   - a second `DS n` (after `DD n`, after `9` renamed the symbol, or a plain
//     redefinition) renames a cell of this read that is defined and still
//     named "n" to "n_k", as Magic does (cifUniqueCell), and creates a fresh
//     cell for the new definition;
//   - `DD n` is compat: magic-compat ON forgets symbol n only, as Magic does;
//     OFF forgets every symbol >= n, as CIF 2.0 says;
//   - the cells left undefined are listed in the iteration order of Magic's
//     CifCellTable, a word-keyed Magic hash table of the symbol numbers
//     (requested size 32; cif/CIFrdcl.c:134, 1752-1772), in both modes.
// Magic's version (cif/CIFrdcl.c:199-290) can clear a user cell named "n" or
// a cell defined earlier in the read; that loses data, hence the fix.
//
// The table deals in names and ids only, so the parser can drive a
// recording ReadSink as well as the read engine.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2, D13).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::cif {

/// A cell created by this read.
struct ReadCell {
  std::string name;
  std::int64_t symbol = 0;  // the symbol number it was created for
  bool defined = false;     // its DS was read (Magic's CDAVAILABLE)
};

class SymbolTable {
 public:
  using CellId = std::size_t;
  /// True when a cell outside this read already has the name.
  using NameInUse = std::function<bool(std::string_view)>;

  /// What `DS n` did.
  struct Definition {
    CellId cell = 0;
    /// Set when a defined cell of this read named "n" was renamed (its new
    /// name); the caller reports "cell definition n reused."
    std::optional<std::string> renamed_to;
    std::string renamed_from;
  };

  /// A table with no cell outside the read.
  SymbolTable() = default;
  /// `outside` answers for cells that exist outside this read; `max_cells`
  /// caps the cells one read may create (a resource cap, design D21).
  explicit SymbolTable(NameInUse outside, std::size_t max_cells = kDefaultMaxCells);

  static constexpr std::size_t kDefaultMaxCells = std::size_t{1} << 20;

  /// `C n`: the cell symbol n maps to, created (undefined) on first
  /// reference. nullopt when the cap is reached.
  [[nodiscard]] std::optional<CellId> call(std::int64_t number);
  /// `DS n`: the cell that receives the definition. nullopt when the cap is
  /// reached.
  [[nodiscard]] std::optional<Definition> define(std::int64_t number);
  /// `DD n`.
  void forget(std::int64_t number, bool magic_compat);
  /// `9 NAME` inside a definition: renames `cell`. The caller checks
  /// name_in_use() first, as DBCellRenameDef does.
  void rename(CellId cell, const std::string& name);
  /// A cell outside this read (the edit cell, top-level `9`) took `name`:
  /// from now on the outside test answers for it.
  void taken_outside(const std::string& name) { given_out_.erase(name); }

  /// The cell symbol `number` maps to, if any.
  [[nodiscard]] std::optional<CellId> find(std::int64_t number) const;
  [[nodiscard]] const ReadCell& cell(CellId id) const { return cells_.at(id); }
  /// Every cell this read created, in creation order.
  [[nodiscard]] const std::vector<ReadCell>& cells() const noexcept { return cells_; }
  /// The cells whose DS was never read, in the order Magic reports them
  /// ("used but not defined"): the iteration order of the symbol table,
  /// then creation order among the cells of one symbol number.
  [[nodiscard]] std::vector<CellId> undefined_cells() const;
  /// True when a cell of this read, or a cell outside it, has `name`. The
  /// outside test is not asked about names this read has given out, so a
  /// rename the caller has not yet applied to the library is not mistaken
  /// for a cell outside the read.
  [[nodiscard]] bool name_in_use(std::string_view name) const;
  /// The number of symbols currently mapped.
  [[nodiscard]] std::size_t size() const noexcept { return symbols_.size(); }

 private:
  [[nodiscard]] std::string free_name(std::int64_t number);
  void name_freed(const std::string& name);
  [[nodiscard]] std::optional<CellId> create(std::int64_t number, bool defined);
  void set_name(CellId id, const std::string& name);

  /// Magic's HT_WORDKEYS hash input: the key's integer value.
  struct WordKey {
    using Key = std::int64_t;
    [[nodiscard]] static std::uint64_t hash(Key key) noexcept { return static_cast<std::uint64_t>(key); }
  };

  NameInUse outside_;
  std::size_t max_cells_ = kDefaultMaxCells;
  util::MagicHashTable<bool, WordKey> table_{32};  // CifCellTable: every symbol number given a cell
  std::map<std::int64_t, CellId> symbols_;
  std::vector<ReadCell> cells_;
  std::unordered_map<std::string, CellId> names_;  // the current names of this read's cells
  std::unordered_set<std::string> given_out_;       // every name this read has given a cell
  std::unordered_map<std::string, std::int64_t> first_suffix_;  // free_name's search start per base
};

}  // namespace hefesto::cif
