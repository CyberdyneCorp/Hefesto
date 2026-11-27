// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the route/tech unit tests: a small test technology
// (three metals, poly, three contacts, DRC width and spacing rules) with a
// caller-supplied `mzrouter` section, loading from text or from a file, and
// derive_after_load against the technology's own DRC rules and LEF table
// (as `tech load` installs them at a 1:1 grid).
//
// Owner: MZTECH.

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
#include "hefesto/drc/rules/style.hpp"
#include "hefesto/lef/tech/lifecycle.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::route::test {

/// The test technology up to (and without) its `mzrouter` section.
inline constexpr std::string_view kTechHead = R"(tech
    format 35
    routetest
end

planes
    well,w
    active,a
    metal1,m1
    metal2,m2
    metal3,m3
end

types
    well    nwell,nw
    active  polysilicon,poly,p
    active  ndiffusion,ndiff
    metal1  metal1,m1
    metal2  metal2,m2
    metal3  metal3,m3
    active  polycontact,pcontact,pc
    metal1  m2contact,m2c,via
    metal2  m3contact,m3c,via2
end

contact
    pc   poly  metal1
    m2c  metal1 metal2
    m3c  metal2 metal3
end

styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
    metal3 22
end

compose
end

connect
    poly,pc      poly,pc
    metal1,pc,m2c metal1,pc,m2c
    metal2,m2c,m3c metal2,m2c,m3c
    metal3,m3c   metal3,m3c
end

cifoutput
end

cifinput
end

drc
    width poly 2 "poly width < 2"
    width metal1 3 "metal1 width < 3"
    width metal2 3 "metal2 width < 3"
    width metal3 6 "metal3 width < 6"
    width m2c 2 "m2c width < 2"
    spacing metal1 metal1 3 touching_ok "metal1 spacing < 3"
    spacing metal2 metal2 4 touching_ok "metal2 spacing < 4"
    spacing poly ndiff 1 touching_illegal "poly ndiff spacing < 1"
end

extract
end
)";

/// The test technology with `mzrouter` section body `body` (nothing when
/// `body` is empty) and `extra` appended (e.g. a `lef` section).
inline std::string tech_text(std::string_view body, std::string_view extra = {}) {
  std::string text{kTechHead};
  if (!body.empty()) {
    text += "\nmzrouter\n";
    text += body;
    text += "\nend\n";
  }
  text += extra;
  return text;
}

/// Loads `text` as a technology file, with the load's messages to `sink`.
inline std::shared_ptr<const tech::Technology> load_text(const std::string& text, diag::DiagnosticSink& sink,
                                                         bool magic_compat = true) {
  platform::MemoryFileSystem fs;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  auto loaded = tech::load_technology_text(text, "/test/route.tech", fs, options, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// Loads the technology file at `path`.
inline std::shared_ptr<const tech::Technology> load_file(const std::filesystem::path& path,
                                                         bool magic_compat = true) {
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  tech::LoadOptions options;
  options.magic_compat = magic_compat;
  auto loaded = tech::load_technology(files, path.string(), options, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// Reads a whole file (empty when it cannot be read).
inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// The styles as derive_after_load leaves them for `technology` (DRC rules
/// of its first style, its LEF layer table), with its messages to `sink`.
inline MazeStyles derived_styles(const tech::Technology& technology, bool magic_compat, diag::DiagnosticSink& sink) {
  const compat::CompatSettings compat{magic_compat};
  const drc::LoadedRules rules = drc::read_rules(technology, compat, sink);
  const lef::LayerTable table = lef::read_layer_section(technology, rules.rules.get(), compat, sink);
  MazeStyles styles = technology.maze_styles();
  derive_after_load(styles, technology, rules.rules.get(), table, magic_compat, sink);
  return styles;
}

inline MazeStyles derived_styles(const tech::Technology& technology, bool magic_compat) {
  diag::NullSink sink;
  return derived_styles(technology, magic_compat, sink);
}

/// The texts of the diagnostics with code prefix `prefix`.
inline std::vector<std::string> texts(const std::vector<diag::Diagnostic>& diagnostics, std::string_view prefix) {
  std::vector<std::string> out;
  for (const diag::Diagnostic& d : diagnostics) {
    if (d.code.starts_with(prefix)) {
      out.push_back(d.text);
    }
  }
  return out;
}

/// The route layer of `style` on tile type `type`, or nullptr.
inline const RouteLayer* layer_of(const MazeStyle& style, tech::TileType type) {
  for (const RouteLayer& layer : style.layers) {
    if (layer.route.type == type) {
      return &layer;
    }
  }
  return nullptr;
}

/// The route contact of `style` on tile type `type`, or nullptr.
inline const RouteContact* contact_of(const MazeStyle& style, tech::TileType type) {
  for (const RouteContact& contact : style.contacts) {
    if (contact.route.type == type) {
      return &contact;
    }
  }
  return nullptr;
}

/// The tile type named `name` in `technology` (fails the test when unknown).
inline tech::TileType type_named(const tech::Technology& technology, std::string_view name) {
  const tech::Lookup<tech::TileType> found = technology.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

}  // namespace hefesto::route::test
