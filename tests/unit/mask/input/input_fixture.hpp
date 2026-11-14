// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the mask/input unit tests: tests/data/mask/input/input.tech
// with a test's own cifinput (and optionally cifoutput) section, read with
// mask::read_input_styles, and a session to run the read engine in.
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

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
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/mask/input/engine.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask::test {

inline compat::CompatSettings mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

inline std::string input_tech_text() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "input.tech");
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// input.tech with `cifinput` (lines without `cifinput`/`end`) and
/// `cifoutput` sections.
inline std::shared_ptr<const tech::Technology> tech_with(std::string_view cifinput, std::string_view cifoutput = {}) {
  std::string text = input_tech_text();
  text += "\ncifoutput\n";
  text += cifoutput;
  text += "\nend\n\ncifinput\n";
  text += cifinput;
  text += "\nend\n\ndrc\nend\n\nextract\nend\n";
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/input.tech", fs, tech::LoadOptions{}, sink);
  if (!loaded.ok()) {
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

inline std::vector<std::string> texts_of(const std::vector<diag::Diagnostic>& messages) {
  std::vector<std::string> out;
  for (const auto& d : messages) {
    out.push_back(d.text);
  }
  return out;
}

/// The cifinput section read with `compat`.
struct Read {
  std::shared_ptr<const tech::Technology> tech;
  InputStyleSet set;
  std::vector<diag::Diagnostic> messages;

  [[nodiscard]] const InputStyle& style(std::size_t i = 0) const {
    REQUIRE(i < set.styles.size());
    return *set.styles[i];
  }
  [[nodiscard]] std::vector<std::string> texts() const { return texts_of(messages); }
  [[nodiscard]] bool has(std::string_view text) const {
    for (const auto& d : messages) {
      if (d.text == text) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] bool has_code(std::string_view code) const {
    for (const auto& d : messages) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }
};

inline Read read(std::string_view body, bool magic_compat = true) {
  Read r;
  r.tech = tech_with(body);
  diag::CollectingSink sink;
  r.set = read_input_styles(*r.tech, mode(magic_compat), sink);
  r.messages = sink.take();
  return r;
}

/// A session with the technology installed (and its first output style),
/// ready for a read engine.
struct Session {
  compat::CompatSettings compat;
  editor::EditorState editor;
  InputStyleSet inputs;
  diag::CollectingSink messages;

  explicit Session(std::string_view cifinput, bool magic_compat = true, std::string_view cifoutput = {}) {
    compat = mode(magic_compat);
    editor.technology = tech_with(cifinput, cifoutput);
    editor.drc.compat = &compat;
    diag::CollectingSink quiet;
    editor.mask.output_styles = read_output_styles(*editor.technology, quiet, compat);
    inputs = read_input_styles(*editor.technology, compat, quiet);
    REQUIRE(inputs.current_style() != nullptr);
  }

  [[nodiscard]] ReadEngine engine(ReadEngine::Options options = {}) {
    if (options.sink == nullptr) {
      options.sink = &messages;
    }
    return ReadEngine{editor, *inputs.current_style(), compat, options};
  }

  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = editor.technology->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }

  /// The rectangles of `type` on its home plane of cell `cell`, in
  /// enumeration order (split tiles by bounding box).
  [[nodiscard]] std::vector<db::Rect> painted(std::string_view cell, std::string_view type_name) const {
    const db::CellDef* def = editor.cells.find(cell);
    REQUIRE(def != nullptr);
    const tech::TileType t = type(type_name);
    const auto home = editor.technology->home_plane(t);
    REQUIRE(home.has_value());
    tech::TypeMask mask;
    mask.set(t);
    std::vector<db::Rect> out;
    def->plane(*home).enumerate(db::whole_plane(), mask, [&](const db::TileView& v) {
      out.push_back(v.area);
      return true;
    });
    return out;
  }
};

}  // namespace hefesto::mask::test
