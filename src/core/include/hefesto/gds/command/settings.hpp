// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GdsSettings (capabilities gds-read, gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D4, D11, D15): every `gds`
// option and its default, held per open project in
// editor::EditorState::mask.gds, as Magic keeps them in session globals
// (calma/CalmaRead.c:68-109, CalmaRdpt.c:54-58, CalmaWrite.c:76-88). The
// session-global Magic state that magic-compat ON reproduces (the
// Q-MASK-111 rewound flag) is an explicit field here, never a static.
// `gds warning` sets the read warnings shared with `cif warning`
// (mask/input/warnings.hpp, EditorState::mask.warnings).
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSW owns
// the write options and the command, GDSR the read options (design D2/D4).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hefesto::gds {

/// `gds polygon subcells` (CalmaSubcellPolygons).
enum class PolygonSubcells : std::uint8_t { none, temporary, keep };

struct GdsSettings {
  // ---- read options (GDSR) ----
  bool flatten = false;                      // CalmaFlattenUses
  std::int32_t flatten_limit = 10;           // CalmaFlattenLimit
  std::vector<std::string> flatglob{"*_CDNS_*"};  // CalmaFlattenUsesByName (CalmaTechInit's default)
  bool readonly = false;                     // CalmaReadOnly
  double magscale = 1.0;                     // CalmaMagScale (float in Magic)
  bool drccheck = true;                      // !CalmaNoDRCCheck
  bool ordering = false;                     // CalmaPostOrder
  bool noduplicates = false;                 // CalmaNoDuplicates
  bool unique = false;                       // CalmaUnique
  bool savepaths = false;                    // CalmaRecordPaths
  std::string maskhints;                     // CalmaMaskHints: "" none, "yes" all, else CIF layers a,b,...
  PolygonSubcells polygon_subcells = PolygonSubcells::none;  // CalmaSubcellPolygons
  bool path_subcells = false;                // CalmaSubcellPaths
  bool rescale = true;                       // CIFRescaleAllow (Q-MASK-130)
  bool rewound = false;                      // CalmaRewound (Q-MASK-111, ON only)
  bool snap_warned = false;                  // cifSeenSnapWarning: the grid message is given once per
                                             // session (a CIF read resets it)

  // ---- write options (GDSW) ----
  bool library = false;                      // CalmaDoLibrary
  bool labels = true;                        // CalmaDoLabels
  bool lower = true;                         // CalmaDoLower
  bool arrays = false;                       // CalmaFlattenArrays
  bool addendum = false;                     // CalmaAddendum
  std::optional<std::int64_t> datestamp;     // CalmaDateStamp (nullopt = the cell's timestamp;
                                             // `gds nodatestamp yes` sets 0)
  bool allow_undefined = false;              // CalmaAllowUndefined
  bool allow_abstract = false;               // CalmaAllowAbstract
  bool contacts = false;                     // CalmaContactArrays (accepted, no effect)
  bool merge = false;                        // CalmaMergeTiles
  std::int32_t compress = 0;                 // CalmaCompression

  friend bool operator==(const GdsSettings&, const GdsSettings&) = default;
};

}  // namespace hefesto::gds
