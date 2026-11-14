// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-read CIF symbol table (Q-MASK-318 fix; Magic's cif/CIFrdcl.c:
// 199-290 cifForgetCell, cifUniqueCell, cifFindCell). Owner: CIF.

#include "hefesto/cif/read/symbols.hpp"

#include <algorithm>
#include <utility>

namespace hefesto::cif {

SymbolTable::SymbolTable(NameInUse outside, std::size_t max_cells)
    : outside_(std::move(outside)), max_cells_(max_cells) {}

bool SymbolTable::name_in_use(std::string_view name) const {
  const std::string key{name};
  if (names_.contains(key)) {
    return true;
  }
  return !given_out_.contains(key) && outside_ && outside_(name);
}

// cifUniqueCell: "<n>" if free, else the first free "<n>_<k>", k >= 1.
// first_suffix_ keeps, per base, a k below which every suffix is known to
// be taken, so redefining one symbol many times stays linear; set_name
// lowers it when a suffixed name is freed, so the names are unchanged.
std::string SymbolTable::free_name(std::int64_t number) {
  std::string base = std::to_string(number);
  if (!name_in_use(base)) {
    return base;
  }
  auto [hint, inserted] = first_suffix_.try_emplace(base, 1);
  for (std::int64_t k = hint->second;; ++k) {
    std::string candidate = base + "_" + std::to_string(k);
    if (!name_in_use(candidate)) {
      hint->second = k;
      return candidate;
    }
  }
}

void SymbolTable::name_freed(const std::string& name) {
  const auto underscore = name.rfind('_');
  if (underscore == std::string::npos || underscore + 1 == name.size()) {
    return;
  }
  const auto hint = first_suffix_.find(name.substr(0, underscore));
  if (hint == first_suffix_.end()) {
    return;
  }
  std::int64_t k = 0;
  for (std::size_t i = underscore + 1; i < name.size(); ++i) {
    if (name[i] < '0' || name[i] > '9' || k > hint->second) {
      return;  // not a suffix, or above the hint already
    }
    k = k * 10 + (name[i] - '0');
  }
  hint->second = std::min(hint->second, k);
}

void SymbolTable::set_name(CellId id, const std::string& name) {
  name_freed(cells_[id].name);
  names_.erase(cells_[id].name);
  cells_[id].name = name;
  names_[name] = id;
  given_out_.insert(name);
}

std::optional<SymbolTable::CellId> SymbolTable::create(std::int64_t number, bool defined) {
  if (cells_.size() >= max_cells_) {
    return std::nullopt;
  }
  const CellId id = cells_.size();
  std::string name = free_name(number);
  cells_.push_back(ReadCell{name, number, defined});
  names_[name] = id;
  given_out_.insert(name);
  symbols_[number] = id;
  return id;
}

std::optional<SymbolTable::CellId> SymbolTable::find(std::int64_t number) const {
  const auto it = symbols_.find(number);
  return it == symbols_.end() ? std::nullopt : std::optional<CellId>{it->second};
}

std::optional<SymbolTable::CellId> SymbolTable::call(std::int64_t number) {
  if (const auto existing = find(number)) {
    return existing;
  }
  return create(number, false);
}

std::optional<SymbolTable::Definition> SymbolTable::define(std::int64_t number) {
  const auto existing = find(number);
  if (existing && !cells_[*existing].defined) {
    cells_[*existing].defined = true;  // a forward C n: the same cell
    return Definition{*existing, std::nullopt, {}};
  }
  if (cells_.size() >= max_cells_) {
    return std::nullopt;
  }
  Definition out;
  const std::string base = std::to_string(number);
  const auto holder = names_.find(base);
  if (holder != names_.end() && cells_[holder->second].defined) {
    const CellId old = holder->second;
    std::string renamed = free_name(number);
    set_name(old, renamed);
    out.renamed_from = base;
    out.renamed_to = std::move(renamed);
  }
  symbols_.erase(number);
  out.cell = *create(number, true);
  return out;
}

void SymbolTable::forget(std::int64_t number, bool magic_compat) {
  if (magic_compat) {
    symbols_.erase(number);
    return;
  }
  symbols_.erase(symbols_.lower_bound(number), symbols_.end());
}

void SymbolTable::rename(CellId cell, const std::string& name) {
  set_name(cell, name);
}

}  // namespace hefesto::cif
