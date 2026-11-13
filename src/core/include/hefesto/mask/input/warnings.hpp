// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The read warning settings (CIFWarningLevel and its redirect file), which
// `cif warning` and `gds warning` both set: Magic keeps one global for both
// readers (cif/CIFrdutils.c, commands/CmdCD.c), so Hefesto keeps one value
// per open project, editor::EditorState::mask.warnings
// (openspec/changes/m5-mask-gds-cif/design.md D4 "As-landed scaffold
// contracts"). Q-MASK-331 (the CIF error limit and redirect) and Q-MASK-130
// (`gds rescale no` sets `limit` under ON) act on it.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); INPUT, GDSR
// and CIF report through it (design D2).

#pragma once

#include <cstdint>
#include <string>

namespace hefesto::mask {

/// `cif warning` / `gds warning` levels.
enum class WarningLevel : std::uint8_t { warn_default, none, align, limit, redirect };

struct WarningSettings {
  WarningLevel level = WarningLevel::warn_default;
  std::string redirect_file;  // `... warning redirect <file>`

  friend bool operator==(const WarningSettings&, const WarningSettings&) = default;
};

}  // namespace hefesto::mask
