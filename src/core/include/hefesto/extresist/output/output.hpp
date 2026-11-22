// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist input model and output (capabilities extresist-command "Input
// model", extresist-output; openspec/changes/m7-extraction/design.md D16):
// ResReadExt's input model over EXTFLAT's tokenizer, and the `.res.ext`,
// `.res.lump`, FastHenry and geometry writers. The three files are written
// through extract::AtomicOutput (Q-EXTRACT-27).
//
// Scaffold state (tasks.md 3.3): every function reports CORE-NOTIMPL-008
// until RESCMD lands it.
//
// Owner: INTEGRATION (this header); RESCMD (the implementation).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/search_path.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace hefesto::extresist {

/// One node of the `.ext` input model (ResExtNode).
struct InputNode {
  std::string name;
  db::Point location;
  std::string type;
  double resistance = 0.0;  // lumped, milliohms
  double cap = 0.0;         // attofarads
};

/// ResReadExt's view of a cell's `.ext` file.
struct InputModel {
  std::string cell;
  std::vector<InputNode> nodes;
  DeviceAnnotations devices;
};

/// A centerline segment of the geometry output.
struct GeometryElement {
  db::Point from;
  db::Point to;
  std::int32_t width = 0;
  std::string layer;
};

[[nodiscard]] diag::Outcome<InputModel> read_input(cell::LookupContext& ctx, std::string_view cell,
                                                   const compat::CompatSettings& compat);
/// The `.res.ext` text of the reduced networks (Q-NETLIST-39 per mode).
[[nodiscard]] diag::Outcome<std::string> write_res_ext(const InputModel& input, const std::vector<Network>& nets,
                                                       const compat::CompatSettings& compat);
[[nodiscard]] diag::Outcome<std::string> write_res_lump(const InputModel& input, const std::vector<Network>& nets);
[[nodiscard]] diag::Outcome<std::string> write_fasthenry(const InputModel& input, const std::vector<Network>& nets,
                                                         double frequency, const compat::CompatSettings& compat);
[[nodiscard]] diag::Outcome<std::vector<GeometryElement>> geometry_elements(const Network& net);

}  // namespace hefesto::extresist
