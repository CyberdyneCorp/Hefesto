// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared by the M7 `.ext` fuzz targets (fuzz_ext_netlist,
// fuzz_res_ext_reader; m7-extraction design D22): a fuzz input split into
// several files of one in-memory directory. The text before the first
// line `@@ <name>` is the first file, each `@@ <name>` line starts the
// file `<name>`. A target names the first file and the extension it adds.
//
// Owner: INTEGRATION (tests/fuzz/, design D2).

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::fuzz {

/// The largest input the M7 `.ext` targets read (a harness cap, design
/// D22: larger inputs only make each run slower, not deeper).
inline constexpr std::size_t kMaxExtInput = std::size_t{64} << 10;

/// Stores the files of `text` under `dir` (see the file comment): the
/// first is `<first><extension>`, each `@@ <name>` line starts
/// `<name><extension>`.
inline void store_marked_files(platform::FileSystem& fs, std::string_view text, const std::string& dir,
                               std::string first, std::string_view extension) {
  constexpr std::string_view kMarker = "@@ ";
  std::string name = std::move(first);
  std::string body;
  auto flush = [&] { (void)fs.write_atomic(dir + "/" + name + std::string{extension}, body); };
  std::size_t at = 0;
  while (at <= text.size()) {
    const std::size_t end = std::min(text.find('\n', at), text.size());
    const std::string_view line = text.substr(at, end - at);
    if (line.starts_with(kMarker)) {
      flush();
      name = std::string{trimmed_line(line.substr(kMarker.size()))};
      body.clear();
    } else {
      body.append(line).append("\n");
    }
    at = end + 1;
  }
  flush();
}

}  // namespace hefesto::fuzz
