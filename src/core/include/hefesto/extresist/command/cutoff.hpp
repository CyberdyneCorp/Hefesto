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
// Hefesto orders by reverse net creation index instead, with glibc's merge
// sort and Magic's comparator (which puts runs of gate entries in reverse),
// so the order is the same on every platform (Q-NETLIST-69).
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
enum class NetSkip : std::uint8_t { none, not_included, ignored, no_record, redundant, skipped };

/// The include and ignore lists, the node record, REDUNDANT (a FastHenry
/// node list named the net) and `res:skip` (with `all` off).
[[nodiscard]] NetSkip net_skip(const InputNode& node, const ResistOptions& options, bool all);

/// The net's driver (rg_devloc, rg_ttype) and DRIVEONLY.
struct Driver {
  db::Point location;
  std::optional<tech::TileType> type;
  bool drive_only = false;

  friend bool operator==(const Driver&, const Driver&) = default;
};

/// ResSortByGate: the net's devices with the non-gate terminals first,
/// grouped by gate net and then by the other source/drain net (a net
/// mentioned later first), the gate entries last in reverse list order.
[[nodiscard]] std::vector<NodeDevice> sort_by_gate(const InputModel& input, const std::vector<NodeDevice>& devices);

/// The driver of net `net`, whose device list must already be in
/// sort_by_gate order (ResProcessNode sorts once; the list is not sorted
/// again here): the largest summed W/L of consecutive parallel
/// FET/BJT devices on a source or drain, overridden by the first drive (or
/// sink) point of a drive-located net, else the first drive or sink point;
/// nullopt when there is none. With magic-compat ON the W/L sums are
/// Magic's int totWL (truncated at each step, so a group whose W/L stays
/// below 1 drives nothing; saturated at the int range in both modes) and the driver of a parallel group that runs to
/// the end of the list is its last-but-one device (ResProcessNode's t1);
/// with it OFF the sums are floats and the driver is the group's last
/// device (Q-NETLIST-71).
[[nodiscard]] std::optional<Driver> choose_driver(const InputModel& input, std::size_t net, bool magic_compat = true);

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
