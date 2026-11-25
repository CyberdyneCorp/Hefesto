// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The in-process developer engine host (Q-SIM-22 (a); openspec/changes/
// m9-lvs-simulation design D2; capability simulation-engine-embedding "Crash
// containment on desktop", last paragraph). Built only with
// HEFESTO_DEVELOPER_BUILD=ON: it loads the same hefesto-ngspice-adapter
// plugin as hefesto-simd, behind sim::Engine, into the calling process, for
// debugging. Release builds do not compile it, and
// scripts/check_engine_boundary.py fails a release build tree whose binaries
// (other than hefesto-simd) reference the plugin.
//
// A fault in the engine takes the process down with it: that is why
// release builds use the worker.
//
// Owner: SIMD.

#pragma once

#include <memory>
#include <string>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/engine.hpp"

namespace hefesto::simd {

/// The in-process engine over the plugin at `plugin_path` (empty: the
/// plugin next to the running executable). Fails with SIM-ENGINE-001 when
/// the plugin cannot be loaded or the engine cannot be created (one per
/// process).
[[nodiscard]] diag::Outcome<std::unique_ptr<sim::Engine>> make_in_process_engine(const std::string& plugin_path);

}  // namespace hefesto::simd
