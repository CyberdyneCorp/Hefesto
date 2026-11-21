// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the `.ext` netlist read and flatten subset (EXTSUB:
// ext-flattening-read, ext-flattening-hierarchy, ext-flattening-names;
// Open Question 1 (a); hefesto-testing-strategy "Parser fuzzing";
// m6-lef-def-oasis design D21). The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  timestamps are checked (the `timestamp` records)
//   bit 2  small limits (depth 8, 4096 instances, 65536 names and merge
//          elements, 64 cells), so the cap paths run
//   bit 3  global nets `vdd!` and `gnd!`
//   bit 4  units per lambda 0.5 and a style unit table
// The rest is a set of `.ext` files in an in-memory directory /w: the text
// before the first line `@@ <cell>` is `top.ext`, each `@@ <cell>` line
// starts `<cell>.ext`. `top` is read and flattened.
//
// Resource caps: extflat::ReadLimits (file 1 GiB, logical line 16 MiB,
// 1 Mi cells, depth 1000, 4 Mi instances, 16 Mi flat names, 16 Mi merge
// elements; use cycles fail; M6-S8). Reaching 4 Mi instances takes seconds,
// so this target lowers the instance, name and merge caps to 2^18, 2^20
// and 2^20 (and further with bit 2): a time-out is then a real hang.
//
// Owner: INTEGRATION (tests/fuzz/, design D2), written by the FUZZSEC
// workstream.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/platform/file_system.hpp"

namespace {

namespace extflat = hefesto::extflat;

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kTimestamps = 0x02;
constexpr std::uint8_t kSmallLimits = 0x04;
constexpr std::uint8_t kGlobals = 0x08;
constexpr std::uint8_t kUnits = 0x10;
constexpr std::string_view kMarker = "@@ ";

/// Stores the files of `text` under /w (see the file comment).
void store_files(hefesto::platform::MemoryFileSystem& fs, std::string_view text) {
  std::string cell = "top";
  std::string body;
  auto flush = [&] { (void)fs.write_atomic("/w/" + cell + ".ext", body); };
  std::size_t at = 0;
  while (at <= text.size()) {
    const std::size_t end = std::min(text.find('\n', at), text.size());
    const std::string_view line = text.substr(at, end - at);
    if (line.starts_with(kMarker)) {
      flush();
      cell = std::string{hefesto::fuzz::trimmed_line(line.substr(kMarker.size()))};
      body.clear();
    } else {
      body.append(line).append("\n");
    }
    at = end + 1;
  }
  flush();
}

extflat::ReadOptions options_for(std::uint8_t mode) {
  extflat::ReadOptions options;
  options.magic_compat = (mode & kCompatOff) == 0;
  options.timestamps = (mode & kTimestamps) != 0;
  options.search_path = {"/w"};
  options.tech_name = "lefdef";
  options.limits.max_instances = std::uint64_t{1} << 18U;
  options.limits.max_flat_names = std::uint64_t{1} << 20U;
  options.limits.max_connections = std::uint64_t{1} << 20U;
  if ((mode & kSmallLimits) != 0) {
    options.limits.max_depth = 8;
    options.limits.max_instances = 4096;
    options.limits.max_flat_names = 65536;
    options.limits.max_connections = 65536;
    options.limits.max_cells = 64;
  }
  if ((mode & kGlobals) != 0) {
    options.global_nets = {"vdd!", "gnd!"};
  }
  if ((mode & kUnits) != 0) {
    options.units_per_lambda = 0.5F;
    options.style_units = [](std::string_view style) -> std::optional<float> {
      return style == "lefdef" ? std::optional<float>{1.0F} : std::nullopt;
    };
  }
  return options;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::platform::MemoryFileSystem fs;
  store_files(fs, rest);
  const hefesto::cell::SearchPaths paths{{"/w"}, {}, {}};
  const hefesto::cell::PathVariables vars;
  hefesto::diag::NullSink sink;
  hefesto::cell::LookupContext ctx{fs, paths, vars, sink};
  auto netlist = extflat::read_flat(ctx, "top", options_for(mode));
  if (netlist.ok()) {
    std::size_t names = 0;
    netlist.value().visit_nodes([&](const extflat::FlatNode& node) { names += node.names.size(); });
    (void)names;
  }
  return 0;
}
