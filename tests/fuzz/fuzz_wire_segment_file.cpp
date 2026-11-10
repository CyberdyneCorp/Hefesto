// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the `wire segment` coordinate file (capability
// wiring-tool "wire segment"; hefesto-testing-strategy "Parser fuzzing").
// The first byte picks the default unit, the wire width and the end caps;
// the rest is the file. The points read are then turned into the path
// polygons, as `wire segment` does before painting.
//
// Written by WIRE; tests/fuzz/ is shared (INTEGRATION, design.md D2).

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/wiring/wiring.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace wiring = hefesto::wiring;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::cmd::CoordinateUnits units;
  units.default_unit = (mode & 1U) != 0 ? hefesto::cmd::DefaultUnit::internal : hefesto::cmd::DefaultUnit::lambda;
  units.internal_per = 1 + ((mode >> 1U) & 1U);
  units.cif_scale = std::array<std::int64_t, 2>{100, 1};
  hefesto::diag::NullSink sink;
  auto points = wiring::read_segment_points(text, "fuzz", units, &sink);
  if (!points.ok() || points.value().empty()) {
    return 0;
  }
  std::vector<hefesto::db::Point> path = points.value();
  std::reverse(path.begin(), path.end());
  const auto width = static_cast<hefesto::db::Coord>(1 + ((mode >> 2U) & 15U));
  (void)wiring::wire_path_polygons(path, width, (mode & 0x40U) != 0, &sink);
  return 0;
}
