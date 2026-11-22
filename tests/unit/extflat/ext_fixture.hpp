// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A memory file system holding `.ext` files under /w, and read_flat over
// it, for the extflat tests. Owner: EXTSUB.

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::extflat::test {

struct ExtFixture {
  platform::MemoryFileSystem fs;
  cell::SearchPaths paths{{"/w"}, {}, {}};
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};

  void file(std::string_view cell, std::string_view text) {
    (void)fs.write_atomic("/w/" + std::string{cell} + ".ext", text);
  }

  diag::Outcome<Netlist> read(std::string_view root, const ReadOptions& options = {}) {
    return read_flat(ctx, root, options);
  }

  /// extflat::read (the M7 reader) with netlister options: coordinates
  /// scaled, couplings flattened.
  diag::Outcome<Hierarchy> read_hier(std::string_view root, ReadOptions options = netlister_options()) {
    return extflat::read(ctx, root, options);
  }

  /// read_hier, then flatten with `flags` (the messages go to `sink`).
  diag::Outcome<Netlist> flat(std::string_view root, FlatFlags flags = netlister_flags(),
                              const FlattenOptions& fo = {}, ReadOptions options = netlister_options()) {
    auto h = read_hier(root, std::move(options));
    if (!h.ok()) {
      return h.error();
    }
    return extflat::flatten(h.value(), flags, fo, sink);
  }

  /// ext2spice's read: dosubckt off, scaled, SPICE.
  static ReadOptions netlister_options() {
    ReadOptions o;
    o.noscale = false;
    o.spice = true;
    o.subcircuits = false;
    return o;
  }
  static FlatFlags netlister_flags() { return FlatFlags::flat_nodes | FlatFlags::flat_caps; }

  /// The texts of the diagnostics with `code`.
  [[nodiscard]] std::vector<std::string> texts(std::string_view code) const {
    std::vector<std::string> out;
    for (const auto& d : sink.snapshot()) {
      if (d.code == code) {
        out.push_back(d.text);
      }
    }
    return out;
  }
  [[nodiscard]] std::vector<std::string> all_texts() const {
    std::vector<std::string> out;
    for (const auto& d : sink.snapshot()) {
      out.push_back(d.text);
    }
    return out;
  }
};

/// The canonical names in visit order.
inline std::vector<std::string> names(const Netlist& n) {
  std::vector<std::string> out;
  n.visit_nodes([&](const FlatNode& node) { out.push_back(node.name); });
  return out;
}

/// Every name of one node, joined with `/`.
inline std::vector<std::string> all_names(const FlatNode& node) {
  std::vector<std::string> out;
  for (const auto& components : node.names) {
    std::string s;
    for (std::size_t i = 0; i < components.size(); ++i) {
      s += (i == 0 ? "" : "/") + components[i];
    }
    out.push_back(s);
  }
  return out;
}

/// A fixture over the committed `.ext` inputs of a NETLIST golden case
/// (tests/golden/NETLIST/cases/<id>/input/).
struct GoldenCase {
  explicit GoldenCase(std::string_view id)
      : paths{{std::string{HEFESTO_SOURCE_DIR} + "/tests/golden/NETLIST/cases/" + std::string{id} + "/input"}, {}, {}} {}

  platform::LocalFileSystem fs;
  cell::SearchPaths paths;
  cell::PathVariables vars;
  diag::CollectingSink sink;
  cell::LookupContext ctx{fs, paths, vars, sink};

  diag::Outcome<Netlist> flat(std::string_view root, FlatFlags flags = ExtFixture::netlister_flags(),
                              const FlattenOptions& fo = {}, ReadOptions options = ExtFixture::netlister_options()) {
    auto h = extflat::read(ctx, root, options);
    if (!h.ok()) {
      return h.error();
    }
    return extflat::flatten(h.value(), flags, fo, sink);
  }

  [[nodiscard]] std::vector<std::string> texts(std::string_view code) const {
    std::vector<std::string> out;
    for (const auto& d : sink.snapshot()) {
      if (d.code == code) {
        out.push_back(d.text);
      }
    }
    return out;
  }
};

/// The nltest.tech tile types of the NL cases' device names (Magic's type
/// indices: the technology's types from TT_TECHDEPBASE = 9 in declaration
/// order; extGetDevType of the extract section's devices), DBNumTypes and
/// the style's units per lambda.
inline ReadOptions nltest_options(ReadOptions o = ExtFixture::netlister_options()) {
  o.device_tile_type = [](std::string_view name) -> std::optional<std::int32_t> {
    if (name == "nfet") return 13;
    if (name == "pfet") return 14;
    if (name == "rppd") return 16;
    if (name == "mimcapm") return 20;
    if (name == "dn") return 11;
    if (name == "npn") return 17;
    return std::nullopt;
  };
  o.tile_type_count = 21;
  o.units_per_lambda = 1.0F;  // `lambda 1`: the files' lscale 5 gives locScale 5
  return o;
}

/// "<x>x<y>" of each device's local rectangle, in visit order.
inline std::vector<std::string> device_places(const Netlist& n) {
  std::vector<std::string> out;
  for (const FlatDevice& d : n.devices) {
    out.push_back(std::to_string(d.local_area.x0) + "x" + std::to_string(d.local_area.y0));
  }
  return out;
}

/// The terminal node names of a device.
inline std::vector<std::string> terminal_nodes(const FlatDevice& d) {
  std::vector<std::string> out;
  for (const FlatTerminal& t : d.terminals) {
    out.push_back(t.node);
  }
  return out;
}

inline const FlatNode* node_named(const Netlist& n, std::string_view name) {
  const auto& nodes = n.nodes();
  const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const FlatNode& f) { return f.name == name; });
  return it == nodes.end() ? nullptr : &*it;
}

}  // namespace hefesto::extflat::test
