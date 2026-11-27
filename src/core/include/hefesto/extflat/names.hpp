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
// Owner: INTEGRATION (this header); EXTFLAT (the implementation).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

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

/// A hierarchical name as its components from the root side, converted
/// with the run's flags (EFHNSprintf, ext2spice.c:4307-4375; EFHNOut,
/// EFvisit.c:878-939). `flags` is read, not changed.
[[nodiscard]] std::string name_to_spice(std::span<const std::string> components, SpiceDialect dialect,
                                        NameFlags& flags, const compat::CompatSettings& compat);
[[nodiscard]] std::string name_to_sim(std::span<const std::string> components, NameFlags& flags,
                                      const compat::CompatSettings& compat);

/// EFHNBest on two hierarchical names written with `/` between their
/// components: true when `a` is preferred to `b` (capability
/// ext-flattening-names "Name preference order"). Both are non-empty.
[[nodiscard]] bool prefer_name(std::string_view a, std::string_view b);

/// No conversion flag at all (EFOutputFlags & EF_TRIM_MASK == 0).
[[nodiscard]] constexpr NameFlags no_name_flags() noexcept {
  return NameFlags{false, false, false, false, false};
}

/// EFArgs' `-t` characters: `!` trim-global, `#` trim-local, `,`
/// convert-comma, `=` convert-equal, `[` or `]` convert-brackets, each
/// added to `flags`.
void add_trim_chars(NameFlags& flags, std::string_view chars) noexcept;

/// The flags of one `ext2spice` run (Q-NETLIST-16; ext2spice.c:1071-1076).
/// With magic-compat ON the session-wide set `session` (Magic's
/// EFOutputFlags, 0 at start-up) gains the run's `-t` characters, then
/// trim-global, convert-comma and convert-equal, then trim-local in HSPICE
/// format, then loses trim-global in CDL format, and is the run's set; so
/// flags of earlier `ext2spice` and `ext2sim` runs stay in effect. With it
/// OFF the run's set is that base (trim-global, convert-comma,
/// convert-equal, plus trim-local in HSPICE, without trim-global in CDL)
/// plus the run's `-t` characters, and `session` is left alone.
[[nodiscard]] NameFlags spice_run_flags(NameFlags& session, SpiceDialect dialect, std::string_view trim_chars,
                                        const compat::CompatSettings& compat) noexcept;
/// The flags of one `ext2sim` run (Q-NETLIST-16): ON, `session` gains the
/// run's `-t` characters and is the run's set; OFF, the run's set is its
/// `-t` characters alone.
[[nodiscard]] NameFlags sim_run_flags(NameFlags& session, std::string_view trim_chars,
                                      const compat::CompatSettings& compat) noexcept;

}  // namespace hefesto::extflat
