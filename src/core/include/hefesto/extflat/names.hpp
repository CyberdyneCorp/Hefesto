// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE and sim name conversion (capabilities ext-flattening-names,
// netlist-spice-options "Node names per dialect", netlist-sim "Name
// conversion state"; openspec/changes/m7-extraction/design.md D13).
// Q-NETLIST-30 per mode, Q-NETLIST-16 per mode (one session-wide NameFlags
// with magic-compat ON, a fresh one per run with it OFF; the caller keeps
// it in EditorState::netlist.names) and Q-NETLIST-10.
//
// Scaffold state (tasks.md 3.3): both conversions join the components with
// `/` unchanged until EXTFLAT lands them.
//
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "hefesto/compat/compat_settings.hpp"

namespace hefesto::extflat {

/// The SPICE dialects (ext2spice `format`), in Magic's order (SPICE2,
/// SPICE3, HSPICE, NGSPICE, CDL). netlist::spice::Dialect is this type.
enum class SpiceDialect : std::uint8_t { spice2, spice3, hspice, ngspice, cdl };

/// EFOutputFlags' name-conversion bits.
struct NameFlags {
  bool convert_comma = true;   // EF_CONVERTCOMMA
  bool convert_equal = true;   // EF_CONVERTEQUAL
  bool convert_brackets = false;  // EF_CONVERTBRACKETS
  bool trim_global = false;    // EF_TRIMGLOB (`!`)
  bool trim_local = false;     // EF_TRIMLOCAL (`#`)

  friend bool operator==(const NameFlags&, const NameFlags&) = default;
};

/// A hierarchical name as its components from the root side.
[[nodiscard]] std::string name_to_spice(std::span<const std::string> components, SpiceDialect dialect,
                                        NameFlags& flags, const compat::CompatSettings& compat);
[[nodiscard]] std::string name_to_sim(std::span<const std::string> components, NameFlags& flags,
                                      const compat::CompatSettings& compat);

}  // namespace hefesto::extflat
