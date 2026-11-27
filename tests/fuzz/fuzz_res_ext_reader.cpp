// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for extresist's `.ext` input model and the `.res.ext`
// overlay read (task 19.2, m7-extraction design D22; capabilities
// extresist-command "Input model", extresist-output "Consumption by
// netlisters", ext-flattening-read "Resistance-extraction overlay";
// hefesto-testing-strategy "Parser fuzzing").
//
// The input is a set of files in /w, as fuzz_ext_reader's: the text before
// the first line `@@ <name>` is `top.ext`, each `@@ <name>` line starts
// `<name>.ext` (so `@@ top.res` starts the overlay `top.res.ext` and
// `@@ parent` a parent cell's `.ext`). The first byte is a mode:
//   bit 0  magic-compat OFF
//   bit 1  `extresist box` (no signal: couplings and `res:drive` ignored)
//   bit 2  the parent `parent.ext` is read for `connect` records
//   bit 3  debug lines
// Every input runs extresist::read_input on `top` with the synthetic
// extraction style of tests/fuzz/data/exttest_fuzz.tech, then the
// `.res.lump` writer over its nets, then the netlisters' read of `top`
// with the `.res.ext` overlay (extflat::ReadOptions::resist_overlay).
//
// Resource caps (harness, design D22; recorded as approved under the
// standing rule of 2026-11-25): inputs above 64 KiB are ignored; the
// overlay read runs with the fuzz_ext_netlist ReadLimits (4096 instances,
// 65536 names and merge elements, 64 cells, depth 16).
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ext_fuzz_files.hpp"
#include "exttest_fuzz.hpp"
#include "fuzz_common.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extresist/output/output.hpp"
#include "hefesto/platform/file_system.hpp"

namespace {

namespace extresist = hefesto::extresist;
namespace extflat = hefesto::extflat;

constexpr std::uint8_t kCompatOff = 0x01;
constexpr std::uint8_t kBox = 0x02;
constexpr std::uint8_t kParents = 0x04;
constexpr std::uint8_t kDebug = 0x08;

void read_model(std::uint8_t mode, hefesto::platform::MemoryFileSystem& fs) {
  const bool compat = (mode & kCompatOff) == 0;
  const hefesto::fuzz::ExtTestTech& tech = hefesto::fuzz::exttest(compat);
  if (tech.style == nullptr) {
    return;
  }
  extresist::InputOptions options;
  options.tech = tech.technology.get();
  options.style = tech.style.get();
  options.signal = (mode & kBox) == 0;
  options.debug = (mode & kDebug) != 0;
  options.magic_compat = compat;
  options.load = [&fs](std::string_view cell) -> std::optional<std::string> {
    auto text = fs.read("/w/" + std::string{cell} + ".ext");
    return text.ok() ? std::optional<std::string>{text.value()} : std::nullopt;
  };
  if ((mode & kParents) != 0) {
    options.parents = {"parent"};
  }
  options.emit = [](const hefesto::diag::Diagnostic&) {};
  const auto model = extresist::read_input("top", options);
  if (model.ok()) {
    (void)model.value().net_order();
    (void)extresist::write_res_lump(model.value(), {});
  }
}

void read_overlay(std::uint8_t mode, hefesto::platform::MemoryFileSystem& fs) {
  const hefesto::cell::SearchPaths paths{{"/w"}, {}, {}};
  const hefesto::cell::PathVariables vars;
  hefesto::diag::NullSink sink;
  hefesto::cell::LookupContext ctx{fs, paths, vars, sink};
  extflat::ReadOptions options;
  options.magic_compat = (mode & kCompatOff) == 0;
  options.search_path = {"/w"};
  options.resist_overlay = true;
  options.limits.max_depth = 16;
  options.limits.max_cells = 64;
  options.limits.max_instances = 4096;
  options.limits.max_flat_names = 65536;
  options.limits.max_connections = 65536;
  (void)extflat::read_flat(ctx, "top", options);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > hefesto::fuzz::kMaxExtInput) {
    return 0;
  }
  const auto [mode, rest] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::platform::MemoryFileSystem fs;
  hefesto::fuzz::store_marked_files(fs, rest, "/w", "top", ".ext");
  read_model(mode, fs);
  read_overlay(mode, fs);
  return 0;
}
