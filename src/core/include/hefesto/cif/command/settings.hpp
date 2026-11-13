// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CifSettings (capability cif-format-io; openspec/changes/m5-mask-gds-cif
// design.md D4, D13): the session state of the `cif` options, held per open
// project in editor::EditorState::mask.cif, as Magic keeps them in session
// globals (cif/CIFmain.c, CIFhier.c, CIFrdutils.c, CIFwrite.c). `cif
// warning` sets the read warnings shared with `gds warning`
// (mask/input/warnings.hpp, EditorState::mask.warnings).
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); CIF owns the
// fields' behaviour (design D2/D4).

#pragma once

#include <cstdint>
#include <string>

namespace hefesto::cif {

struct CifSettings {
  bool hier = true;                 // `cif *hier` (CIFHierWriteDisable inverted)
  bool array = true;                // `cif *array` (CIFArrayWriteDisable inverted)
  std::string prefix;               // `cif prefix` (CIFPathPrefix)
  bool polygon_subcells = false;    // `cif polygon subcells` (CIFSubcellPolygons)
  bool unfracture = false;          // `cif unfracture` (CIFUnfracture; Q-MASK-413)
  bool rescale = true;              // `cif rescale` (CIFRescaleAllow)
  std::int32_t limit = 0;           // `cif limit` (CIFRescaleLimit; 0 = Magic's default)
  bool arealabels = false;          // `cif arealabels` (CIFDoAreaLabels)
  bool idcell = false;              // `cif idcell` (CIFDoCellIdLabels)
  bool drccheck = true;             // `cif drccheck` (!CIFNoDRCCheck)

  friend bool operator==(const CifSettings&, const CifSettings&) = default;
};

}  // namespace hefesto::cif
