// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF layer table, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D4, D5).
//
// Owner: LEFTECH (design D2).

#include "hefesto/lef/tech/table.hpp"

namespace hefesto::lef {

LayerTable LayerTable::from_section(const tech::RawSection& /*section*/, const tech::Technology& /*tech*/,
                                    diag::DiagnosticSink& /*sink*/) {
  return LayerTable{};
}

void LayerTable::fill_defaults(const drc::RuleSet& /*rules*/, const tech::Technology& /*tech*/,
                               const compat::CompatSettings& /*compat*/) {}

void LayerTable::scale(std::int32_t /*n*/, std::int32_t /*d*/) {}

const LayerRecord* LayerTable::find(std::string_view lef_name) const {
  const auto* entry = names_.find(std::string{lef_name});
  return entry != nullptr && entry->value ? &records_[*entry->value] : nullptr;
}

const LayerRecord* LayerTable::magic_to_lef(tech::TileType /*type*/) const {
  return nullptr;
}

void LayerTable::enumerate(const Visitor& visit) const {
  names_.for_each([&](const auto& entry) {
    if (entry.value) {
      visit(entry.key, records_[*entry.value]);
    }
  });
}

}  // namespace hefesto::lef
