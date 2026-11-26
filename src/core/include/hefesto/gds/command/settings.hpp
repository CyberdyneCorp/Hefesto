// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// GdsSettings (capabilities gds-read, gds-write;
// openspec/changes/archive/2026-11-24-m5-mask-gds-cif/design.md D4, D11, D15): every `gds`
// option and its default, held per open project in
// editor::EditorState::mask.gds, as Magic keeps them in session globals
// (calma/CalmaRead.c:68-109, CalmaRdpt.c:54-58, CalmaWrite.c:76-88). The
// session-global Magic state that magic-compat ON reproduces (the
// Q-MASK-111 rewound flag) is an explicit field here, never a static.
// `gds warning` sets the read warnings shared with `cif warning`
// (mask/input/warnings.hpp, EditorState::mask.warnings). The read options
// that `oasis` shares (Q-OASIS-90 (a)) are stream::StreamReadSettings
// (mask/stream_read.hpp, EditorState::mask.stream_read), split out by the
// M6 scaffold (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D15).
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSW owns
// the write options and the command, GDSR the read options (design D2/D4).

#pragma once

#include <cstdint>
#include <optional>

#include "hefesto/mask/stream_read.hpp"

namespace hefesto::gds {

using stream::PolygonSubcells;

struct GdsSettings {
  // ---- read options (GDSR) not shared with `oasis` ----
  double magscale = 1.0;                     // CalmaMagScale (float in Magic)
  bool rewound = false;                      // CalmaRewound (Q-MASK-111, ON only)
  // The shared read options are MaskSession::stream_read. `gds rescale`
  // (CIFRescaleAllow) and cifSeenSnapWarning are shared with `cif`:
  // MaskSession::rescale_allowed and MaskSession::snap_warned.

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
