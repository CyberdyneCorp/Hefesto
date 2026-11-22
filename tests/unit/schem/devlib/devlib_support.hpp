// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the system-library tests: where the library and its
// interface table live, and small file helpers.
//
// Owner: DEVLIB.

#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "hefesto/platform/file_system.hpp"

namespace hefesto::test::devlib {

using platform::Path;

inline const Path kSource{HEFESTO_SOURCE_DIR};
/// XSCHEM_SHAREDIR as CMD sets it: <CAD_ROOT>/assets/schematic, with the
/// source tree as CAD_ROOT.
inline const Path kSharedir = kSource / "assets" / "schematic";
/// Where the PDK xschemrc files look for the system library
/// (`${XSCHEM_SHAREDIR}/xschem_library/devices`).
inline const Path kLibrary = kSharedir / "xschem_library" / "devices";
inline const Path kData{HEFESTO_TEST_DATA_DIR};

inline std::string read_text(const Path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

inline nlohmann::json read_json(const Path& path) { return nlohmann::json::parse(read_text(path)); }

/// The "symbols" object of tests/data/schem/devices_interface.json.
inline const nlohmann::json& interface_table() {
  static const nlohmann::json table = read_json(kSource / "tests/data/schem/devices_interface.json").at("symbols");
  return table;
}

}  // namespace hefesto::test::devlib
