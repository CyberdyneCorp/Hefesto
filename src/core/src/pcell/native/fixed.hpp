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

}  // namespace hefesto::pcell::native
