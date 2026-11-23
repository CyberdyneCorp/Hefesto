// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the lef/via unit tests: tests/data/lef/via/via.tech with an
// optional drc section, output styles built in code, and a collecting
// via context. The LEF and DEF parsers land separately (LEFR, DEFR), so the
// tests drive the via code through its own API with hand-built records.
//
// Owner: LEFVIA (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/context.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef::test {

inline std::string via_tech_text() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "via.tech", std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// via.tech with `drc_body` as its drc section.
inline std::shared_ptr<const tech::Technology> via_tech(std::string_view drc_body = {}) {
  std::string text = via_tech_text();
  text += "\ndrc\n";
  text += drc_body;
  text += "\nend\n";
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/via.tech", fs, tech::LoadOptions{}, sink);
  if (!loaded.ok()) {
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

inline std::shared_ptr<const drc::RuleSet> rules_of(const tech::Technology& technology) {
  diag::CollectingSink sink;
  const drc::CifCatalog cif = drc::CifCatalog::from_technology(technology, sink);
  auto rules = drc::load_style(technology, drc::style_list(technology).default_style(), tech::GridScale{}, cif,
                               compat::CompatSettings{}, sink);
  REQUIRE(rules != nullptr);
  return rules;
}

inline tech::TileType type_of(const tech::Technology& technology, std::string_view name) {
  const auto found = technology.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

/// An output style with scale factor `factor` whose one layer generates
/// `contact` cuts by squares (border, size, sep).
inline mask::OutputStyle squares_style(tech::TileType contact, std::int32_t factor, std::int32_t border,
                                       std::int32_t size, std::int32_t sep) {
  mask::OutputStyle style;
  style.scale.scale_factor = factor;
  mask::OutputLayer layer;
  layer.name = "CUT";
  mask::LayerOp orop;
  orop.types = tech::TypeMask::of(contact);
  mask::LayerOp squares;
  squares.kind = mask::OpKind::squares;
  squares.values = {border, size, sep};
  layer.ops = {orop, squares};
  style.layers.push_back(layer);
  return style;
}

/// An empty layer table for begin_via (a fresh one at each call).
inline LayerTable& no_layers() {
  static LayerTable table;
  table = LayerTable{};
  return table;
}

inline compat::CompatSettings compat_mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

/// A via context over `technology` that collects its messages.
struct Context {
  std::shared_ptr<const tech::Technology> technology;
  compat::CompatSettings compat;
  diag::CollectingSink sink;
  const mask::OutputStyle* style = nullptr;

  explicit Context(std::shared_ptr<const tech::Technology> t, bool magic_compat = true,
                   const mask::OutputStyle* s = nullptr)
      : technology(std::move(t)), compat(compat_mode(magic_compat)), style(s) {}

  [[nodiscard]] ViaContext via() { return ViaContext{*technology, style, compat, sink}; }
  [[nodiscard]] tech::TileType type(std::string_view name) const { return type_of(*technology, name); }
  [[nodiscard]] std::vector<std::string> texts() {
    std::vector<std::string> out;
    for (const auto& d : sink.take()) {
      out.push_back(d.text);
    }
    return out;
  }
};

}  // namespace hefesto::lef::test
