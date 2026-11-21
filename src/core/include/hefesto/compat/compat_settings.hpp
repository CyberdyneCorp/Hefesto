// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reference-tool compatibility settings (capability hefesto-magic-compat-mode,
// requirements "magic-compat setting for value-changing quirks",
// "Reference-tool compatibility settings", "netgen-compat setting for netgen
// quirks", "xschem-compat setting for xschem quirks"). Each setting is
// independent of the others.

#pragma once

#include <string>

namespace hefesto::compat {

struct CompatSettings {
  bool magic_compat = true;
  bool netgen_compat = true;
  /// Governs the `Resolution: compat` entries of SCHEM.md (D-21). Schematic
  /// code receives it explicitly; no layout, extraction or LVS code reads it.
  bool xschem_compat = true;

  friend bool operator==(const CompatSettings&, const CompatSettings&) = default;
};

/// The tag that every netlist and `.ext` writer puts in its header comment:
/// "magic-compat=on" or "magic-compat=off". The writer adds its own comment
/// leader (e.g. "* " for SPICE).
[[nodiscard]] std::string magic_compat_header_tag(const CompatSettings& settings);

/// "netgen-compat=on" or "netgen-compat=off", for LVS reports. netgen-compat
/// never appears in layout, extraction or netlist output.
[[nodiscard]] std::string netgen_compat_header_tag(const CompatSettings& settings);

/// The line a schematic SPICE netlist records `xschem-compat` with:
/// "** xschem-compat=off" when it is OFF, and the empty string (no line)
/// when it is ON. The netlist writer inserts it directly after the first
/// `** sch_path:` line. Schematic netlists never record magic-compat.
[[nodiscard]] std::string xschem_compat_netlist_line(const CompatSettings& settings);

}  // namespace hefesto::compat
