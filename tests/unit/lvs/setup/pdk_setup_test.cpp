// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-setup-file "PDK setup files evaluate unchanged" (Q-LVS-31; NG-PDK-01..03
// setup part): the three installed netgen setup files, read unedited from the
// pinned data sets of scripts/fetch_pdk.py (sky130-xschem, gf180mcu-netgen,
// ihp-netgen), evaluate with no error against small Hefesto-authored
// netlists that use the PDK's device names. A set that was not fetched
// skips cleanly.
//
// Owner: LVSSETUP.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/platform/file_system.hpp"
#include "reference_data.hpp"
#include "setup_fixture.hpp"

namespace {

namespace lvs = hefesto::lvs;
namespace test = hefesto::test;
using setup_test::Pair;

struct PdkCase {
  const test::PinnedToolFile* file;
  std::string_view netlist;
};

constexpr std::string_view kSky130 =
    ".subckt top a y vdd vss\n"
    "X1 y a vss vss sky130_fd_pr__nfet_01v8 w=1 l=0.15\n"
    "X2 y a vdd vdd sky130_fd_pr__pfet_01v8 w=2 l=0.15\n"
    "X3 a y vss sky130_fd_pr__res_generic_po w=0.35 l=2\n"
    "X4 a vss sky130_fd_pr__cap_mim_m3_1 w=2 l=2\n"
    ".ends\n";
constexpr std::string_view kGf180 =
    ".subckt top a y vdd vss\n"
    "X1 y a vss vss nfet_03v3 w=1 l=0.28\n"
    "X2 y a vdd vdd pfet_03v3 w=2 l=0.28\n"
    "X3 a y vss ppolyf_u w=1 l=2\n"
    "X4 a vss cap_mim_2f0_m4m5_noshield w=2 l=2\n"
    ".ends\n";
constexpr std::string_view kIhp =
    ".subckt top a y vdd vss\n"
    "X1 y a vss vss sg13_lv_nmos w=1 l=0.13\n"
    "X2 y a vdd vdd sg13_lv_pmos w=2 l=0.13\n"
    "X3 a y vss rppd w=0.5 l=2\n"
    "X4 a vss cap_cmim w=2 l=2\n"
    ".ends\n";

void evaluate_pdk_setup(const PdkCase& pdk, bool compat) {
  const auto path = test::pinned_tool_file(*pdk.file);
  if (!path) {
    SKIP("netgen setup file not fetched (scripts/fetch_pdk.py): " + std::string{pdk.file->path});
  }
  Pair pair(pdk.netlist);
  hefesto::platform::LocalFileSystem files;
  auto ctx = pair.context(compat);
  ctx.files = &files;
  ctx.sandbox.read_only_roots.push_back(path->parent_path());
  const auto state = lvs::evaluate_setup(lvs::Source{path->string(), std::nullopt}, ctx);
  REQUIRE(state.ok());
  for (const auto& message : state.value().log) {
    UNSCOPED_INFO(message.text);
  }
  CHECK(state.value().evaluated);
  CHECK(state.value().errors == 0);
  // The file's commands ran: its property and permute declarations were recorded.
  CHECK(state.value().properties.size() + state.value().permutes.size() > 0);
}

}  // namespace

TEST_CASE("NG-PDK-01 sky130A_setup.tcl evaluates with no error", "[cap:lvs-setup-file][req:PDK setup files evaluate unchanged]") {
  for (const bool compat : {true, false}) {
    evaluate_pdk_setup(PdkCase{&test::kSky130NetgenSetup, kSky130}, compat);
  }
}

TEST_CASE("NG-PDK-02 gf180mcuD_setup.tcl evaluates with no error", "[cap:lvs-setup-file][req:PDK setup files evaluate unchanged]") {
  for (const bool compat : {true, false}) {
    evaluate_pdk_setup(PdkCase{&test::kGf180mcuNetgenSetup, kGf180}, compat);
  }
}

TEST_CASE("NG-PDK-03 ihp-sg13g2_setup.tcl evaluates with no error", "[cap:lvs-setup-file][req:PDK setup files evaluate unchanged]") {
  for (const bool compat : {true, false}) {
    evaluate_pdk_setup(PdkCase{&test::kIhpNetgenSetup, kIhp}, compat);
  }
}

TEST_CASE("The same setup file gives the same state on every evaluation", "[cap:lvs-setup-file][req:Setup file language and runtime]") {
  constexpr std::string_view kSetup =
      "permute default\nproperty default\n"
      "foreach dev {nfet pfet} { property \"-circuit1 $dev\" tolerance {W 0.02} }\n"
      "equate -unique classes {inv 0} {inv 1}\n";
  Pair a;
  Pair b;
  const auto first = a.ok(kSetup);
  const auto second = b.ok(kSetup);
  CHECK(first.log == second.log);
  CHECK(first.equated == second.equated);
  CHECK(a.cell(1, "nfet").properties == b.cell(1, "nfet").properties);
}
