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
// Building a style (tasks.md 4.2-4.6): load_styles() reads every style of
// the section once (ExtTechLine for that style, then the parse half of
// extTechFinalStyle: plane-order check, height defaults, contact residues,
// coupling-table completion); StyleTable::select() hands out a parsed
// style; micron_convert() runs the rest of extTechFinalStyle (micron
// conversion, zero-step reset, the /1000 normalisation of the side halo
// and of parameter offsets and limits) and rescale() is ExtTechScale. The
// hook calls them in that order (editor/extract_hook.cpp). Fixtures built
// with testing::StyleBuilder (testing/style_builder.hpp) skip the parse.
//
// The capacitance rules have two forms: CapRules' lists (one entry per type
// or type pair, what fixtures set) and CapTables (Magic's per-type and
// per-pair tables after ExtTechFinal, sparse). The reader fills both;
// micron_convert() and rescale() convert both.
//
// Owner: INTEGRATION (this header); EXTTECH (the members and builders).

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
/// order (extract.h DEV_*).
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
  dsubckt,   // DEV_DSUBCKT (15)
  veriloga,  // DEV_VERILOGA (16)
};

/// One `area` or `perimeter` parameter of a device line, or a named one
/// (`a1`, `p1`, `l`, `w`, `c`, ...): the name, the Magic parameter code and
/// its scale and offset (ParamList).
///
/// `offset`, `minimum` and `maximum` are Magic's pl_offset, pl_minimum and
/// pl_maximum: milli-units as parsed (x1000); internal units after
/// micron_convert() with magic-compat ON (integer values); still
/// milli-units with it OFF (Q-EXTRACT-21). minimum > maximum means "no
/// range" (the defaults are 0 and -1). `name` is empty for a limit-only
/// parameter.
struct DeviceParam {
  std::string name;
  std::string code;
  double scale = 1.0;
  double offset = 0.0;
  double minimum = 0.0;
  double maximum = -1.0;

  friend bool operator==(const DeviceParam&, const DeviceParam&) = default;
};

/// A parameter range of a device line (`l=0.15:1.0`): min and max as
/// written (Q-EXTRACT-21 decides the unit per mode). The reader gives one
/// per parameter with a written limit (name = the code), in the unit of
/// DeviceParam's limits.
struct ParamRange {
  std::string name;
  std::optional<double> min;
  std::optional<double> max;

  friend bool operator==(const ParamRange&, const ParamRange&) = default;
};

/// A `devresist` / `fetresist` value of one device record
/// (exts_deviceResist): the record this line makes for `type`, the region
/// key and the value (ohms).
struct DeviceResist {
  tech::TileType type = 0;
  std::string region;
  std::int32_t value = 0;

  friend bool operator==(const DeviceResist&, const DeviceResist&) = default;
};

/// One `device` (or `fet`) line of a style. Magic makes one ExtDevice per
/// type of `types`, all sharing the parameter list; ExtStyle::devices keeps
/// the lines in declaration order, and the record Magic tries first for a
/// type is the last line that names it (ExtStyle::records_for()).
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
  std::int32_t terminal_count = 0;   // exts_deviceSDCount (nterm)
  double perimeter_cap = 0.0;        // exts_deviceSDCap (gscap)
  double area_cap = 0.0;             // exts_deviceGateCap (gccap)
  std::vector<DeviceResist> resist;  // devresist values, per record

  friend bool operator==(const DevicePattern&, const DevicePattern&) = default;
};

/// One resistance class (`resist` line), in class order. The per-type
/// tables are ExtStyle::resist_class, sheet_resistance, corner_chop and
/// via_resistance (Magic's); contact_resistance is the `contact` value of
/// the class's lowest type that has one, and contact_size stays 0 (`contact`
/// sizes are ignored, as in Magic).
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

/// An ordered pair of tile types, the key of Magic's [NT][NT] tables.
using TypePair = std::pair<tech::TileType, tech::TileType>;

/// One rule of an edge's sidewall or sideoverlap list (EdgeCap): near =
/// ec_near (sideoverlap: the overlapped types), far = ec_far (sideoverlap:
/// the shielding types), planes = ec_pmask.
struct EdgeRule {
  double cap = 0.0;
  float offset = 0.0F;
  tech::TypeMask near_types;
  tech::TypeMask far_types;
  tech::PlaneMask planes = 0;

  friend bool operator==(const EdgeRule&, const EdgeRule&) = default;
};

/// Magic's capacitance tables (ExtStyle exts_*), sparse: a pair absent from
/// a map has Magic's zero value. Edge lists are in Magic's list order (the
/// rule declared last first). Index = tech::TileType or tech::PlaneId.
struct CapTables {
  std::array<double, tech::kMaxTileTypes> area{};                   // exts_areaCap
  std::map<TypePair, double> perimeter;                              // exts_perimCap
  std::array<tech::TypeMask, tech::kMaxTileTypes> perimeter_mask{};  // exts_perimCapMask
  tech::PlaneMask overlap_planes = 0;                                // exts_overlapPlanes
  std::array<tech::TypeMask, tech::kMaxPlanes> overlap_types{};      // exts_overlapTypes
  std::array<tech::TypeMask, tech::kMaxTileTypes> overlap_other_types{};
  std::array<tech::PlaneMask, tech::kMaxTileTypes> overlap_other_planes{};
  std::map<TypePair, tech::TypeMask> overlap_shield_types;
  std::map<TypePair, tech::PlaneMask> overlap_shield_planes;
  std::map<TypePair, double> overlap;                                // exts_overlapCap
  std::map<TypePair, float> fringe_mult;                             // exts_overlapMult
  std::map<TypePair, std::vector<EdgeRule>> side_couple;             // exts_sideCoupleCap
  std::map<TypePair, tech::TypeMask> side_couple_other_edges;
  std::map<TypePair, std::vector<EdgeRule>> side_overlap;            // exts_sideOverlapCap
  std::map<TypePair, tech::PlaneMask> side_overlap_other_planes;
  std::map<TypePair, tech::TypeMask> side_overlap_other_types;
  std::map<TypePair, tech::PlaneMask> side_overlap_shield_planes;
  tech::PlaneMask side_planes = 0;                                   // exts_sidePlanes
  std::array<tech::TypeMask, tech::kMaxPlanes> side_types{};         // exts_sideTypes
  std::array<tech::TypeMask, tech::kMaxTileTypes> side_edges{};      // exts_sideEdges

  friend bool operator==(const CapTables&, const CapTables&) = default;
};

/// The capacitance rules of a style, with the coupling tables completed
/// (ExtTechFinal). The lists hold one entry per type (area), per type and
/// outside type (perimeter, `outside` one type), per ordered pair (overlap:
/// top = s, bottom = t) and per edge rule (sidewall, sideoverlap: inside =
/// s, outside = t), in type order; the reader fills them from `tables`,
/// fixtures set them directly.
struct CapRules {
  std::vector<TypeCap> area;
  std::vector<TypeCap> perimeter;
  std::vector<OverlapRule> overlap;
  std::vector<SidewallRule> sidewall;
  std::vector<SideOverlapRule> sideoverlap;
  std::int32_t fringe_shield_halo = 0;  // `fringeshieldhalo` is deprecated and ignored
  CapTables tables;

  friend bool operator==(const CapRules&, const CapRules&) = default;
};

/// The antenna calculation model of a type (`antenna` line).
enum class AntennaModel : std::uint8_t { none, surface, sidewall };

/// The `model` flags of a style (exts_antennaModel; Q-MISC-2: empty at
/// every style start).
inline constexpr std::uint8_t kAntennaPartial = 0x01;
inline constexpr std::uint8_t kAntennaCumulative = 0x02;
inline constexpr std::uint8_t kAntennaSurface = 0x04;
inline constexpr std::uint8_t kAntennaSidewall = 0x08;

/// One type's antenna rule (`antenna type model ratio [ratio...]`) and the
/// style's `model` flags (Q-MISC-2, Q-MISC-3, Q-MISC-5). ratio_diff holds
/// Magic's two diffusion ratios: [0] = ratioDiffB (the first diffusion
/// value written, +inf for `none`), [1] = ratioDiffA (the second, 0 when
/// absent; scaled like an area coefficient).
struct AntennaRule {
  tech::TileType type = 0;
  AntennaModel model = AntennaModel::none;
  double ratio_gate = 0.0;
  std::vector<double> ratio_diff;

  friend bool operator==(const AntennaRule&, const AntennaRule&) = default;
};

/// The `substrate` declaration of a style.
struct SubstrateDecl {
  tech::TypeMask types;
  tech::TypeMask shield_types;
  std::string node_name;  // with its `$` variable unexpanded (Q-EXTRACT-9)
  std::optional<tech::PlaneId> plane;
  std::optional<tech::TileType> default_type;  // exts_globSubstrateDefaultType

  friend bool operator==(const SubstrateDecl&, const SubstrateDecl&) = default;
};

/// The fringe model a style runs with ("Fringe model default chosen from
/// tech format"): fringe halo for format 34 and later.
enum class FringeModel : std::uint8_t { legacy, fringe_halo };

/// One entry of ExtGetDevInfo's table (ExtTech.c:312-466), read by SPICE,
/// SIM and ANTENNA: per device in ExtGetDevInfo's index order.
///
/// substrate_node is the line's name as written (ExtGetDevInfo returns it
/// so); the netlisters apply their run's `$` substitution with
/// substitute_substrate_node(). drain_class of a device with one terminal
/// mask is the loop index ExtGetDevInfo leaves behind: the source class, or
/// the number of classes when the source has none (magic-compat ON; -1
/// with it OFF).
struct DeviceInfo {
  std::string model;
  tech::TileType type = 0;
  std::int32_t source_class = -1;     // resistance class of the source terminal, -1 = none
  std::int32_t drain_class = -1;      // of the drain terminal
  std::int32_t substrate_class = -1;  // of the substrate terminal
  std::string substrate_node;         // default substrate node, `$` variable unexpanded

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
  std::int32_t side_halo = 0;      // exts_sideCoupleHalo (milli-units until micron_convert)
  FringeModel fringe_model = FringeModel::fringe_halo;  // the tech format's default
  bool magic_compat = true;        // the magic-compat setting the style was read with
  bool converted = false;          // micron_convert() has run
  /// Q-EXTRACT-19 with magic-compat OFF: the step and halo kept fractional
  /// (tech units, then lambda, then internal units); step_size and
  /// side_halo are their rounded values after micron_convert().
  double step_exact = 0.0;
  double side_halo_exact = 0.0;

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
  // Per type (index = tech::TileType): exts_typeToResistClass (-1 = none),
  // exts_sheetResist, exts_cornerChop, exts_viaResist, and
  // exts_typesResistChanged (empty until the reader fills it).
  std::array<std::int32_t, tech::kMaxTileTypes> resist_class = filled(std::int32_t{-1});
  std::array<std::int32_t, tech::kMaxTileTypes> sheet_resistance{};
  std::array<float, tech::kMaxTileTypes> corner_chop = filled(1.0F);
  std::array<std::int32_t, tech::kMaxTileTypes> via_resistance{};
  std::vector<tech::TypeMask> resist_changed;
  CapRules caps;
  std::vector<DevicePattern> devices;
  tech::TypeMask device_types;            // exts_deviceMask
  tech::TypeMask substrate_device_types;  // exts_subsDevTypes
  SubstrateDecl substrate;
  tech::TypeMask non_electrical;
  tech::TypeMask active;
  std::vector<AntennaRule> antenna;
  std::uint8_t antenna_model = 0;  // kAntenna* flags of the `model` lines
  tech::TypeMask tiedown;

  /// ExtGetDevInfo's table (EXTTECH fills it while loading, tasks.md 4.5).
  [[nodiscard]] std::span<const DeviceInfo> device_info() const noexcept { return device_info_; }
  void set_device_info(std::vector<DeviceInfo> table) { device_info_ = std::move(table); }

  /// The device lines that make records for `type`, in the order Magic
  /// tries them (exts_device[type]: the line declared last first).
  [[nodiscard]] std::vector<const DevicePattern*> records_for(tech::TileType type) const;

  /// StyleView's four queries (style_view.hpp). gate_types() is
  /// ExtGetGateTypesMask: the types of fet, mosfet, asymmetric and
  /// msubcircuit lines.
  [[nodiscard]] tech::TypeMask gate_types() const;
  [[nodiscard]] tech::TypeMask diff_types() const { return tiedown; }
  [[nodiscard]] tech::TypeMask active_types() const { return active; }
  [[nodiscard]] tech::TypeMask substrate_types() const { return substrate.types; }

  /// ExtTechScale(scalen = n, scaled = d), in Magic's argument order: every
  /// distance scaled by d/n, units_per_lambda by n/d and areas by (n/d)^2
  /// (Q-EXTRACT-3, Q-EXTRACT-19, Q-EXTRACT-21, Q-EXTRACT-25). Unchanged
  /// when n or d is not positive or n == d.
  [[nodiscard]] ExtStyle rescale(std::int32_t n, std::int32_t d, const compat::CompatSettings& compat) const;
  /// The conversion half of extTechFinalStyle with the cifoutput scale of
  /// `style` at the unscaled grid (nullptr: no output style, dscale 1;
  /// Q-EXTRACT-3, Q-EXTRACT-19, Q-EXTRACT-21), in the mode the style was
  /// read with. Skipped after `noplaneordering` (Magic returns early) and
  /// when already converted. The second form reports the zero-step reset.
  [[nodiscard]] ExtStyle micron_convert(const mask::OutputStyle* style) const;
  [[nodiscard]] ExtStyle micron_convert(const mask::OutputStyle* style, diag::DiagnosticSink& sink) const;

  friend bool operator==(const ExtStyle&, const ExtStyle&) = default;

 private:
  template <typename T>
  static constexpr std::array<T, tech::kMaxTileTypes> filled(T value) noexcept {
    std::array<T, tech::kMaxTileTypes> out{};
    out.fill(value);
    return out;
  }

  std::vector<DeviceInfo> device_info_;
};

namespace detail {
struct StyleCache;
}

/// The style table of a technology: names in declaration order, variants
/// expanded. load_styles() parses every style of the section once and
/// keeps its messages, so a selection never re-reads the technology.
class StyleTable {
 public:
  StyleTable() = default;
  explicit StyleTable(std::vector<std::string> names) : names_(std::move(names)) {}

  [[nodiscard]] std::span<const std::string> names() const noexcept { return names_; }
  /// The parsed style of that exact name (before micron_convert); nullptr
  /// when the table has no such style. With magic-compat ON a type that an
  /// earlier selection mentioned stays electrical, as Magic's
  /// allExtractTypes accumulates across ExtLoadStyle calls.
  [[nodiscard]] std::shared_ptr<const ExtStyle> select(std::string_view name) const;
  /// The same, reporting the messages Magic prints when ExtLoadStyle
  /// re-reads that style (its line errors and the non-electrical list).
  [[nodiscard]] std::shared_ptr<const ExtStyle> select(std::string_view name, diag::DiagnosticSink& sink) const;

 private:
  friend StyleTable load_styles(const tech::RawSection& section, const tech::Technology& technology,
                                diag::DiagnosticSink& sink, const compat::CompatSettings& compat);

  std::vector<std::string> names_;
  std::shared_ptr<detail::StyleCache> cache_;
};

/// Reads the technology's `extract` section (ExtTechLine, ExtTechFinal):
/// the style names and every style, parsed. Reports what Magic prints at
/// tech load: the first style's line errors, its plane-order warning and
/// the non-electrical list. A section without a `style` line gives the
/// style "default".
[[nodiscard]] StyleTable load_styles(const tech::RawSection& section, const tech::Technology& technology,
                                     diag::DiagnosticSink& sink, const compat::CompatSettings& compat);

/// DBScaleValue(v, d, n) for one ExtTechScale distance (step, halo, the
/// `extract halo` / `stepsize` overrides): v * d / n, rounded toward zero
/// for a positive product and toward -inf for a negative one.
[[nodiscard]] std::int32_t scale_distance(std::int32_t value, std::int32_t n, std::int32_t d) noexcept;

/// The `$` substitution the netlisters apply to a device's substrate node
/// name (ext2spice.c:952-960): a name starting with `$` becomes the value
/// of the variable named by the rest when `lookup` knows it; otherwise the
/// name is returned unchanged.
[[nodiscard]] std::string substitute_substrate_node(
    std::string_view name, const std::function<std::optional<std::string>(std::string_view)>& lookup);

}  // namespace hefesto::extract
