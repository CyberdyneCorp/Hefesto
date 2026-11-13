// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MaskSession: the M5 session state of one open project,
// editor::EditorState::mask (openspec/changes/m5-mask-gds-cif/design.md D4).
// INTEGRATION owns the struct; each member belongs to the named workstream.
//
// Owner: INTEGRATION.

#pragma once

#include "hefesto/cif/command/settings.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/input/style.hpp"
#include "hefesto/mask/input/warnings.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/util/libc_random.hpp"

namespace hefesto::mask {

struct MaskSession {
  // STYLES: the parsed cifoutput styles (one immutable object per style,
  // swapped on reload, selection or rescale) and the current one.
  OutputStyleSet output_styles;
  // INPUT: the parsed cifinput styles and the current one.
  InputStyleSet input_styles;
  // GDSW (read options GDSR): every `gds` option, the session date stamp
  // and the Q-MASK-111 rewound flag.
  gds::GdsSettings gds;
  // CIF: the `cif` options.
  cif::CifSettings cif;
  // INPUT / GDSR / CIF: the read warning level and redirect, which `cif
  // warning` and `gds warning` share (Magic's CIFWarningLevel).
  WarningSettings warnings;
  // INTEGRATION: the shared glibc TYPE_3 generator of `random` / `random
  // seed`, which the Q-MASK-211 full-dump prefix reads under magic-compat
  // ON (moved from db/commands_misc.cpp).
  util::LibcRandom random;
};

}  // namespace hefesto::mask
