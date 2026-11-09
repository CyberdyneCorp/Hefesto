// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the hefesto.project reader (capability
// hefesto-settings; hefesto-testing-strategy "Parser fuzzing"). An accepted
// project is written with the deterministic writer; that text must read
// again and write to the same bytes.

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "fuzz_common.hpp"
#include "hefesto/app/project_file.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace app = hefesto::app;
  const auto project = app::parse_project(hefesto::fuzz::as_text(data, size), "fuzz");
  if (!project.ok()) {
    return 0;
  }
  const std::string text = app::to_text(project.value());
  const auto again = app::parse_project(text, "fuzz");
  if (!again.ok() || app::to_text(again.value()) != text) {
    std::abort();
  }
  return 0;
}
