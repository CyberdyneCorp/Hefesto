// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The view models of the LVS result browser and of the top bar's status
// chip (capability lvs-command "Result browser", "LVS status chip"; design
// D16). Core code only: the desktop shell (SHELL, task 17.2) draws them.
// Cross-probing from an entry to the layout or schematic waits on the name
// maps (MAPS, task 11); an entry carries the references the run attached.
//
// Owner: LVSCMD.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/lvs/hier/run.hpp"
#include "hefesto/lvs/session.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::lvs {

/// One compared pair in the browser, in queue order.
struct BrowserPair {
  std::string cell1;
  std::string cell2;
  std::string status;  // lvs-reports "Result classification" name
  std::uint64_t devices1 = 0;
  std::uint64_t devices2 = 0;
  std::uint64_t nets1 = 0;
  std::uint64_t nets2 = 0;
  bool flattened = false;
  bool black_boxed = false;
  std::vector<MismatchClass<NetFragment>> net_fragments;
  std::vector<MismatchClass<InstanceFragment>> device_fragments;
  std::vector<std::pair<std::string, std::string>> pins;  // pin pairs after matching
  std::vector<PropertyPair> properties;
};

struct BrowserModel {
  std::string overall;  // `clean`, `mismatch`, `error` or `cancelled`
  bool top_pin_mismatch = false;
  std::vector<std::string> failed_subcells;
  std::vector<std::string> property_error_cells;
  std::vector<BrowserPair> pairs;
  std::string comp_out;  // the raw log text
};

[[nodiscard]] BrowserModel browser_model(const LvsResult& result);

/// The six states of the chip.
enum class ChipState : std::uint8_t { not_run, match, mismatch, error, cancelled, stale };

struct StatusChip {
  ChipState state = ChipState::not_run;
  std::size_t mismatched = 0;  // pairs that are not `match` or `black-box`
  std::string text;
};

/// `LVS not run`, `LVS match`, `LVS mismatch (<n>)`, `LVS error`,
/// `LVS cancelled` or `LVS stale`.
[[nodiscard]] std::string chip_text(ChipState state, std::size_t mismatched);

/// The chip of `document`: stale when its layout changed (Session::changed)
/// or an input file's modification time differs from the run's; `files`
/// may be nullptr (no file check).
[[nodiscard]] StatusChip status_chip(const Session& session, std::string_view document,
                                     platform::FileSystem* files);

}  // namespace hefesto::lvs
