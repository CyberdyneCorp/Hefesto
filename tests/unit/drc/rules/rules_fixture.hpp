// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the drc/rules unit tests: tests/data/drc/rules/rules.tech with
// a test-specific `drc` section, compiled through the public RULES API
// (load_style for a style reload, reload_rules for the first load of
// `tech load`), plus helpers to name types, planes and masks.
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/drc/session.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc::test {

inline std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// Loads technology text (labelled `/t/<name>`); fails the test on errors.
inline std::shared_ptr<const tech::Technology> load_tech_text(const std::string& text,
                                                              const std::string& name = "rules.tech") {
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/" + name, fs, tech::LoadOptions{}, sink);
  if (!loaded.ok()) {
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// rules.tech with `drc_body` (lines without `drc`/`end`) as its drc section.
inline std::shared_ptr<const tech::Technology> rules_tech(std::string_view drc_body) {
  std::string text = read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "rules.tech");
  text += "\ndrc\n";
  text += drc_body;
  text += "\nend\n\nextract\nend\n";
  return load_tech_text(text);
}

inline compat::CompatSettings compat_mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

/// One compiled style and the messages of its load.
struct Compiled {
  std::shared_ptr<const tech::Technology> tech;
  CifCatalog cif;
  std::shared_ptr<const RuleSet> rules;
  std::vector<diag::Diagnostic> messages;

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }
  [[nodiscard]] tech::TypeMask mask(std::string_view list) const {
    const auto parsed = tech->parse_type_list(list);
    REQUIRE(parsed.ok());
    return parsed.value();
  }
  [[nodiscard]] tech::PlaneId plane(std::string_view name) const {
    const auto found = tech->find_plane(name);
    REQUIRE(found.ok());
    return *found.id;
  }
  /// PT(plane): every type on the plane, space included.
  [[nodiscard]] tech::TypeMask plane_types(std::string_view name) const { return tech->plane_types(plane(name)); }
  [[nodiscard]] std::vector<Rule> at(std::string_view a, std::string_view b) const {
    const auto span = rules->rules(type(a), type(b));
    return {span.begin(), span.end()};
  }
  [[nodiscard]] std::string why(const Rule& rule) const { return std::string{rules->why().text(rule.why)}; }
  [[nodiscard]] bool said(std::string_view text) const {
    for (const auto& d : messages) {
      if (d.text.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] std::size_t count_code(std::string_view code) const {
    std::size_t n = 0;
    for (const auto& d : messages) {
      if (d.code == code) {
        ++n;
      }
    }
    return n;
  }
};

/// Compiles `style` (the default one when empty) of a technology.
inline Compiled compile_tech(std::shared_ptr<const tech::Technology> technology, bool magic_compat = true,
                             std::string style = {}, tech::GridScale grid = {}) {
  Compiled out;
  out.tech = std::move(technology);
  diag::CollectingSink sink;
  out.cif = CifCatalog::from_technology(*out.tech, sink);
  if (style.empty()) {
    style = style_list(*out.tech).default_style();
  }
  out.rules = load_style(*out.tech, style, grid, out.cif, compat_mode(magic_compat), sink);
  out.messages = sink.take();
  REQUIRE(out.rules != nullptr);
  return out;
}

inline Compiled compile(std::string_view drc_body, bool magic_compat = true, std::string style = {},
                        tech::GridScale grid = {}) {
  return compile_tech(rules_tech(drc_body), magic_compat, std::move(style), grid);
}

/// The first load of `tech load` (reload_rules) into a fresh session.
struct FirstLoad {
  compat::CompatSettings settings;
  editor::EditorState editor;
  std::vector<diag::Diagnostic> messages;

  explicit FirstLoad(std::shared_ptr<const tech::Technology> technology, bool magic_compat = true,
                     tech::GridScale grid = {}) {
    settings.magic_compat = magic_compat;
    editor.technology = std::move(technology);
    editor.grid = grid;
    editor.drc.compat = &settings;
    diag::CollectingSink sink;
    reload_rules(editor, sink);
    messages = sink.take();
  }
};

/// A rule with the fields the spec scenarios state.
inline bool is_rule(const Rule& r, RuleFlags flags, std::int32_t dist, std::int32_t cdist, const tech::TypeMask& ok,
                    const tech::TypeMask& corner) {
  return r.flags == flags && r.dist == dist && r.cdist == cdist && r.ok == ok && r.corner == corner;
}

}  // namespace hefesto::drc::test
