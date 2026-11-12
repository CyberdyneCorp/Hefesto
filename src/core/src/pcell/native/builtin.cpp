// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The native generators built into Hefesto (design D10), each registered
// for the exact PDK release it was validated against with
// scripts/pcell_equivalence.py (D-29):
//  - open_pdks ciel release 5740c4a8 (the stand-in of the spec pin
//    c6df28c1, design D15): sky130A and gf180mcuD;
//  - IHP-Open-PDK 5e6d592e: ihp-sg13g2's MOSFETs (the fixed devices wait
//    for SCRIPT-PDK-14 to confirm Q-SCRIPT-45).
// Any other version of these PDKs runs the Tcl generators.

#include <string>
#include <utility>
#include <vector>

#include "hefesto/pcell/native/registry.hpp"
#include "pcell/native/builtin.hpp"
#include "pcell/native/fixed.hpp"

namespace hefesto::pcell {

namespace {

constexpr const char* kOpenPdksRelease = "5740c4a81c42f9fe4af5ef2ee258a99151053d28";
constexpr const char* kIhpRelease = "5e6d592e4002946a4616f798c357f0f3c06cf3b6";

struct FixedRow {
  const char* device;    // <type>
  const char* cell;      // the library cell fixed_draw places
  const char* defaults;  // the _defaults dictionary text
};

// PDK ref: open_pdks c6df28c1 sky130A-fix.tcl:50-89, 284-327.
const std::vector<FixedRow>& sky130_fixed() {
  static const std::vector<FixedRow> kRows = {
      {"sky130_fd_pr__npn_05v5_W1p00L1p00", "sky130_fd_pr__rf_npn_05v5_W1p00L1p00",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 7.03 ystep 7.03 class bjt"},
      {"sky130_fd_pr__npn_05v5_W1p00L2p00", "sky130_fd_pr__rf_npn_05v5_W1p00L2p00",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 7.03 ystep 8.03 class bjt"},
      {"sky130_fd_pr__pnp_05v5_W0p68L0p68", "sky130_fd_pr__rf_pnp_05v5_W0p68L0p68",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 3.72 ystep 3.72 class bjt"},
      {"sky130_fd_pr__pnp_05v5_W3p40L3p40", "sky130_fd_pr__rf_pnp_05v5_W3p40L3p40",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 6.44 ystep 6.44 class bjt"},
      {"sky130_fd_pr__npn_11v0_W1p00L1p00", "sky130_fd_pr__rf_npn_11v0_W1p00L1p00",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 7.03 ystep 7.03 class bjt"},
      {"sky130_fd_pr__rf_test_coil1", "sky130_fd_pr__rf_test_coil1",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 258 ystep 258 class inductor"},
      {"sky130_fd_pr__rf_test_coil2", "sky130_fd_pr__rf_test_coil2",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 290 ystep 404 class inductor"},
      {"sky130_fd_pr__rf_test_coil3", "sky130_fd_pr__rf_test_coil3",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 290 ystep 404 class inductor"},
      {"sky130_fd_pr__cap_vpp_11p5x11p7_m1m2m3m4_shieldl1m5", "sky130_fd_pr__cap_vpp_11p5x11p7_m1m2m3m4_shieldl1m5",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 11.08 ystep 11.36 class capacitor"},
      {"sky130_fd_pr__cap_vpp_11p5x11p7_m1m2_noshield", "sky130_fd_pr__cap_vpp_11p5x11p7_m1m2_noshield",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 11.08 ystep 11.36 class capacitor"},
      {"sky130_fd_pr__cap_vpp_08p6x07p8_m1m2_shieldl1", "sky130_fd_pr__cap_vpp_08p6x07p8_m1m2_shieldl1",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 11.08 ystep 11.36 class capacitor"},
      {"sky130_fd_pr__cap_vpp_04p4x04p6_m1m2_shieldl1", "sky130_fd_pr__cap_vpp_04p4x04p6_m1m2_shieldl1",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 11.08 ystep 11.36 class capacitor"},
  };
  return kRows;
}

// PDK ref: open_pdks c6df28c1 gf180mcuD-bjt.tcl:27-69, 262-304 (three cell
// names do not exist in gf180mcu_fd_pr, so those devices fail as in Tcl).
const std::vector<FixedRow>& gf180mcu_fixed() {
  static const std::vector<FixedRow> kRows = {
      {"npn_05p00x05p00", "npn_05p00x05p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 15.29 ystep 22.92 class bjt"},
      {"npn_00p54x02p00", "npn_00p54x02p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 13.55 ystep 14.95 class bjt"},
      {"npn_10p00x10p00", "npn_10p00x10p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 23.04 ystep 23.04 class bjt"},
      {"npn_00p54x10p00", "npn_00p54x10p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 16.94 ystep 16.94 class bjt"},
      {"eFuse", "eFuse_0", "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 16.94 ystep 16.94"},
      {"npn_00p54x04p00", "npn_00p54x04p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 13.92 ystep 16.96 class bjt"},
      {"npn_00p54x08p00", "npn_npn_00p54x08p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 12.36 ystep 21.22 class bjt"},
      {"pnp_05p00x05p00", "pnp_05p00x05p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 8.25 ystep 8.25 class bjt"},
      {"pnp_05p00x00p42", "pnp_05p00x00p42_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 3.69 ystep 8.25 class bjt"},
      {"pnp_10p00x10p00", "pnp_10p00x10p00_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 13.25 ystep 13.25 class bjt"},
      {"pnp_10p00x00p42", "pnp_10p00x00p42_0",
       "nx 1 ny 1 deltax 0 deltay 0 nocell 1 xstep 3.75 ystep 13.25 class bjt"},
  };
  return kRows;
}

void add_fixed(NativeRegistry& registry, const script::PdkIdentity& pdk, const char* library, HelperFamily family,
               const std::vector<FixedRow>& rows) {
  for (const auto& row : rows) {
    registry.add(pdk, std::string{library} + "::" + row.device,
                 native::make_fixed_device(family, row.defaults, row.cell));
  }
}

NativeRegistry make_builtin() {
  NativeRegistry registry;
  const script::PdkIdentity sky130{"sky130", "sky130A", kOpenPdksRelease};
  const script::PdkIdentity gf180mcu{"gf180mcu", "gf180mcuD", kOpenPdksRelease};
  add_fixed(registry, sky130, "sky130", HelperFamily::sky130, sky130_fixed());
  native::add_sky130_mosfets(registry, sky130);
  add_fixed(registry, gf180mcu, "gf180mcu", HelperFamily::gf180mcu, gf180mcu_fixed());
  native::add_gf180mcu_mosfets(registry, gf180mcu);
  native::add_sg13g2_mosfets(registry, script::PdkIdentity{"ihp-sg13", "ihp-sg13g2", kIhpRelease});
  return registry;
}

}  // namespace

const NativeRegistry& builtin_generators() {
  static const NativeRegistry kRegistry = make_builtin();
  return kRegistry;
}

}  // namespace hefesto::pcell
