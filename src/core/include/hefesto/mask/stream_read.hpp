// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// StreamReadSettings (capabilities gds-read, oasis-command "Settings shared
// with the gds command"; openspec/changes/m6-lef-def-oasis/design.md D4,
// D15, Q-OASIS-90 (a)): the read settings that `gds` and `oasis` share, held
// per open project in editor::EditorState::mask.stream_read. Magic keeps
// them in the Calma session globals (calma/CalmaRead.c:68-109,
// CalmaRdpt.c:54-58); its OASIS reader is absent, and Q-OASIS-90 (a) makes
// `oasis read` follow the same values. The other two shared read settings
// already live in MaskSession: `rescale` (rescale_allowed) and `warning`
// (warnings).
//
// Owner: INTEGRATION (this header, split out of gds::GdsSettings by the M6
// scaffold); GDSR and OASR read it, the `gds` and `oasis` commands set it.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hefesto::stream {

/// `gds polygon subcells` (CalmaSubcellPolygons).
enum class PolygonSubcells : std::uint8_t { none, temporary, keep };

struct StreamReadSettings {
  bool flatten = false;                           // CalmaFlattenUses
  std::int32_t flatten_limit = 10;                // CalmaFlattenLimit
  std::vector<std::string> flatglob{"*_CDNS_*"};  // CalmaFlattenUsesByName (CalmaTechInit's default)
  bool readonly = false;                          // CalmaReadOnly
  bool drccheck = true;                           // !CalmaNoDRCCheck
  bool ordering = false;                          // CalmaPostOrder
  bool noduplicates = false;                      // CalmaNoDuplicates
  bool unique = false;                            // CalmaUnique
  bool savepaths = false;                         // CalmaRecordPaths
  std::string maskhints;                          // CalmaMaskHints: "" none, "yes" all, else CIF layers a,b,...
  PolygonSubcells polygon_subcells = PolygonSubcells::none;  // CalmaSubcellPolygons
  bool path_subcells = false;                     // CalmaSubcellPaths

  friend bool operator==(const StreamReadSettings&, const StreamReadSettings&) = default;
};

}  // namespace hefesto::stream
