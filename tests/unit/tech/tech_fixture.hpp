// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the TECH unit tests: the annotated example technology
// (tests/data/tech/tiny.tech), text edits of it, and loads over an
// in-memory file system with the diagnostics collected.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::tech::test {

inline std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

/// The valid `tiny.tech` of tech-file-format "Complete annotated example".
inline std::string tiny() {
  static const std::string text = read_file(std::string{HEFESTO_TEST_DATA_DIR} + "/tiny.tech");
  return text;
}

/// `text` with the first occurrence of `from` replaced by `to`.
inline std::string with(std::string text, std::string_view from, std::string_view to) {
  const auto pos = text.find(from);
  REQUIRE(pos != std::string::npos);
  text.replace(pos, from.size(), to);
  return text;
}

struct LoadRun {
  std::shared_ptr<const Technology> tech;  // null when the load failed
  std::vector<diag::Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const { return tech != nullptr; }
  [[nodiscard]] bool has(std::string_view code) const {
    for (const auto& d : diagnostics) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] bool says(std::string_view text) const {
    for (const auto& d : diagnostics) {
      if (d.text.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
  [[nodiscard]] std::size_t count(std::string_view text) const {
    std::size_t n = 0;
    for (const auto& d : diagnostics) {
      n += d.text.find(text) != std::string::npos ? 1U : 0U;
    }
    return n;
  }
  [[nodiscard]] const Technology& t() const {
    REQUIRE(tech != nullptr);
    return *tech;
  }
};

/// Loads `text` as `/t/x.tech` from `fs` (other files may be put there first).
inline LoadRun load_in(platform::MemoryFileSystem& fs, const std::string& text, bool magic_compat = true,
                       const std::string& path = "/t/x.tech") {
  REQUIRE(fs.write_atomic(path, text).ok());
  diag::CollectingSink sink;
  LoadOptions options;
  options.magic_compat = magic_compat;
  auto result = load_technology(fs, path, options, sink);
  LoadRun run;
  run.diagnostics = sink.take();
  if (result.ok()) {
    run.tech = result.value();
  }
  return run;
}

inline LoadRun load(const std::string& text, bool magic_compat = true) {
  platform::MemoryFileSystem fs;
  return load_in(fs, text, magic_compat);
}

/// The annotated example, loaded (must succeed).
inline std::shared_ptr<const Technology> tiny_tech() {
  static const std::shared_ptr<const Technology> tech = [] {
    auto run = load(tiny());
    REQUIRE(run.ok());
    return run.tech;
  }();
  return tech;
}

/// A type number by name (must resolve).
inline TileType type_of(const Technology& tech, std::string_view name) {
  const auto found = tech.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

inline PlaneId plane_of(const Technology& tech, std::string_view name) {
  const auto found = tech.find_plane(name);
  REQUIRE(found.ok());
  return *found.id;
}

inline TypeMask mask_of(std::initializer_list<TileType> types) {
  TypeMask mask;
  for (const TileType t : types) {
    mask.set(t);
  }
  return mask;
}

inline PlaneMask planes_of(std::initializer_list<PlaneId> planes) {
  PlaneMask mask = 0;
  for (const PlaneId p : planes) {
    mask |= plane_bit(p);
  }
  return mask;
}

/// A minimal file around the given sections (each "name\n...lines...\nend").
inline std::string skeleton(std::string_view planes, std::string_view types, std::string_view contact = "",
                            std::string_view compose = "", std::string_view connect = "",
                            std::string_view styles = "") {
  std::string out = "tech\n  t\nend\nplanes\n";
  out += planes;
  out += "end\ntypes\n";
  out += types;
  out += "end\ncontact\n";
  out += contact;
  out += "end\nstyles\n";
  out += styles;
  out += "end\ncompose\n";
  out += compose;
  out += "end\nconnect\n";
  out += connect;
  out += "end\ncifoutput\nend\ncifinput\nend\ndrc\nend\nextract\nend\n";
  return out;
}

}  // namespace hefesto::tech::test
