// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResultStore (task 13.3): raw files under `<root>/<job id>/result.raw`,
// raw import with its cap (design D18), CSV export and waveform
// measurements over the pure functions of raw.hpp.
//
// Owner: SIMRES.

#include "hefesto/sim/result/result_store.hpp"

#include <utility>

#include "hefesto/sim/result/raw.hpp"

namespace hefesto::sim {

namespace {

diag::Diagnostic no_files() { return diag::make("SIM-RESULT-009", "the result store has no file system"); }

}  // namespace

diag::Status ResultStore::write_raw(const JobId& job, const Plot& plot) const {
  if (files == nullptr) {
    return no_files();
  }
  if (!is_safe_job_id(job)) {  // M9 R2 review: `root / job` must stay below root
    return diag::make("SIM-RESULT-009", "job id '" + job + "' is not a single path component");
  }
  RawWriteOptions options;
  options.title = job;
  auto text = render_raw({plot}, options);
  if (!text.ok()) {
    return text.error();
  }
  const platform::Path dir = root / job;
  if (diag::Status made = files->create_directories(dir); !made.ok()) {
    return diag::make("SIM-RESULT-009", "cannot create " + dir.generic_string() + ": " + made.error().text);
  }
  if (diag::Status written = files->write_atomic(dir / "result.raw", text.value()); !written.ok()) {
    return diag::make("SIM-RESULT-009", "cannot write " + (dir / "result.raw").generic_string() + ": " +
                                            written.error().text);
  }
  return diag::Done{};
}

diag::Outcome<Plot> ResultStore::read_raw(const platform::Path& path) const {
  if (files == nullptr) {
    return no_files();
  }
  const std::string name = path.filename().generic_string();
  if (auto size = files->size(path); size.ok() && size.value() > kMaxRawImportBytes) {
    return diag::make("SIM-RESULT-003", name + ": " + std::to_string(size.value()) +
                                            " bytes exceed the raw import limit of " +
                                            std::to_string(kMaxRawImportBytes) + " bytes");
  }
  auto data = files->read_at_most(path, static_cast<std::size_t>(kMaxRawImportBytes));
  if (!data.ok()) {
    return diag::make("SIM-RESULT-009", "cannot read " + path.generic_string() + ": " + data.error().text);
  }
  auto plots = parse_raw(data.value(), name, kDesktopResultBudget);
  if (!plots.ok()) {
    return plots.error();
  }
  return std::move(plots.value().front());
}

diag::Status ResultStore::export_csv(const Plot& plot, const platform::Path& path) const {
  if (files == nullptr) {
    return no_files();
  }
  auto text = render_csv(plot);
  if (!text.ok()) {
    return text.error();
  }
  if (diag::Status written = files->write_atomic(path, text.value()); !written.ok()) {
    return diag::make("SIM-RESULT-009", "cannot write " + path.generic_string() + ": " + written.error().text);
  }
  return diag::Done{};
}

diag::Outcome<double> ResultStore::measure_waveform(const Plot& plot, const WaveformMeasure& m) const {
  return measure(plot, m);
}

}  // namespace hefesto::sim
