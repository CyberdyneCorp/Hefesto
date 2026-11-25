// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MaskSession: the M5 session state of one open project,
// editor::EditorState::mask (openspec/changes/archive/2026-11-24-m5-mask-gds-cif/design.md D4).
// INTEGRATION owns the struct; each member belongs to the named workstream.
// The M6 scaffold split the read settings that `gds` and `oasis` share out
// of `gds` into `stream_read` (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
// D4, D15).
//
// Owner: INTEGRATION.

#pragma once

#include <string>
#include <string_view>

#include "hefesto/cif/command/settings.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/mask/stream_read.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/oasis/command/settings.hpp"
#include "hefesto/util/libc_random.hpp"

namespace hefesto::mask {

/// The setting that pins the zone of GDS and CIF dates (Q-MASK-132), read
/// into MaskSession::time_zone at startup, e.g. `--setting gds-time-zone=UTC0`.
inline constexpr std::string_view kTimeZoneSetting = "gds-time-zone";

struct MaskSession {
  // STYLES: the parsed cifoutput styles (one immutable object per style,
  // swapped on reload, selection or rescale) and the current one.
  OutputStyleSet output_styles;
  // INPUT: the parsed cifinput styles and the current one.
  InputStyleSet input_styles;
  // INTEGRATION (D15; read by GDSR and OASR): the read settings that `gds`
  // and `oasis` share (Q-OASIS-90 (a)).
  stream::StreamReadSettings stream_read;
  // GDSW (read options GDSR): every other `gds` option, the session date
  // stamp and the Q-MASK-111 rewound flag.
  gds::GdsSettings gds;
  // OASCMD (write side OASW, read side OASR): the `oasis` options stored
  // apart from the `gds` ones (M6 design D4, D15).
  oasis::OasisSettings oasis;
  // CIF: the `cif` options.
  cif::CifSettings cif;
  // INPUT / GDSR / CIF: the read warning level and redirect, which `cif
  // warning` and `gds warning` share (Magic's CIFWarningLevel).
  WarningSettings warnings;
  // INPUT / GDSR / CIF: Magic's CIFRescaleAllow, which `gds rescale` and
  // `cif rescale` share (both readers may subdivide the internal grid only
  // while it is set), and cifSeenSnapWarning: the read grid message is given
  // once until a CIF read resets it.
  bool rescale_allowed = true;
  bool snap_warned = false;
  // INTEGRATION: the shared glibc TYPE_3 generator of `random` / `random
  // seed`, which the Q-MASK-211 full-dump prefix reads under magic-compat
  // ON (moved from db/commands_misc.cpp).
  util::LibcRandom random;
  // INTEGRATION (Q-MASK-132): the time-zone override for GDS dates and the
  // CIF header date, a POSIX TZ string ("UTC0", "EST5EDT"); empty = the
  // host local zone. Set at startup from the `gds-time-zone` setting
  // (kTimeZoneSetting); passed to GdsWriteOptions, GdsReadOptions and
  // CifWriteOptions by the `gds` and `cif` commands.
  std::string time_zone;
  // INTEGRATION: the `cif statistics` counters (Magic's CIFTileOps and
  // totals), fed on the command thread after each generation.
  StatisticsCounters statistics;
  // SHELL (D16): the desktop's hosts; empty = inline, no progress (CLI, tests).
  // read_host runs a read, write_host a whole write (its prepare and finish
  // phases), on the command thread with frames pumped; emit_host runs a
  // write's emit phase on a worker while the command thread pumps.
  ProgressHost read_host;
  ProgressHost write_host;
  ProgressHost emit_host;
};

}  // namespace hefesto::mask
