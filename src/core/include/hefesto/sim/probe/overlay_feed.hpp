// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::OverlayFeed: the read-only data feed to the 3D overlays
// (openspec/changes/m9-lvs-simulation design.md D13, "Area contracts"
// SIMPROBE; capability simulation-ui-and-crossprobe "Data feed to 3D
// overlays"), consumed by M10 P3. Values come from the stored vectors and
// the deck's resistor values, interpolated linearly at `time`; nothing is
// re-simulated.
//
// Owner: SIMPROBE.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/job.hpp"

namespace hefesto::sim {

/// Net voltage of one mapped net with a saved vector, in V.
struct NetVoltage {
  std::string layout_name;
  double volts = 0.0;
};

/// Current through one `extresist` resistor, positive from `node1` to
/// `node2` (mode `rc` only).
struct EdgeCurrent {
  std::string net;  // the parent net's layout_name
  std::string node1;
  std::string node2;
  double amps = 0.0;
  std::optional<double> linear_density;      // Jw in mA/um; nullopt when the width is unknown
  std::optional<double> volumetric_density;  // J in mA/um^2; nullopt when width or thickness is unknown
};

class OverlayFeed {
 public:
  /// The net voltages of `job` at `time` (the plot scale; ignored for `op`).
  [[nodiscard]] diag::Outcome<std::vector<NetVoltage>> voltages(const Job& job, double time) const;
  /// The resistor-edge currents of `job` at `time`.
  [[nodiscard]] diag::Outcome<std::vector<EdgeCurrent>> currents(const Job& job, double time) const;
};

}  // namespace hefesto::sim
