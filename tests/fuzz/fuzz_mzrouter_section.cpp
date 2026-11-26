// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `mzrouter` section reader
// (capability router-maze "mzrouter section grammar";
// hefesto-testing-strategy "Parser fuzzing"; m10-plot-route design D20).
// The input is the body of an `mzrouter` section, split into lines and
// tokens as the technology reader keeps raw sections, read against the
// wiring test technology (tests/data/wiring/wire.tech).
//
// Scaffold version (m10-plot-route tasks.md 3.1); MZTECH (task 9.3) fills
// it in.
//
// Owner: MZTECH.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/tech/loader.hpp"
#include "section_lines.hpp"

namespace {

std::shared_ptr<const hefesto::tech::Technology> technology() {
  static const auto tech = [] {
    hefesto::platform::MemoryFileSystem fs;
    hefesto::diag::NullSink sink;
    auto loaded = hefesto::tech::load_technology_text(hefesto::fuzz::source_file("tests/data/wiring/wire.tech"),
                                                      "/fuzz/wire.tech", fs, hefesto::tech::LoadOptions{}, sink);
    return loaded.ok() ? loaded.value() : hefesto::tech::builtin_technology();
  }();
  return tech;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto section = hefesto::fuzz::raw_section("mzrouter", hefesto::fuzz::as_text(data, size), "/fuzz/wire.tech");
  hefesto::diag::NullSink sink;
  const hefesto::route::MazeStyles styles = hefesto::route::read_mzrouter_section(&section, *technology(), sink);
  (void)hefesto::route::find_style(styles, "irouter");
  return 0;
}
