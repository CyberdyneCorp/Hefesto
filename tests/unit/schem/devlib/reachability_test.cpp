// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The system library is reachable as `devices/<name>` and `<name>` through
// XSCHEM_SHAREDIR (capability schematic-symbols-and-libraries "System
// library", scenario "IHP testbench with devices prefix"; "Symbol
// references", "Library search path"; design D8 step 4). CMD sets
// XSCHEM_SHAREDIR to <CAD_ROOT>/assets/schematic and the PDK xschemrc files
// append ${XSCHEM_SHAREDIR}/xschem_library/devices and
// ${XSCHEM_SHAREDIR}/xschem_library to the search path; the tests use the
// source tree as CAD_ROOT. The scenario runs on a Hefesto-authored test bench
// that writes its references as IHP's do, and on IHP's own dc_lv_nmos.sch
// when the ihp-xschem data set is fetched (scripts/fetch_pdk.py).
//
// Owner: DEVLIB.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "devlib_support.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "reference_data.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace dl = hefesto::test::devlib;
using platform::Path;

namespace {

constexpr const char* kSystemLibrary = "[cap:schematic-symbols-and-libraries][req:System library]";
constexpr const char* kReferences = "[cap:schematic-symbols-and-libraries][req:Symbol references]";

const Path kBench = dl::kData / "tb";

bool same_file(const Path& a, const Path& b) {
  std::error_code ec;
  return std::filesystem::equivalent(a, b, ec);
}

bool in_library(const Path& file) { return same_file(file.parent_path(), dl::kLibrary); }

/// The search path the bench folder's xschemrc gives with CMD's XSCHEM_SHAREDIR.
schem::SearchPath bench_search_path(platform::FileSystem& fs) {
  schem::XschemrcInput input;
  input.document_folder = kBench;
  input.variables = {{"XSCHEM_SHAREDIR", dl::kSharedir.generic_string()},
                     {"USER_CONF_DIR", (kBench / ".xschem").generic_string()}};
  return schem::SearchPath::from_xschemrc(fs, input).search_path;
}

struct Resolution {
  std::vector<const schem::Symbol*> symbols;
  std::vector<diag::Diagnostic> warnings;
};

Resolution resolve_all(const schem::Document& document, const schem::SearchPath& search, platform::FileSystem& fs,
                       schem::SymbolCache& cache) {
  diag::CollectingSink sink;
  const schem::ResolveContext context{search, fs, &sink, true};
  Resolution out;
  for (const auto& instance : document.instances) {
    out.symbols.push_back(&cache.resolve(schem::Reference{instance.symbol}, document, context));
  }
  out.warnings = sink.take();
  return out;
}

bool is_system_reference(const std::string& reference) {
  const std::string name = reference.rfind("devices/", 0) == 0 ? reference.substr(8) : reference;
  return dl::interface_table().contains(name);
}

}  // namespace

TEST_CASE("the library resolves as devices/<name> and <name> through XSCHEM_SHAREDIR", kReferences) {
  platform::LocalFileSystem fs;
  const schem::SearchPath search = bench_search_path(fs);
  REQUIRE(search.directories().size() == 2);
  CHECK(same_file(search.directories()[0], dl::kLibrary));
  CHECK(same_file(search.directories()[1], dl::kSharedir / "xschem_library"));
  for (const auto& [name, entry] : dl::interface_table().items()) {
    INFO(name);
    const auto bare = schem::resolve_reference(name, kBench, search, fs);
    CHECK(bare.via == schem::ResolvedVia::search_path);
    CHECK(same_file(bare.file, dl::kLibrary / name));
    const auto prefixed = schem::resolve_reference("devices/" + name, kBench, search, fs);
    CHECK(prefixed.via == schem::ResolvedVia::search_path);
    CHECK(same_file(prefixed.file, dl::kLibrary / name));
  }
}

TEST_CASE("IHP testbench with devices prefix", kSystemLibrary) {
  // A Hefesto-authored bench that mixes `devices/<name>` and bare `<name>`
  // references, as IHP's sg13g2_tests benches do.
  platform::LocalFileSystem fs;
  const schem::SearchPath search = bench_search_path(fs);
  auto document = schem::load_document(fs, kBench / "ihp_style_tb.sch", {});
  REQUIRE(document);
  schem::SymbolCache cache;
  const Resolution resolution = resolve_all(document.value(), search, fs, cache);
  CHECK(resolution.warnings.empty());
  REQUIRE(resolution.symbols.size() == document.value().instances.size());
  for (std::size_t i = 0; i < resolution.symbols.size(); ++i) {
    INFO(document.value().instances[i].symbol);
    CHECK_FALSE(resolution.symbols[i]->placeholder);
    CHECK(in_library(resolution.symbols[i]->file));
  }
}

TEST_CASE("IHP testbench with devices prefix, pinned sg13g2_tests/dc_lv_nmos.sch", kSystemLibrary) {
  const auto tree = hefesto::test::pinned_xschem(hefesto::test::kIhpSg13g2);
  if (!tree) SKIP("ihp-xschem data set not fetched (scripts/fetch_pdk.py ihp-xschem)");
  const Path pdk_root = tree->parent_path().parent_path().parent_path();
  const Path bench = *tree / "sg13g2_tests" / "dc_lv_nmos.sch";
  platform::LocalFileSystem fs;
  const auto input =
      schem::pdk_xschemrc_input(pdk_root, "ihp-sg13g2", dl::kSharedir, bench.parent_path() / ".xschem", bench.parent_path());
  const schem::SearchPath search = schem::SearchPath::from_xschemrc(fs, input).search_path;
  REQUIRE(search.directories().size() >= 2);
  CHECK(same_file(search.directories()[0], dl::kLibrary));  // the system library folders come first
  auto document = schem::load_document(fs, bench, {});
  REQUIRE(document);
  schem::SymbolCache cache;
  const Resolution resolution = resolve_all(document.value(), search, fs, cache);
  std::size_t system = 0;
  for (std::size_t i = 0; i < resolution.symbols.size(); ++i) {
    const std::string& reference = document.value().instances[i].symbol;
    INFO(reference);
    CHECK_FALSE(resolution.symbols[i]->placeholder);  // no placeholder for any reference
    if (is_system_reference(reference)) {
      CHECK(in_library(resolution.symbols[i]->file));
      ++system;
    }
  }
  // devices/code_shown.sym, devices/gnd.sym, lab_pin.sym and the others.
  CHECK(system >= 3);
  CHECK(std::none_of(resolution.warnings.begin(), resolution.warnings.end(),
                     [](const diag::Diagnostic& d) { return d.text.find("Symbol not found") != std::string::npos; }));
}
