// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtStyle: the extraction style of the loaded technology, Magic's
// ExtCurStyle data model (capability extract-tech-section;
// openspec/changes/m7-extraction/design.md D5, D7). One value per style,
// built by the one `extract` section reader (M6's style subset, M10's
// height subset, completed by EXTTECH), shared read-only through
// `std::shared_ptr<const ExtStyle>` (EditorState::extract.current): the
// style is replaced on tech load, `extract style`, `scalegrid` and
// finer-grid reads, never mutated.
//
// StyleView (style_view.hpp) and M10's HeightView (height_view.hpp) keep
// their interfaces; EXTTECH re-sources them from ExtStyle (tasks.md 4.7).
//
// Scaffold state (tasks.md 3.3): the data model is complete enough for the
// other workstreams to build fixtures with testing::StyleBuilder
// (testing/style_builder.hpp); load_styles(), rescale() and
// micron_convert() are stubs that EXTTECH fills (tasks.md 4.1-4.6).
//
// Owner: INTEGRATION (this header); EXTTECH (the members and builders).

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::extract {

/// The device classes of a `device` line (ExtTech.c devTable), in Magic's
/// order.
enum class DeviceClass : std::uint8_t {
  fet,
  mosfet,
  asymmetric,
  bjt,
  devres,
  devcap,
  devcaprev,
  vsource,
  diode,
  pdiode,
  ndiode,
  subckt,
  rsubckt,
  msubckt,
  csubckt,
};

/// One `area` or `perimeter` parameter of a device line, or a named one
/// (`a1`, `p1`, `l`, `w`, `c`, ...): the name, the Magic parameter code and
/// its scale and offset (ParamList).
struct DeviceParam {
  std::string name;
  std::string code;
  double scale = 1.0;
  double offset = 0.0;

  friend bool operator==(const DeviceParam&, const DeviceParam&) = default;
};

/// A parameter range of a device line (`l=0.15:1.0`): min and max as
/// written (Q-EXTRACT-21 decides the unit per mode).
struct ParamRange {
  std::string name;
  std::optional<double> min;
  std::optional<double> max;

  friend bool operator==(const ParamRange&, const ParamRange&) = default;
};

/// One `device` line of a style (ExtDevice): one per line, in declaration
/// order within its device type.
struct DevicePattern {
  DeviceClass device_class = DeviceClass::mosfet;
  std::string model;                         // the model (SPICE) name
  tech::TypeMask types;                      // the device (gate) types
  std::vector<tech::TypeMask> terminals;     // terminal type masks, in terminal order
  tech::TypeMask substrate_types;            // exts_deviceSubstrateTypes
  std::string substrate_node;                // default substrate node name, `$` variables unexpanded
  tech::TypeMask identifiers;                // identifier layers (exts_deviceIdentifierTypes)
  std::vector<DeviceParam> params;
  std::vector<ParamRange> ranges;

  friend bool operator==(const DevicePattern&, const DevicePattern&) = default;
};

/// One resistance class (`resist` line) and the contact data of its
/// types (`contact` lines).
struct ResistClass {
  tech::TypeMask types;
  double sheet_resistance = 0.0;  // milliohms per square
  double contact_resistance = 0.0;
  std::int32_t contact_size = 0;

  friend bool operator==(const ResistClass&, const ResistClass&) = default;
};

/// One overlap rule (`overlap top bottom cap [shield]`).
struct OverlapRule {
  tech::TypeMask top;
  tech::TypeMask bottom;
  double cap = 0.0;  // aF per square internal unit after conversion
  tech::TypeMask shield;

  friend bool operator==(const OverlapRule&, const OverlapRule&) = default;
};

/// One sidewall rule (`sidewall inside outside near far cap [offset]`).
/// The offset is single precision, as Magic stores it (Q-EXTRACT-20).
struct SidewallRule {
  tech::TypeMask inside;
  tech::TypeMask outside;
  tech::TypeMask near_types;
  tech::TypeMask far_types;
  double cap = 0.0;
  float offset = 0.0F;

  friend bool operator==(const SidewallRule&, const SidewallRule&) = default;
};

/// One sideoverlap rule (`sideoverlap inside outside over cap [shield]`).
struct SideOverlapRule {
  tech::TypeMask inside;
  tech::TypeMask outside;
  tech::TypeMask over;
  double cap = 0.0;
  tech::TypeMask shield;

  friend bool operator==(const SideOverlapRule&, const SideOverlapRule&) = default;
};

/// Area capacitance of one type (`areacap`), or perimeter capacitance of
/// the edges from `type` to `outside` (`perimc`).
struct TypeCap {
  tech::TileType type = 0;
  tech::TypeMask outside;  // perimeter rules only
  double cap = 0.0;

  friend bool operator==(const TypeCap&, const TypeCap&) = default;
};

/// The capacitance rules of a style, with the coupling tables completed
/// (ExtTechFinal).
struct CapRules {
  std::vector<TypeCap> area;
  std::vector<TypeCap> perimeter;
  std::vector<OverlapRule> overlap;
  std::vector<SidewallRule> sidewall;
  std::vector<SideOverlapRule> sideoverlap;
  std::int32_t fringe_shield_halo = 0;

  friend bool operator==(const CapRules&, const CapRules&) = default;
};

/// The antenna calculation model of a type (`antenna` line).
enum class AntennaModel : std::uint8_t { none, surface, sidewall };

/// One type's antenna rule (`antenna type model ratio [ratio...]`) and the
/// style's `model` flags (Q-MISC-2, Q-MISC-3, Q-MISC-5).
struct AntennaRule {
  tech::TileType type = 0;
  AntennaModel model = AntennaModel::none;
  double ratio_gate = 0.0;
  std::vector<double> ratio_diff;  // the piecewise-linear diffusion ratios, as written

  friend bool operator==(const AntennaRule&, const AntennaRule&) = default;
};

/// The `substrate` declaration of a style.
struct SubstrateDecl {
  tech::TypeMask types;
  tech::TypeMask shield_types;
  std::string node_name;  // with its `$` variable unexpanded (Q-EXTRACT-9)
  std::optional<tech::PlaneId> plane;

  friend bool operator==(const SubstrateDecl&, const SubstrateDecl&) = default;
};

/// The fringe model a style runs with ("Fringe model default chosen from
/// tech format"): fringe halo for format 34 and later.
enum class FringeModel : std::uint8_t { legacy, fringe_halo };

/// One entry of ExtGetDevInfo's table (ExtTech.c:312-466), read by SPICE,
/// SIM and ANTENNA: per device in ExtGetDevInfo's index order.
struct DeviceInfo {
  std::string model;
  tech::TileType type = 0;
  std::int32_t source_class = -1;     // resistance class of the source terminal, -1 = none
  std::int32_t drain_class = -1;      // of the drain terminal
  std::int32_t substrate_class = -1;  // of the substrate terminal
  std::string substrate_node;         // default substrate node after `$` substitution

  friend bool operator==(const DeviceInfo&, const DeviceInfo&) = default;
};

/// The height and thickness of one type (M10's subset).
struct TypeHeightThickness {
  float height = 0.0F;
  float thickness = 0.0F;

  friend bool operator==(const TypeHeightThickness&, const TypeHeightThickness&) = default;
};

/// ExtCurStyle.
class ExtStyle {
 public:
  // Scale.
  std::string name;
  std::int32_t scale = 1;          // exts_unitsPerLambda numerator as read (`scale`)
  bool units_microns = false;      // `units microns`
  float units_per_lambda = 100.0F; // exts_unitsPerLambda
  double cscale = 1.0;             // capacitance scale
  double rscale = 1.0;             // resistance scale
  std::int32_t step_size = 0;      // exts_stepSize
  std::int32_t side_halo = 0;      // exts_sideCoupleHalo
  FringeModel fringe_model = FringeModel::fringe_halo;

  // Planes.
  std::vector<std::int32_t> plane_order;  // index = tech::PlaneId, -1 = none
  bool plane_ordering = true;             // false after `noplaneordering`
  std::array<TypeHeightThickness, tech::kMaxTileTypes> heights{};

  // Connectivity overrides (exts_nodeConn, exts_deviceConn, exts_resistConn),
  // index = tech::TileType.
  std::vector<tech::TypeMask> node_conn;
  std::vector<tech::TypeMask> device_conn;
  std::vector<tech::TypeMask> resist_conn;

  std::vector<ResistClass> resist_classes;
  CapRules caps;
  std::vector<DevicePattern> devices;
  SubstrateDecl substrate;
  tech::TypeMask non_electrical;
  tech::TypeMask active;
  std::vector<AntennaRule> antenna;
  tech::TypeMask tiedown;

  /// ExtGetDevInfo's table (EXTTECH fills it while loading, tasks.md 4.5).
  [[nodiscard]] std::span<const DeviceInfo> device_info() const noexcept { return device_info_; }
  void set_device_info(std::vector<DeviceInfo> table) { device_info_ = std::move(table); }

  /// StyleView's four queries (style_view.hpp).
  [[nodiscard]] tech::TypeMask gate_types() const;
  [[nodiscard]] tech::TypeMask diff_types() const { return tiedown; }
  [[nodiscard]] tech::TypeMask active_types() const { return active; }
  [[nodiscard]] tech::TypeMask substrate_types() const { return substrate.types; }

  /// ExtTechScale(scalen = n, scaled = d), in Magic's argument order: every
  /// distance scaled by d/n, units_per_lambda by n/d and areas by (n/d)^2
  /// (Q-EXTRACT-19, Q-EXTRACT-25). Stub: the copy is returned unchanged
  /// until tasks.md 4.6.
  [[nodiscard]] ExtStyle rescale(std::int32_t n, std::int32_t d, const compat::CompatSettings& compat) const;
  /// ExtTechFinal's micron conversion with the cifoutput scale of `style`
  /// (nullptr: no output style; Q-EXTRACT-3). Stub until tasks.md 4.6.
  [[nodiscard]] ExtStyle micron_convert(const mask::OutputStyle* style) const;

  friend bool operator==(const ExtStyle&, const ExtStyle&) = default;

 private:
  std::vector<DeviceInfo> device_info_;
};

/// The style table of a technology: names in declaration order, variants
/// expanded; each style parsed on first selection (ExtLoadStyle).
class StyleTable {
 public:
  StyleTable() = default;
  explicit StyleTable(std::vector<std::string> names) : names_(std::move(names)) {}

  [[nodiscard]] std::span<const std::string> names() const noexcept { return names_; }
  /// The style of that exact name, parsed on first use; nullptr when the
  /// table has no such style. Stub: nullptr until tasks.md 4.1.
  [[nodiscard]] std::shared_ptr<const ExtStyle> select(std::string_view name) const;

 private:
  std::vector<std::string> names_;
};

/// Reads the style names of the technology's `extract` section. Stub: an
/// empty table until tasks.md 4.1.
[[nodiscard]] StyleTable load_styles(const tech::RawSection& section, const tech::Technology& technology,
                                     diag::DiagnosticSink& sink, const compat::CompatSettings& compat);

}  // namespace hefesto::extract
