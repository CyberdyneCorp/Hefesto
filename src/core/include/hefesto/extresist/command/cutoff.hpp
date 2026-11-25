// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist net selection, driver choice and cutoffs (capability
// extresist-command "Net selection and first cutoff", "Second cutoff
// after extraction", "Lumped resistance file"; design D16): ResProcessNode,
// ResSortByGate, ResWriteExtFile's second cutoff and ResWriteLumpFile, as
// functions of the input model and the options.
//
// ResSortByGate sorts the net's device list with qsort on node addresses;
// Hefesto orders by net creation index instead (the allocation order Magic's
// addresses usually follow), with a stable sort, so the order is the same
// on every platform.
//
// Owner: RESCMD.

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/extresist/command/session.hpp"
#include "hefesto/extresist/output/output.hpp"

namespace hefesto::extresist {

/// Why a net is passed over before its driver is sought.
enum class NetSkip : std::uint8_t { none, not_included, ignored, no_record, skipped };

/// The include and ignore lists, the node record and `res:skip` (with
/// `all` off).
[[nodiscard]] NetSkip net_skip(const InputNode& node, const ResistOptions& options, bool all);

/// The net's driver (rg_devloc, rg_ttype) and DRIVEONLY.
struct Driver {
  db::Point location;
  std::optional<tech::TileType> type;
  bool drive_only = false;

  friend bool operator==(const Driver&, const Driver&) = default;
};

/// ResSortByGate: the net's devices with the non-gate terminals first,
/// grouped by gate net and then by the other source/drain net.
[[nodiscard]] std::vector<NodeDevice> sort_by_gate(const InputModel& input, const std::vector<NodeDevice>& devices);

/// The driver of net `net`: the largest summed W/L of consecutive parallel
/// FET/BJT devices on a source or drain, overridden by the first drive (or
/// sink) point of a drive-located net, else the first drive or sink point;
/// nullopt when there is none.
[[nodiscard]] std::optional<Driver> choose_driver(const InputModel& input, std::size_t net);

/// The first cutoff (processThis): `all`, the resistance and delay test,
/// or the force list. Thresholds in milliohms; the delay in ps.
[[nodiscard]] bool first_cutoff(const InputNode& node, const ResistOptions& options, bool all);

/// What network extraction measured: the largest driver-to-node
/// resistance (milliohms) and, when computed, the Elmore delay (zs).
struct Extracted {
  double maxres = 0.0;
  std::optional<double> tdi;
};

/// The second cutoff: true when the net is withheld from the output.
[[nodiscard]] bool withheld(const InputNode& node, const Extracted& measured, const ResistOptions& options);

/// ResWriteLumpFile's value in ohms.
[[nodiscard]] std::int32_t lumped_ohms(const InputNode& node, const Extracted& measured, const ResistOptions& options);

/// The options' delay threshold in zeptoseconds (ps * 1e9).
[[nodiscard]] double min_delay_zs(const ResistOptions& options) noexcept;

}  // namespace hefesto::extresist
