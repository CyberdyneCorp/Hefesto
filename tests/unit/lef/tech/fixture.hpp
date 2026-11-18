// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixtures of the lef/tech unit tests: tests/data/lef/tech/base.tech with a
// drc and a lef section appended, read either directly
// (lef::read_layer_section) or through `tech load` in a command session.
//
// Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/tech/lifecycle.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/ui/line.hpp"

namespace hefesto::lef::test {

/// m1: width 3, spacing 4; pc: width 2; m5 has no rule (lambda units).
inline constexpr std::string_view kDrc = R"(
drc
    width m1 3 "m1 width < 3"
    spacing m1 m1 4 touching_ok "m1 spacing < 4"
    width pc 2 "pc width < 2"
end
)";

inline std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// base.tech, then `drc`, a `lef` section holding `lef_lines`, and `extract`.
inline std::string tech_text(std::string_view lef_lines, std::string_view drc = kDrc) {
  std::string text = read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "base.tech");
  text += drc;
  text += "\nlef\n";
  text += lef_lines;
  text += "\nend\n\nextract\nend\n";
  return text;
}

/// A table read from tech_text(lef_lines) with the DRC rules at lambda.
struct Built {
  std::shared_ptr<const tech::Technology> tech;
  std::shared_ptr<const drc::RuleSet> rules;
  LayerTable table;
  std::vector<diag::Diagnostic> diagnostics;  // of the lef section only

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const auto& d : diagnostics) {
      out.push_back(d.text);
    }
    return out;
  }

  [[nodiscard]] std::vector<std::string> codes() const {
    std::vector<std::string> out;
    for (const auto& d : diagnostics) {
      out.push_back(d.code);
    }
    return out;
  }
};

inline Built build(std::string_view lef_lines, bool magic_compat = true, std::string_view drc = kDrc) {
  platform::MemoryFileSystem fs;
  diag::CollectingSink load_sink;
  auto loaded = tech::load_technology_text(tech_text(lef_lines, drc), "/t/lef.tech", fs, tech::LoadOptions{},
                                           load_sink);
  REQUIRE(loaded.ok());
  Built b;
  b.tech = loaded.value();
  const compat::CompatSettings compat{magic_compat};
  diag::NullSink quiet;
  b.rules = drc::read_rules(*b.tech, compat, quiet).rules;
  diag::CollectingSink sink;
  b.table = read_layer_section(*b.tech, b.rules.get(), compat, sink);
  b.diagnostics = sink.take();
  return b;
}

/// A command session over in-memory services.
struct Session {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(bool magic_compat = true) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  /// Runs a `;`-separated line; returns the first failure, else the last
  /// result. Every diagnostic is kept in `messages`.
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(*dispatcher, line, cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        messages.push_back(d);
      }
      if (!r.ok()) {
        return std::move(r);
      }
      last = std::move(r);
    }
    return last;
  }

  /// `tech load` of tech_text(lef_lines) at the current editor.grid.
  void load(std::string_view lef_lines, std::string_view drc = kDrc) {
    file("/t/lef.tech", tech_text(lef_lines, drc));
    REQUIRE(run("tech load /t/lef.tech -noprompt").ok());
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  [[nodiscard]] const LayerRecord& record(std::string_view lef_name) const {
    const LayerRecord* r = editor.lefdef.layers.find(lef_name);
    REQUIRE(r != nullptr);
    return *r;
  }

  [[nodiscard]] db::Coord default_width(std::string_view type_name) const {
    return drc::default_width(*editor.drc.rules, *editor.technology, type(type_name));
  }

  [[nodiscard]] db::Coord default_spacing(std::string_view type_name) const {
    const tech::TileType t = type(type_name);
    return drc::default_spacing(*editor.drc.rules, *editor.technology, t, t);
  }

  std::vector<diag::Diagnostic> messages;
};

}  // namespace hefesto::lef::test
