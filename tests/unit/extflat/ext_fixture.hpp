// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A memory file system holding `.ext` files under /w, and read_flat over
// it, for the extflat tests. Owner: EXTSUB.

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
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

inline const FlatNode* node_named(const Netlist& n, std::string_view name) {
  const auto& nodes = n.nodes();
  const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const FlatNode& f) { return f.name == name; });
  return it == nodes.end() ? nullptr : &*it;
}

}  // namespace hefesto::extflat::test
