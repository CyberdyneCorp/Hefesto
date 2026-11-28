// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The result browser and the status chip (lvs-command "Result browser",
// "LVS status chip"). The browser reads the same structured result as
// `lvs -list`; nothing here recomputes a comparison.
//
// Owner: LVSCMD.

#include "hefesto/lvs/cmd/view_model.hpp"

#include <algorithm>

#include "hefesto/lvs/report/report.hpp"

namespace hefesto::lvs {

namespace {

std::uint64_t total(const std::vector<DeviceCount>& devices) {
  std::uint64_t n = 0;
  for (const auto& d : devices) {
    n += d.count;
  }
  return n;
}

BrowserPair browser_pair(const PairResult& pair, const PairReport& report) {
  BrowserPair row;
  row.cell1 = report.cell1.empty() ? pair.cell1 : report.cell1;
  row.cell2 = report.cell2.empty() ? pair.cell2 : report.cell2;
  row.status = std::string{pair_status_name(pair.status)};
  row.devices1 = total(report.devices1);
  row.devices2 = total(report.devices2);
  row.nets1 = report.nets1;
  row.nets2 = report.nets2;
  row.flattened = report.flattened;
  row.black_boxed = report.black_boxed;
  row.net_fragments = report.bad_nets;
  row.device_fragments = report.bad_elements;
  if (report.pins) {
    const auto& [pins1, pins2] = *report.pins;
    for (std::size_t k = 0; k < std::max(pins1.size(), pins2.size()); ++k) {
      row.pins.emplace_back(k < pins1.size() ? pins1[k] : "", k < pins2.size() ? pins2[k] : "");
    }
  }
  row.properties = report.properties;
  return row;
}

bool matched(PairStatus status) {
  return status == PairStatus::match || status == PairStatus::black_box;
}

bool inputs_changed(const Session& session, std::string_view document, platform::FileSystem* files) {
  if (session.changed.count(document) != 0) {
    return true;
  }
  const auto it = session.inputs.find(document);
  if (files == nullptr || it == session.inputs.end()) {
    return false;
  }
  return std::any_of(it->second.begin(), it->second.end(), [files](const Session::InputStamp& stamp) {
    return files->modified_time(stamp.path) != stamp.modified;
  });
}

}  // namespace

BrowserModel browser_model(const LvsResult& result) {
  BrowserModel model;
  model.overall = std::string{overall_status_name(overall_status(result))};
  model.top_pin_mismatch = result.top_pin_mismatch;
  model.failed_subcells = result.failed_subcells;
  model.property_error_cells = result.property_error_cells;
  for (std::size_t i = 0; i < result.pairs.size(); ++i) {
    const PairReport empty;
    model.pairs.push_back(browser_pair(result.pairs[i], i < result.reports.size() ? result.reports[i] : empty));
  }
  model.comp_out = write_comp_out(result).value();
  return model;
}

std::string chip_text(ChipState state, std::size_t mismatched) {
  switch (state) {
    case ChipState::not_run:
      return "LVS not run";
    case ChipState::match:
      return "LVS match";
    case ChipState::mismatch:
      return "LVS mismatch (" + std::to_string(mismatched) + ")";
    case ChipState::error:
      return "LVS error";
    case ChipState::cancelled:
      return "LVS cancelled";
    case ChipState::stale:
      return "LVS stale";
  }
  return "LVS error";
}

StatusChip status_chip(const Session& session, std::string_view document, platform::FileSystem* files) {
  StatusChip chip;
  const auto it = session.last_results.find(document);
  if (it == session.last_results.end()) {
    chip.text = chip_text(chip.state, 0);
    return chip;
  }
  const LvsResult& result = it->second;
  chip.mismatched = static_cast<std::size_t>(std::count_if(
      result.pairs.begin(), result.pairs.end(), [](const PairResult& pair) { return !matched(pair.status); }));
  switch (overall_status(result)) {
    case OverallStatus::clean:
      chip.state = ChipState::match;
      break;
    case OverallStatus::mismatch:
      // A run that failed only on the top level's pins still counts one.
      chip.state = ChipState::mismatch;
      chip.mismatched = std::max<std::size_t>(chip.mismatched, 1);
      break;
    case OverallStatus::error:
      chip.state = ChipState::error;
      break;
    case OverallStatus::cancelled:
      chip.state = ChipState::cancelled;
      break;
  }
  if (inputs_changed(session, document, files)) {
    chip.state = ChipState::stale;
  }
  chip.text = chip_text(chip.state, chip.mismatched);
  return chip;
}

}  // namespace hefesto::lvs
