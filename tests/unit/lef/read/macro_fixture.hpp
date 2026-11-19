// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the lef/read MACRO and SITE tests: a lef::MacroHost over a
// bare cell library, a LEF stream runner that reads into it, and paint and
// property queries.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/lef/read/macro.hpp"
#include "read_fixture.hpp"

namespace lefread_test {

/// lef::MacroHost over a cell library; records what the reader asked.
class LibraryHost final : public lef::MacroHost {
 public:
  db::CellDef* find(std::string_view name) override { return cells.find(name); }

  db::CellDef* create(std::string_view name) override {
    auto made = cells.create(name, technology()->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    created.emplace_back(name);
    return made.value();
  }

  void annotating(db::CellDef& cell) override { annotated.push_back(cell.name()); }

  void finished(db::CellDef& cell, std::optional<db::Rect> drc_area) override {
    done.push_back(cell.name());
    drc.push_back(drc_area);
  }

  db::CellLibrary cells;
  std::vector<std::string> created;
  std::vector<std::string> annotated;
  std::vector<std::string> done;
  std::vector<std::optional<db::Rect>> drc;
};

/// One LEF read into a cell library.
struct MacroRun {
  explicit MacroRun(bool magic_compat = true, float scale = 1.0F) : lef{magic_compat, scale} {
    define_scenario_layers(lef.layers);
  }

  lef::StreamResult read(std::string_view text) {
    gds::MemorySource source{bytes(text)};
    lef::Lexer lexer{source};
    const lef::MessageTarget messages{lef.counter, lef.sink, lexer};
    lef::StreamTarget target{lef.layers, lef.vias, lef.rules, nullptr, &host};
    lef.options.macros = macros;
    return lef::read_stream(lexer, messages, lef.options, target);
  }

  std::vector<std::string> texts() { return lef.texts(); }

  db::CellDef& cell(std::string_view name) {
    db::CellDef* found = host.cells.find(name);
    REQUIRE(found != nullptr);
    return *found;
  }

  /// A cell made in the library before the read (annotation tests).
  db::CellDef& existing(std::string_view name) {
    auto made = host.cells.create(name, technology()->plane_count());
    REQUIRE(made.ok());
    made.value()->flags().available = true;
    return *made.value();
  }

  LefRun lef;
  LibraryHost host;
  lef::MacroOptions macros;
};

/// The tiles of `type` on its home plane.
inline std::vector<db::Rect> tiles(const db::CellDef& cell, std::string_view type_name) {
  const tech::TileType type = type_of(type_name);
  const auto plane = technology()->home_plane(type);
  REQUIRE(plane.has_value());
  std::vector<db::Rect> out;
  (void)cell.plane(*plane).enumerate(db::whole_plane(), tech::TypeMask::of(type), [&](const db::TileView& t) {
    out.push_back(t.area);
    return true;
  });
  return out;
}

/// A string property; nullopt when absent.
inline std::optional<std::string> string_property(const db::CellDef& cell, std::string_view name) {
  const db::PropertyValue* value = cell.properties().find(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  const auto* text = std::get_if<std::string>(value);
  REQUIRE(text != nullptr);
  return *text;
}

/// FIXED_BBOX; nullopt when absent.
inline std::optional<db::Rect> fixed_bbox(const db::CellDef& cell) {
  const db::PropertyValue* value = cell.properties().find("FIXED_BBOX");
  if (value == nullptr) {
    return std::nullopt;
  }
  const auto* list = std::get_if<db::DimensionList>(value);
  REQUIRE(list != nullptr);
  REQUIRE(list->values.size() == 4);
  return db::Rect{list->values[0], list->values[1], list->values[2], list->values[3]};
}

/// A label made by hand (annotation tests).
inline db::Label label(std::string text, std::string_view type, db::Rect rect) {
  db::Label l;
  l.text = std::move(text);
  l.type = type_of(type);
  l.rect = rect;
  return l;
}

}  // namespace lefread_test
