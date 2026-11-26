// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The plot parameters (capability plot-output "Plot parameters";
// openspec/changes/m10-plot-route/design.md D4, D7, "Area contracts"
// PLOTCMD). One Params per session (EditorState::plot), set to Magic's
// defaults at start-up only: Magic keeps them across technology loads. The
// HP/Versatec parameters and `pnmplotRTL` are not offered (Q-MISC-51 (a)).
//
// Owner: PLOTCMD.

#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::plot {

/// The 14 parameters, with the types and defaults Magic uses
/// (plotMain.c, plotPS.c, plotPNM.c).
struct Params {
  bool show_cell_names = true;              // showCellNames
  std::string ps_id_font = "/Helvetica";    // PS_cellIdFont
  std::string ps_name_font = "/HelveticaBold";  // PS_cellNameFont
  std::string ps_label_font = "/Helvetica";  // PS_labelFont
  int ps_id_size = 8;                       // PS_cellIdSize
  int ps_name_size = 12;                    // PS_cellNameSize
  int ps_label_size = 12;                   // PS_labelSize
  bool ps_boundary = true;                  // PS_boundary
  int ps_width = 612;                       // PS_width (8.5 in)
  int ps_height = 792;                      // PS_height (11 in)
  int ps_margin = 72;                       // PS_margin (1 in)
  int pnm_maxmem = 64 * 1024;               // pnmmaxmem (KB)
  int pnm_downsample = 0;                   // pnmdownsample
  int pnm_background = 0xff;                // pnmbackground

  /// `plot parameters` with no argument: Magic's listing.
  void print(std::ostream& out) const;
  /// `plot parameters <name> <value>`: unique-prefix match on the name,
  /// Magic's constraint messages.
  [[nodiscard]] diag::Status set(std::string_view name, std::string_view value);
};

}  // namespace hefesto::plot
