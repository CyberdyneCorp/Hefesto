// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SpiceSettings: the session-sticky `ext2spice` options (capability
// netlist-spice-options "Command options"; openspec/changes/m7-extraction
// design.md D5, D14), with Magic's defaults. The `lvs` preset and the
// settings simulation-netlist-preparation overrides are values of this
// type; render() takes one by value and never writes it back (D18).
//
// `scale` is also the M4 PDK startup setting `ext2spice scale` (user
// decision M7-3, 2026-11-22: M4's partial ext2spice is folded into this
// command).
//
// Owner: INTEGRATION (this header); SPICE (the members' semantics).

#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/extflat/names.hpp"

namespace hefesto::netlist::spice {

using Dialect = extflat::SpiceDialect;

/// `merge none|conservative|aggressive`.
enum class MergeMode : std::uint8_t { none, conservative, aggressive };
/// `short none|resistor|voltage`.
enum class ShortMode : std::uint8_t { none, resistor, voltage };
/// `subcircuits top on|off|auto`.
enum class TopSubckt : std::uint8_t { off, on, automatic };

struct SpiceSettings {
  Dialect format = Dialect::spice3;
  double cthresh = 2.0;  // fF; infinity = no parasitic capacitors
  std::int64_t rthresh = std::numeric_limits<std::int64_t>::max();  // ohms; max = infinite
  MergeMode merge = MergeMode::none;
  bool extresist = false;
  bool resistor_tee = false;
  bool scale = true;
  ShortMode shorts = ShortMode::none;
  bool subckt_descend = true;  // esDoPorts
  TopSubckt subckt_top = TopSubckt::automatic;
  bool hierarchy = false;
  bool blackbox = false;
  bool renumber = false;
  bool global = true;  // esMergeNames
  /// Run flags that persist across runs, as Magic's globals do: `-B`
  /// (esNoAttrs), `-d` (esDistrJunct), `-J hier|flat` (esHierAP). `-F`
  /// applies to one run only.
  bool no_attributes = false;
  bool distributed_junctions = false;
  bool hier_ap = false;
  /// The `-p` search path and `-T` technology of the last run that gave
  /// them (EFSearchPath, EFArgTech persist across runs); `default` clears
  /// them.
  std::vector<std::string> search_path;
  std::optional<std::string> tech;

  friend bool operator==(const SpiceSettings&, const SpiceSettings&) = default;

  /// The `lvs` preset applied to `base`: hierarchy on, ngspice, cthresh and
  /// rthresh infinite, renumber off, scale off, blackbox on, global off,
  /// subcircuits top auto.
  [[nodiscard]] static SpiceSettings lvs(SpiceSettings base) {
    base.hierarchy = true;
    base.format = Dialect::ngspice;
    base.cthresh = std::numeric_limits<double>::infinity();
    base.rthresh = std::numeric_limits<std::int64_t>::max();
    base.renumber = false;
    base.scale = false;
    base.blackbox = true;
    base.global = false;
    base.subckt_top = TopSubckt::automatic;
    return base;
  }
};

/// esSIvalue: a value with Magic's SI suffix formatting.
[[nodiscard]] std::string format_si(double value);

}  // namespace hefesto::netlist::spice
