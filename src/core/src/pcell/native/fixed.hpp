// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Native fixed-layout (`nocell`) devices of sky130 and gf180mcu (design
// D10): the ports of `<pdk>::fixed_check` and `<pdk>::fixed_draw`, which
// are the same procedure in both PDKs (PDK ref: open_pdks c6df28c1
// sky130/magic/sky130.tcl -> sky130A-fix.tcl:250-376 and
// gf180mcu/magic/gf180mcu-bjt.tcl -> gf180mcuD-bjt.tcl:228-354): the
// device places the library cell `cell` with `getcell` and arrays it.
//
// Owner: PCELL (design D2).

#pragma once

#include <memory>
#include <string>

#include "hefesto/pcell/native/registry.hpp"

namespace hefesto::pcell::native {

/// A fixed device whose `_defaults` returns `defaults` (the dictionary
/// text) and whose `_draw` calls `fixed_draw <cell>`.
[[nodiscard]] std::unique_ptr<NativeGenerator> make_fixed_device(HelperFamily family, std::string defaults,
                                                                 std::string cell);

/// The base cell an sg13g2 fixed device builds before placing it (PDK ref:
/// IHP-Open-PDK 5e6d592e ihp-sg13g2-fix.tcl:268-580): `<type>_draw` runs
/// `<type>_generate` when `cellname list exists <cell>` is 0, which loads
/// `cell`, sources `${sg13g2::script_path}/<cell>.tcl` (the PDK keeps the
/// layout as a Tcl script) and loads the window's cell again.
struct BaseCellScript {
  std::string guard;  // the cell `_generate` tests first (schottky's tests "schottky_cell")
};

/// An sg13g2 fixed device: the `fixed_check`/`fixed_draw` of the other PDKs
/// (the same text) after the base cell is generated. With magic-compat ON the
/// generation is Magic's sequence, which loses the edit cell, view and box
/// (SCRIPT-PDK-14: the first device is not placed); with it OFF the edit
/// context is put back afterwards and the device is placed in the edit cell
/// (Q-SCRIPT-45, resolution: compat).
[[nodiscard]] std::unique_ptr<NativeGenerator> make_generated_fixed_device(std::string defaults, std::string cell,
                                                                           BaseCellScript base);

}  // namespace hefesto::pcell::native
