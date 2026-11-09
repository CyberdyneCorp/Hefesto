// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the recent-projects list (`recent-projects.json`,
// read at startup and by `pdk uninstall`; capability hefesto-pdk-manager
// "Uninstall and cleanup"; hefesto-testing-strategy "Parser fuzzing"). The
// input is the file's content; a project is then noted, which rewrites it.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/app/recent_projects.hpp"
#include "hefesto/platform/file_system.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace platform = hefesto::platform;
  platform::MemoryFileSystem fs;
  const platform::Path file{"/state/recent-projects.json"};
  if (!fs.write_atomic(file, hefesto::fuzz::as_text(data, size)).ok()) {
    return 0;
  }
  (void)hefesto::app::load_recent_projects(fs, file);
  (void)hefesto::app::note_recent_project(fs, file, platform::Path{"/work/chip"});
  return 0;
}
