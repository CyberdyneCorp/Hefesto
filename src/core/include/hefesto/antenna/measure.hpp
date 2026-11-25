// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The antenna measurement and ratio test of `antennacheck` (capability
// antenna-check "Per-plane progressive evaluation", "Antenna area
// measurement", "Antenna ratio test"; design D17). A gate net is traced
// once per check plane into a TracedNet (Magic's __PATHYANK__ cell), then
// measured: the tie-down area D, the gate area A_g and the per-type antenna
// areas a[t]; evaluate() applies the ratio test.
//
// The decisions applied here:
// - Q-MISC-1 (compat): the cumulative model sums nothing with magic-compat
//   ON; with it OFF it sums every type's term.
// - Q-MISC-4 (compat): the contact cut count mixes internal and output
//   units with magic-compat ON; with it OFF it converts the tile first and
//   keeps f^2 fractional. In both modes a contact with no cut rule counts
//   0 and a zero pitch counts one cut per direction.
// - Q-MISC-6 (fix, both modes): plane-order indices with no plane are
//   skipped (check_planes()).
// - Q-MISC-7 (compat): the connectivity restriction of the first contact
//   on the check plane by plane number with magic-compat ON; of every such
//   contact by plane order with it OFF (restricted_connect()).
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c
// (antennacheckVisit, areaAccumFunc, antennaAccumFunc), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: ANTENNA.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "hefesto/antenna/antenna.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::antenna {

/// The cut rule of a contact type: the first cifoutput `squares`/`slots`
/// operator generating it (CIFGetContactSize), in output units, and the
/// style's scale factor.
struct CutRule {
  std::int32_t size = 0;
  std::int32_t spacing = 0;
  std::int32_t border = 0;
  std::int32_t scale_factor = 1;
};

/// The cut rule of a type; nullopt when no cifoutput style generates it.
using CutLookup = std::function<std::optional<CutRule>(tech::TileType)>;

/// What the measurement reads.
struct MeasureSetup {
  const tech::Technology* tech = nullptr;
  const RuleView* rules = nullptr;
  CutLookup cuts;
  bool magic_compat = true;
};

/// A gate net traced for one check plane (DBTreeCopyConnect's yank cell):
/// one plane per technology plane, painted with the pieces the search
/// reports.
class TracedNet {
 public:
  explicit TracedNet(const tech::Technology& tech);

  /// Paints one piece of the net on its plane.
  [[nodiscard]] diag::Status add(const connect::NetTile& piece);

  [[nodiscard]] const db::Plane& plane(tech::PlaneId plane) const { return planes_.at(plane); }
  [[nodiscard]] std::size_t plane_count() const noexcept { return planes_.size(); }

 private:
  const tech::Technology* tech_;
  std::vector<db::Plane> planes_;
};

/// One check plane P_k and its order k.
struct CheckPlane {
  tech::PlaneId plane = 0;
  std::int32_t order = 0;
};

/// The check planes of a gate on `gate_plane`, in order: every order k
/// from order(G) + 1 to the highest plane order that some plane has
/// (Q-MISC-6: an order with no plane is skipped).
[[nodiscard]] std::vector<CheckPlane> check_planes(const RuleView& rules, const tech::Technology& tech,
                                                   tech::PlaneId gate_plane);

/// The connect table of the trace at `check` (Q-MISC-7 per mode).
[[nodiscard]] connect::ConnectTable restricted_connect(const RuleView& rules, const tech::Technology& tech,
                                                       const CheckPlane& check, bool magic_compat);

/// The cut count and cut area of a w x h contact tile (Q-MISC-4 per mode).
struct ContactCuts {
  std::int64_t nx = 1;
  std::int64_t ny = 1;
  std::int64_t area = 0;       // C
  std::int64_t perimeter = 0;  // cperim
};
[[nodiscard]] ContactCuts contact_cuts(db::Coord w, db::Coord h, const std::optional<CutRule>& rule,
                                       bool magic_compat);

/// A sum of areas and the last rectangle it visited (Magic prints the
/// last gate and antenna rectangles in debug mode).
struct AreaSum {
  std::int64_t area = 0;
  db::Rect last;
};

/// The tie-down area D: every tile of a `tiedown` type on any plane, a
/// contact on its home plane only, a split tile half its box.
[[nodiscard]] AreaSum tiedown_area(const TracedNet& net, const MeasureSetup& setup);
/// The gate area A_g: the gate-type tiles on `gate_plane`.
[[nodiscard]] AreaSum gate_area(const TracedNet& net, const MeasureSetup& setup, tech::PlaneId gate_plane);

/// The per-type antenna areas a[t] (index = tile type) of the planes the
/// model includes at `check`, and the last non-contact tile measured.
struct AntennaAreas {
  std::vector<std::int64_t> area;
  db::Rect last;
};
[[nodiscard]] AntennaAreas antenna_areas(const TracedNet& net, const MeasureSetup& setup, const CheckPlane& check);

/// Whether plane `plane` is searched for antenna area at `check`.
[[nodiscard]] bool plane_included(const RuleView& rules, tech::PlaneId plane, const CheckPlane& check);

/// The ratio test of one check plane.
struct Evaluation {
  double metal = 0.0;  // S_metal (anttotal)
  double via = 0.0;    // S_via (conttotal)
  float limit = 0.0F;  // L (saveRatio)
  bool metal_violation = false;
  bool via_violation = false;

  [[nodiscard]] bool violation() const noexcept { return metal_violation || via_violation; }
};
[[nodiscard]] Evaluation evaluate(const MeasureSetup& setup, const std::vector<std::int64_t>& areas,
                                  std::int64_t gate, std::int64_t diffusion, const CheckPlane& check);

/// The effective ratio Magic prints: L * S / A_g in single precision
/// (+inf when A_g is 0).
[[nodiscard]] float effective_ratio(float limit, double sum, std::int64_t gate);

}  // namespace hefesto::antenna
