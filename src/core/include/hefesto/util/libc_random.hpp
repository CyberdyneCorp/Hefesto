// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// glibc's random() (TYPE_3: the additive-feedback generator of degree 31 and
// separation 3), which Magic calls, so a seed gives the numbers Magic gives
// on Linux on every host (CMDEDIT-MISC-05). One generator per open project
// (editor::EditorState::mask.random), shared by the `random` command and,
// with magic-compat ON, the GDS full-dump prefix (Q-MASK-211;
// openspec/changes/m5-mask-gds-cif/design.md D4, D15). Moved from
// db/commands_misc.cpp by the M5 scaffold.
//
// Owner: INTEGRATION.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hefesto::util {

class LibcRandom {
 public:
  /// Seeded with 1, as a C program that never calls srandom().
  LibcRandom() { seed(1); }

  /// srandom(): only the low 32 bits of the seed matter; 0 acts as 1.
  void seed(std::uint32_t value);

  /// random(): a value in [0, 2^31).
  [[nodiscard]] std::int64_t next();

 private:
  static constexpr std::size_t kDeg = 31;
  static constexpr std::size_t kSep = 3;
  std::array<std::uint32_t, kDeg> state_{};
  std::size_t front_ = kSep;
  std::size_t rear_ = 0;
};

}  // namespace hefesto::util
