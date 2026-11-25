// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): every member fails with CORE-NOTIMPL-009.
//
// Owner: SIMRES.

#include "hefesto/sim/result/result_store.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Status ResultStore::write_raw(const JobId& /*job*/, const Plot& /*plot*/) const {
  return not_implemented("sim::ResultStore::write_raw");
}

diag::Outcome<Plot> ResultStore::read_raw(const platform::Path& /*path*/) const {
  return not_implemented("sim::ResultStore::read_raw");
}

diag::Status ResultStore::export_csv(const Plot& /*plot*/, const platform::Path& /*path*/) const {
  return not_implemented("sim::ResultStore::export_csv");
}

diag::Outcome<double> ResultStore::measure_waveform(const Plot& /*plot*/, const WaveformMeasure& /*measure*/) const {
  return not_implemented("sim::ResultStore::measure_waveform");
}

}  // namespace hefesto::sim
