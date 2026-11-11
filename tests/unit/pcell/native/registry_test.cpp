// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability hefesto-scripting "Native PCell generators validated against
// Tcl": a native generator is used only for the exact PDK version it was
// validated against; any other version falls back to the Tcl generator.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/pcell/native/helpers.hpp"
#include "hefesto/pcell/native/registry.hpp"

namespace pcell = hefesto::pcell;

namespace {

struct Fake final : pcell::NativeGenerator {
  pcell::ParameterDict defaults() const override { return {}; }
  std::optional<hefesto::diag::Outcome<pcell::ParameterDict>> check(pcell::NativeHost&,
                                                                    const pcell::ParameterDict& p) const override {
    return hefesto::diag::Outcome<pcell::ParameterDict>{p};
  }
  std::optional<hefesto::cmd::CommandResult> draw(pcell::NativeHost&, const pcell::ParameterDict&) const override {
    return hefesto::cmd::CommandResult::success();
  }
};

std::size_t count_for(const hefesto::script::PdkIdentity& pdk) {
  std::size_t n = 0;
  for (const auto& [key, device] : pcell::builtin_generators().keys()) {
    n += key == pdk ? 1U : 0U;
  }
  return n;
}

}  // namespace

TEST_CASE("native generators are found only for their exact PDK version",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  pcell::NativeRegistry registry;
  const hefesto::script::PdkIdentity pdk{"sky130", "sky130A", "5740c4a8"};
  registry.add(pdk, "sky130::sky130_fd_pr__nfet_01v8", std::make_unique<Fake>());
  CHECK(registry.find(pdk, "sky130::sky130_fd_pr__nfet_01v8") != nullptr);
  CHECK(registry.find({"sky130", "sky130A", "0fe599b2"}, "sky130::sky130_fd_pr__nfet_01v8") == nullptr);
  CHECK(registry.find(pdk, "sky130::sky130_fd_pr__pfet_01v8") == nullptr);
  CHECK(registry.has_other_version({"sky130", "sky130A", "0fe599b2"}, "sky130::sky130_fd_pr__nfet_01v8"));
  CHECK_FALSE(registry.has_other_version(pdk, "sky130::sky130_fd_pr__nfet_01v8"));
  CHECK_FALSE(registry.has_other_version({"sky130", "sky130B", "0fe599b2"}, "sky130::sky130_fd_pr__nfet_01v8"));
}

TEST_CASE("the built-in generators of design D10 per exact PDK release",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  const std::string open_pdks = "5740c4a81c42f9fe4af5ef2ee258a99151053d28";
  const std::string ihp = "5e6d592e4002946a4616f798c357f0f3c06cf3b6";
  // sky130: 15 MOS devices (every mos_draw user) and 12 fixed devices.
  CHECK(count_for({"sky130", "sky130A", open_pdks}) == 27);
  // gf180mcu: 21 MOS devices and 11 fixed devices.
  CHECK(count_for({"gf180mcu", "gf180mcuD", open_pdks}) == 32);
  // sg13g2: the 4 MOSFETs and their 4 RF variants; the fixed devices wait
  // for SCRIPT-PDK-14 (Q-SCRIPT-45).
  CHECK(count_for({"ihp-sg13", "ihp-sg13g2", ihp}) == 8);
  CHECK(pcell::builtin_generators().size() == 67);
  const auto& builtin = pcell::builtin_generators();
  CHECK(builtin.find({"sky130", "sky130A", open_pdks}, "sky130::sky130_fd_pr__nfet_01v8") != nullptr);
  CHECK(builtin.find({"sky130", "sky130A", open_pdks}, "sky130::sky130_fd_pr__res_generic_po") == nullptr);
  CHECK(builtin.find({"gf180mcu", "gf180mcuD", open_pdks}, "gf180mcu::npn_05p00x05p00") != nullptr);
  CHECK(builtin.find({"ihp-sg13", "ihp-sg13g2", ihp}, "sg13g2::sg13_hv_rfpmos") != nullptr);
  CHECK(builtin.find({"ihp-sg13", "ihp-sg13g2", ihp}, "sg13g2::npn13g2") == nullptr);
  // The pre-M4 pins have none.
  CHECK(builtin.find({"sky130", "sky130A", "0fe599b2afb6708d281543108caf8310912f54af"},
                     "sky130::sky130_fd_pr__nfet_01v8") == nullptr);
}
