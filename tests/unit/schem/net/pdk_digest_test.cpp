// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-platform netlist digests (openspec/changes/m8-schematic design D20,
// task 8.6): every example schematic (`.sch`) of the three PDK xschem data
// sets is netlisted with its PDK's own xschemrc evaluated in the sandbox and
// the system library in place, as `schematic netlist` does, and the SHA-256
// of each netlist is compared with the committed table
// pdk_netlist_digests.txt. The same table on Linux x86_64, Linux aarch64 and
// macOS arm64 proves the netlists byte-identical across the CI legs. Paths
// that name the host (the PDK root, the system library) are written as
// `/pdk` and `$XSCHEM_SHAREDIR` before hashing, as the golden mask does. A set
// that is not fetched (scripts/fetch_pdk.py <set>) is skipped with a message.
//
// Regenerate the table after an intended netlist change with
//   HEFESTO_WRITE_PDK_DIGESTS=1 hefesto_schem_net_tests "[pdk-digest]"
// (all three sets fetched), and review the diff.
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
namespace fs = std::filesystem;
using hefesto::platform::Path;

namespace {

constexpr const char* kDeterminism = "[cap:schematic-netlisting][req:In-process netlisting][pdk-digest]";

const Path kSource{HEFESTO_SOURCE_DIR};
const Path kSharedir = kSource / "assets" / "schematic";
const Path kTable = kSource / "tests/unit/schem/net/pdk_netlist_digests.txt";

/// `<variant>/<path below the variant>` -> `<sha256> <ok|failed>`.
using Digests = std::map<std::string, std::string>;

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  if (from.empty()) return text;
  for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

/// Every `.sch` below `tree`, sorted by generic path (directory order is not portable).
std::vector<Path> schematics(const Path& tree) {
  std::vector<Path> out;
  for (const auto& entry : fs::recursive_directory_iterator(tree)) {
    if (entry.is_regular_file() && entry.path().extension() == ".sch") out.push_back(entry.path());
  }
  std::sort(out.begin(), out.end(), [](const Path& a, const Path& b) { return a.generic_string() < b.generic_string(); });
  return out;
}

/// The netlist of `top` with the PDK `variant` under `root` active, its host paths written portably.
std::pair<std::string, bool> portable_netlist(const Path& root, const std::string& variant, const Path& top) {
  hefesto::editor::EditorState editor;
  hefesto::platform::LocalFileSystem files;
  const Path folder = top.parent_path();
  const schem::XschemrcInput input = schem::pdk_xschemrc_input(root, variant, kSharedir, folder / ".xschem", folder);
  hefesto::script::SandboxPolicy policy = schem::xschemrc_policy(input);
  policy.read_only_roots.push_back(root);
  policy.read_only_roots.push_back(kSharedir);
  schem::TclEnvironment tcl(editor, std::move(policy), files);
  const schem::XschemrcResult rc = tcl.load_xschemrc(input);
  auto document = schem::load_document(files, top, schem::ReadOptions{});
  if (!document) return {"unreadable", true};
  schem::SymbolCache symbols;
  const schem::TclBridge bridge = tcl.bridge();
  const schem::NetlistEnvironment environment{symbols, rc.search_path, files, &bridge, root, 4};
  const schem::NetlistResult result =
      schem::netlist(environment, document.value(), schem::NetlistOptions{}, hefesto::compat::CompatSettings{});
  std::string text = replace_all(result.text, root.generic_string(), "/pdk");
  text = replace_all(std::move(text), kSharedir.generic_string(), "$XSCHEM_SHAREDIR");
  return {std::move(text), result.failed};
}

/// The digests of every example schematic of one set; nullopt when the set is not fetched.
std::optional<Digests> set_digests(const hefesto::test::PinnedPdk& pinned) {
  const auto tree = hefesto::test::pinned_xschem(pinned);
  if (!tree) return std::nullopt;
  const Path root = tree->parent_path().parent_path().parent_path();
  const std::string variant{pinned.variant};
  Digests out;
  for (const Path& top : schematics(*tree)) {
    const auto [text, failed] = portable_netlist(root, variant, top);
    const std::string key = variant + "/" + fs::relative(top, root / variant).generic_string();
    out[key] = hefesto::pdk::sha256_hex(text) + (failed ? " failed" : " ok");
  }
  return out;
}

Digests read_table() {
  Digests out;
  std::ifstream in(kTable);
  for (std::string line; std::getline(in, line);) {
    if (line.empty() || line.front() == '#') continue;
    const std::size_t space = line.find(' ');
    if (space != std::string::npos) out[line.substr(0, space)] = line.substr(space + 1);
  }
  return out;
}

void write_table(const Digests& digests) {
  std::ofstream out(kTable, std::ios::binary);
  out << "# SPDX-License-Identifier: Apache-2.0\n# Copyright 2026 The Hefesto Authors\n"
      << "# Netlist digests of the PDK xschem example schematics (pdk_digest_test.cpp, m8-schematic D20):\n"
      << "# <variant>/<schematic> <sha256 of the portable netlist> <ok|failed>\n";
  for (const auto& [key, value] : digests) out << key << ' ' << value << '\n';
}

/// The table's entries of one variant.
Digests of_variant(const Digests& table, const std::string& variant) {
  Digests out;
  for (const auto& [key, value] : table) {
    if (key.rfind(variant + "/", 0) == 0) out[key] = value;
  }
  return out;
}

}  // namespace

TEST_CASE("PDK example netlists have the committed digests", kDeterminism) {
  const bool write = std::getenv("HEFESTO_WRITE_PDK_DIGESTS") != nullptr;
  const Digests table = read_table();
  Digests all;
  std::vector<std::string> skipped;
  for (const auto* pinned : {&hefesto::test::kSky130A, &hefesto::test::kGf180mcuD, &hefesto::test::kIhpSg13g2}) {
    const std::string variant{pinned->variant};
    const auto digests = set_digests(*pinned);
    if (!digests) {
      skipped.push_back(variant);
      continue;
    }
    all.insert(digests->begin(), digests->end());
    if (write) continue;
    CAPTURE(variant);
    const Digests expected = of_variant(table, variant);
    REQUIRE_FALSE(expected.empty());
    for (const auto& [key, value] : expected) {
      INFO(key);
      const auto it = digests->find(key);
      REQUIRE(it != digests->end());
      CHECK(it->second == value);
    }
    CHECK(digests->size() == expected.size());
  }
  if (write) {
    REQUIRE(skipped.empty());
    write_table(all);
  }
  if (!skipped.empty()) {
    std::string sets;
    for (const std::string& s : skipped) sets += " " + s;
    SKIP("xschem data set not fetched (scripts/fetch_pdk.py):" + sets);
  }
}
