// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for raw result files (capability
// simulation-analyses-and-results "Raw file import", "Raw file format";
// openspec/changes/m9-lvs-simulation design D12, D18, D20). The input is
// parsed as a raw file with a small result budget (1 MiB, so a header that
// declares huge plots is refused before anything is allocated); every plot
// that parses is written again as binary and ASCII raw and must parse back
// to the same vectors, then exported as CSV and measured.
//
// Owner: SIMRES; tests/fuzz/ is shared (INTEGRATION).

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "hefesto/sim/result/raw.hpp"

namespace {

namespace sim = hefesto::sim;

constexpr std::uint64_t kBudget = std::uint64_t{1} << 20;

bool same_bits(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (__builtin_memcmp(&a[i], &b[i], sizeof(double)) != 0) {
      return false;
    }
  }
  return true;
}

void round_trip(const sim::Plot& plot) {
  sim::RawWriteOptions options;
  options.date = "Thu Nov  5 14:30:00 2026";
  const auto binary = sim::render_raw({plot}, options);
  if (!binary.ok()) {
    return;  // vectors of different lengths cannot be written
  }
  const auto again = sim::parse_raw(binary.value(), "again.raw", kBudget * 2);
  if (!again.ok() || again.value().size() != 1 || again.value()[0].vectors.size() != plot.vectors.size()) {
    __builtin_trap();  // a written plot must read back
  }
  for (std::size_t i = 0; i < plot.vectors.size(); ++i) {
    if (!same_bits(again.value()[0].vectors[i].real, plot.vectors[i].real)) {
      __builtin_trap();  // binary data is not bit-exact
    }
  }
  options.binary = false;
  (void)sim::render_raw({plot}, options);
  (void)sim::render_csv(plot);
  (void)sim::measure(plot, sim::WaveformMeasure{"cross", plot.vectors.back().name, {0.5, 1, 0}, {}, {}});
  (void)sim::measure(plot, sim::WaveformMeasure{"rms", plot.vectors.back().name, {}, {}, {}});
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view input{reinterpret_cast<const char*>(data), size};  // NOLINT
  const auto plots = sim::parse_raw(input, "fuzz.raw", kBudget);
  if (!plots.ok()) {
    return 0;
  }
  for (const sim::Plot& plot : plots.value()) {
    round_trip(plot);
  }
  return 0;
}
