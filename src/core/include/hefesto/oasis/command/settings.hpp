// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OasisSettings (capability oasis-command; openspec/changes/archive/2026-11-22-m6-lef-def-oasis
// design.md D4, D15): the `oasis` options that are stored apart from the
// `gds` ones (oasis-command "Settings shared with the gds command"), with
// the defaults of "Options shared with the gds command" and "OASIS-only
// options". Held per open project in editor::EditorState::mask.oasis. The
// 13 read settings shared with `gds` live in mask.stream_read (Q-OASIS-90
// (a); `rescale` and `warning` in MaskSession::rescale_allowed and
// MaskSession::warnings), never here.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASCMD owns
// the command, OASW the write fields and OASR the read fields (design D2/D4).

#pragma once

#include <cstdint>
#include <optional>

#include "hefesto/oasis/codec/validate.hpp"

namespace hefesto::oasis {

struct OasisSettings {
  // ---- write options (OASW), same syntax as `gds`, separate values ----
  bool allow_abstract = false;   // abstract allow|disallow
  bool addendum = false;         // addendum
  bool arrays = false;           // arrays
  std::int32_t compress = 6;     // compress 0-9: the CBLOCK DEFLATE level (gds: 0)
  bool contacts = false;         // contacts (accepted, no effect)
  bool labels = true;            // labels
  bool library = false;          // library
  bool lower = true;             // lower
  bool merge = false;            // merge
  bool allow_undefined = false;  // undefined allow|disallow
  // ---- read side (OASR), separate from `gds` ----
  /// `datestamp`/`nodatestamp`: nullopt = `yes` (the file's or the default
  /// timestamp); `nodatestamp yes` sets 0.
  std::optional<std::int64_t> datestamp;
  double magscale = 1.0;  // magscale: accepted, no effect on OASIS
  // ---- OASIS-only options (oasis-command "OASIS-only options") ----
  bool strict = true;                                     // strict: name tables (OASW)
  ValidationScheme validation = ValidationScheme::crc32;  // validation none|crc32|checksum (OASW)
  bool repetitions = true;                                // repetitions: detection on write (OASW)
  bool layernames = true;                                 // layernames: LAYERNAME records (OASW)
  std::int32_t circlepoints = 64;                         // circlepoints 8-4096: CIRCLE vertices on read (OASR)

  friend bool operator==(const OasisSettings&, const OasisSettings&) = default;
};

}  // namespace hefesto::oasis
