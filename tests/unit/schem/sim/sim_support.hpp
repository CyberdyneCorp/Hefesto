// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Helpers of the schem/sim tests: copies of system-library symbols next to a
// test schematic (relative references resolve against the document's
// folder).
//
// Owner: SIMINT.

#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>

namespace hefesto::test::sim {

inline const std::filesystem::path kDevices =
    std::filesystem::path{HEFESTO_SOURCE_DIR} / "assets/schematic/xschem_library/devices";

inline const std::filesystem::path kGolden = std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/SCHEM/cases";

/// Copies the system-library symbols `names` into `folder`.
inline void copy_devices(const std::filesystem::path& folder, std::initializer_list<const char*> names) {
  std::filesystem::create_directories(folder);
  for (const char* name : names) {
    std::filesystem::copy_file(kDevices / name, folder / name, std::filesystem::copy_options::overwrite_existing);
  }
}

}  // namespace hefesto::test::sim
