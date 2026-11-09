// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the .mag reader (capability cell-file-format;
// hefesto-testing-strategy "Parser fuzzing"). Each input is read into a new
// cell of a session on the tiny test technology (the technology check is
// skipped so every input reaches the body), then written back, so the
// writer also sees whatever the reader accepted.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "fuzz_common.hpp"
#include "hefesto/cell/host.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cell/timestamps.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"

namespace {

std::shared_ptr<const hefesto::tech::Technology> tiny() {
  static const auto kTech = [] {
    hefesto::platform::MemoryFileSystem fs;
    hefesto::diag::NullSink sink;
    auto loaded = hefesto::tech::load_technology_text(hefesto::fuzz::source_file("tests/data/tech/tiny.tech"),
                                                      "tiny.tech", fs, hefesto::tech::LoadOptions{}, sink);
    return loaded.ok() ? loaded.value() : hefesto::tech::builtin_technology();
  }();
  return kTech;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace cell = hefesto::cell;
  const auto tech = tiny();
  hefesto::platform::MemoryFileSystem fs;
  hefesto::platform::FixedClock clock{std::chrono::system_clock::time_point{std::chrono::seconds{1000}}};
  hefesto::db::CellLibrary library;
  hefesto::tech::GridScale grid;
  cell::SearchPaths paths;
  cell::PathVariables vars;
  hefesto::diag::NullSink sink;
  const auto host = cell::make_memory_file_host(fs, "/work");
  cell::TimestampLog stamps;
  cell::CellIoContext ctx{fs, clock, library, tech.get(), grid, paths, vars, sink, *host, stamps,
                          nullptr, nullptr, true, true, nullptr, {}, {}};
  ctx.rescale_database = [](std::int64_t, std::int64_t) { return true; };
  auto created = library.create("fuzz", tech->plane_count());
  if (!created.ok()) {
    return 0;
  }
  cell::ReadOptions options;
  options.ignore_tech = true;
  (void)cell::read_mag(ctx, *created.value(), hefesto::fuzz::as_text(data, size), options);
  (void)cell::write_mag(ctx, *created.value());
  return 0;
}
