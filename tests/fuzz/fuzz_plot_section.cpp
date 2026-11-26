// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the technology `plot` section reader (capability
// plot-output "Technology plot section structure", "PostScript style
// lines", "PNM style lines"; hefesto-testing-strategy "Parser fuzzing";
// m10-plot-route design D20). The input is the body of a `plot` section,
// split into lines and tokens as the technology reader keeps raw sections,
// read against the wiring test technology (tests/data/wiring/wire.tech).
//
// Owner: PLOTTECH.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/plot/tech/plot_tech.hpp"
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
  const auto section = hefesto::fuzz::raw_section("plot", hefesto::fuzz::as_text(data, size), "/fuzz/wire.tech");
  hefesto::platform::MemoryFileSystem fs;
  hefesto::plot::StyleSource styles;
  styles.files = &fs;
  hefesto::diag::NullSink sink;
  (void)hefesto::plot::read_plot_section(&section, *technology(), styles, sink);
  return 0;
}
