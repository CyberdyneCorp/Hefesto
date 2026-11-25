// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Loading the hefesto-ngspice-adapter plugin (openspec/changes/
// m9-lvs-simulation design D2): dlopen of the plugin found next to the
// executable through the install layout (<bin>/../lib, then <bin>), and its
// one entry point, hefesto_engine_v1. Used by hefesto-simd and by the
// developer-build in-process host; nothing else loads the plugin.
//
// Owner: SIMD.

#pragma once

#include <string>
#include <vector>

#include "abi/hefesto_engine_v1.h"

namespace hefesto::simd {

/// The plugin's file name on this platform.
[[nodiscard]] std::string adapter_file_name();

/// Where the plugin is looked for, given the directory of the executable:
/// `<dir>/../lib/<name>`, then `<dir>/<name>`.
[[nodiscard]] std::vector<std::string> adapter_candidates(const std::string& executable_dir);

/// The directory of the running executable ("" when unknown).
[[nodiscard]] std::string executable_dir();

struct LoadedPlugin {
  const hefesto_engine_api* api = nullptr;  // null on failure
  std::string path;
  std::string error;
};

/// Loads the first candidate that exists (or `explicit_path` when set) and
/// checks its ABI version. The library stays loaded for the process.
[[nodiscard]] LoadedPlugin load_adapter(const std::string& explicit_path, const std::vector<std::string>& candidates);

}  // namespace hefesto::simd
