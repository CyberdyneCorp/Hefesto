// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Security review, task 12.1 (design D14): the PDK testbenches of sky130,
// gf180mcu and IHP sg13g2 (the pinned xschem data sets of
// scripts/fetch_pdk.py) netlisted through the sandbox as `schematic
// netlist` does: the PDK `xschemrc` and every `tcleval` value they carry
// need nothing the sandbox refuses, and no path outside the granted roots is
// touched. Skipped, per PDK, when its data set is not fetched.
//
// Owner: SECURITY.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"
#include "hefesto/script/tcl/sandbox.hpp"
#include "reference_data.hpp"
#include "security_support.hpp"

namespace schem = hefesto::schem;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace script = hefesto::script;
namespace sec = hefesto::test::security;
namespace stdfs = std::filesystem;

namespace {

const platform::Path kSharedir = platform::Path{HEFESTO_SOURCE_DIR} / "assets" / "schematic";

// At most this many testbenches per PDK (sorted by path), to bound the run.
constexpr std::size_t kMaxBenches = 40;

bool is_testbench(const stdfs::path& file) {
  if (file.extension() != ".sch") return false;
  const std::string name = file.filename().string();
  const std::string folder = file.parent_path().filename().string();
  return name.find("tb") != std::string::npos || name.starts_with("test") || folder.find("test") != std::string::npos;
}

std::vector<stdfs::path> testbenches(const stdfs::path& xschem) {
  std::vector<stdfs::path> found;
  for (const auto& entry : stdfs::recursive_directory_iterator(xschem)) {
    if (entry.is_regular_file() && is_testbench(entry.path())) found.push_back(entry.path());
  }
  std::sort(found.begin(), found.end());
  if (found.size() > kMaxBenches) found.resize(kMaxBenches);
  return found;
}

// Every path the sandbox's filesystem layer denied while `run` ran.
class DeniedPaths {
 public:
  DeniedPaths() {
    script::set_filesystem_audit([this](const script::FilesystemAccess& access) {
      if (access.verdict == script::FilesystemAccess::Verdict::denied) paths_.push_back(access.path);
    });
  }
  ~DeniedPaths() { script::set_filesystem_audit({}); }
  DeniedPaths(const DeniedPaths&) = delete;
  DeniedPaths& operator=(const DeniedPaths&) = delete;

  [[nodiscard]] const std::vector<std::string>& paths() const noexcept { return paths_; }

 private:
  std::vector<std::string> paths_;
};

// Netlists `top` of `pdk` (installed under `root`) as `schematic netlist`
// does, and checks that nothing was denied.
void check_bench(const stdfs::path& root, const std::string& pdk, const stdfs::path& top) {
  INFO(top.generic_string());
  const schem::XschemrcInput input =
      schem::pdk_xschemrc_input(root, pdk, kSharedir, top.parent_path() / ".xschem", top.parent_path());
  script::SandboxPolicy policy = schem::xschemrc_policy(input);
  policy.read_only_roots.push_back(root);
  hefesto::editor::EditorState editor;
  platform::LocalFileSystem files;
  diag::CollectingSink sink;
  DeniedPaths denied;
  schem::TclEnvironment tcl(editor, policy, files);
  tcl.set_diagnostics(&sink);
  const schem::XschemrcResult rc = tcl.load_xschemrc(input);
  auto document = schem::load_document(files, top, schem::ReadOptions{});
  REQUIRE(document.ok());
  schem::SymbolCache symbols;
  const schem::TclBridge bridge = tcl.bridge();
  const schem::NetlistEnvironment environment{symbols, rc.search_path, files, &bridge, root, 4};
  (void)schem::netlist(environment, document.value(), schem::NetlistOptions{}, hefesto::compat::CompatSettings{});
  for (const auto& d : sec::with_code_prefix(rc.diagnostics, "SCRIPT-SANDBOX-")) FAIL_CHECK(d.text);
  for (const auto& d : sink.take()) FAIL_CHECK(d.code + " " + d.text);
  // Tcl probes the process working directory (Tcl_FSGetCwd, when it
  // normalizes a relative path) without accessing it, and the layer records
  // that claim as a denial; a real access there would also have reached
  // `sink` as SCRIPT-SANDBOX-004 above. Every other denied path fails.
  const std::string cwd = stdfs::current_path().generic_string();
  for (const std::string& path : denied.paths()) {
    if (path != cwd) FAIL_CHECK("denied path " + path);
  }
}

void check_pdk(const hefesto::test::PinnedPdk& pinned) {
  const auto xschem = hefesto::test::pinned_xschem(pinned);
  if (!xschem) SKIP(std::string{pinned.variant} + " xschem data set not fetched (scripts/fetch_pdk.py)");
  const stdfs::path root = xschem->parent_path().parent_path().parent_path();
  const std::vector<stdfs::path> benches = testbenches(*xschem);
  CHECK_FALSE(benches.empty());
  for (const stdfs::path& bench : benches) check_bench(root, std::string{pinned.variant}, bench);
}

}  // namespace

TEST_CASE("sky130 testbenches need nothing the sandbox refuses", sec::kPermissionModel) {
  check_pdk(hefesto::test::kSky130A);
}

TEST_CASE("gf180mcu testbenches need nothing the sandbox refuses", sec::kPermissionModel) {
  check_pdk(hefesto::test::kGf180mcuD);
}

TEST_CASE("IHP sg13g2 testbenches need nothing the sandbox refuses", sec::kPermissionModel) {
  check_pdk(hefesto::test::kIhpSg13g2);
}
