// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Fixture of the mask/style unit tests: tests/data/mask/style/styles.tech
// with a test's own cifoutput section, read with mask::read_output_styles.
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::mask::test {

inline std::string styles_tech_text() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "styles.tech");
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// styles.tech with `body` (lines without `cifoutput`/`end`) as its
/// cifoutput section.
inline std::shared_ptr<const tech::Technology> tech_with(std::string_view body) {
  std::string text = styles_tech_text();
  text += "\ncifoutput\n";
  text += body;
  text += "\nend\n\ncifinput\nend\n\ndrc\nend\n\nextract\nend\n";
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/styles.tech", fs, tech::LoadOptions{}, sink);
  if (!loaded.ok()) {
    for (const auto& d : sink.take()) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
  }
  REQUIRE(loaded.ok());
  return loaded.value();
}

struct Read {
  std::shared_ptr<const tech::Technology> tech;
  OutputStyleSet set;
  std::vector<diag::Diagnostic> messages;  // emitted by the read (the first style's)

  [[nodiscard]] const OutputStyle& style(std::size_t i = 0) const {
    REQUIRE(i < set.styles.size());
    return *set.styles[i];
  }
  [[nodiscard]] tech::TileType type(std::string_view name) const {
    const auto found = tech->find_type(name);
    REQUIRE(found.ok());
    return *found.id;
  }
  [[nodiscard]] tech::TypeMask types(std::initializer_list<std::string_view> names) const {
    tech::TypeMask mask;
    for (const std::string_view name : names) {
      mask.set(type(name));
    }
    return mask;
  }
  /// The texts of the emitted messages, in order.
  [[nodiscard]] std::vector<std::string> texts() const {
    std::vector<std::string> out;
    for (const auto& d : messages) {
      out.push_back(d.text);
    }
    return out;
  }
};

inline Read read(std::string_view body) {
  Read r;
  r.tech = tech_with(body);
  diag::CollectingSink sink;
  r.set = read_output_styles(*r.tech, sink);
  r.messages = sink.take();
  return r;
}

/// The texts of `messages`.
inline std::vector<std::string> texts_of(const std::vector<diag::Diagnostic>& messages) {
  std::vector<std::string> out;
  for (const auto& d : messages) {
    out.push_back(d.text);
  }
  return out;
}

inline compat::CompatSettings compat_mode(bool magic_compat) {
  compat::CompatSettings c;
  c.magic_compat = magic_compat;
  return c;
}

}  // namespace hefesto::mask::test
