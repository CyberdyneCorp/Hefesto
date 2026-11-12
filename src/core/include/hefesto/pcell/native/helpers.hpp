// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK helper procedures reproduced bit for bit
// (openspec/changes/m4-scripting/design.md D10; capability
// scripting-pdk-integration "PDK helper procedures in the engine
// namespace"; Q-SCRIPT-40): each PDK defines its own `magic::spice2float`
// (with its defects), `u2i`, `i2u`, `3digitpastdecimal` (and sg13g2's
// `%.3f`/`round` variants). The native generators call these, never Tcl;
// the names stay undefined in the interpreter until a PDK script defines
// them (design D7).
//
// Values cross as Tcl strings (the results are the strings Tcl would
// produce, e.g. "2.0", "1e-6", "1"). std::nullopt means the call cannot be
// reproduced natively (an expression outside the reproduced arithmetic, a
// big integer, or a Tcl error outside the procedure's own `catch`): the
// caller then runs the PDK's Tcl generator instead.
//
// (PDK ref: open_pdks sky130/magic/sky130.tcl and gf180mcu/magic/gf180mcu.tcl
// `magic::i2u`, `u2i`, `spice2float`, `3digitpastdecimal` (identical in both);
// IHP ihp-sg13g2-util.tcl:691-787.)
//
// Owner: PCELL (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hefesto::pcell {

enum class HelperFamily : unsigned char { sky130, gf180mcu, sg13g2 };

/// The result of magic::spice2float.
struct SpiceFloat {
  std::string value;     // the Tcl result
  bool faulted = false;  // "Value is not numeric!" was printed and `faultval` returned
};

/// The helpers of one PDK family.
struct PdkHelpers {
  HelperFamily family = HelperFamily::sky130;
  /// `[magic::cif scale output]` as the engine returns it (microns per
  /// internal unit).
  std::string cif_scale_output = "1";

  /// magic::spice2float value ?faultval?: SPICE suffix parsing with the
  /// PDKs' defects (`string map` after `string tolower`, so uF/nF/pF/aF
  /// never match; `n` maps to the invalid "* 1.0 e-9" and faults; `a` is
  /// 1e-15) and an unbraced `expr`.
  [[nodiscard]] std::optional<SpiceFloat> spice2float(std::string_view value,
                                                      std::string_view faultval = "0.0") const;
  /// magic::u2i: microns to internal units (sg13g2 rounds to an integer).
  [[nodiscard]] std::optional<std::string> u2i(std::string_view microns) const;
  /// magic::i2u: internal units to microns (sg13g2 formats with %.3f).
  [[nodiscard]] std::optional<std::string> i2u(std::string_view internal) const;
  /// magic::3digitpastdecimal: `int(value * 1000 + 0.5) / 1000.0`.
  [[nodiscard]] std::optional<std::string> three_digit_past_decimal(std::string_view value) const;
};

}  // namespace hefesto::pcell
