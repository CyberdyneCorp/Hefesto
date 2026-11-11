// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RULES-private: the compiler from one style of the technology's `drc`
// section to a frozen drc::RuleSet (openspec/changes/m3-drc/design.md D5).
// It is a port of drc/DRCtech.c keyword by keyword: the per-pair rule lists
// of the loading style (DRCRulesTbl with drcFindBucket insertion), the
// why-text table, exceptions, the halo, the scale factor, the DRC paint
// table and the mask-layer statements; finish() runs drcTechFinalStyle
// (scaling, halo and step size, illegal-edge pruning, covered-rule
// elimination) and freezes the lists into the CSR table.
//
// The keyword handlers are members defined in keywords_*.cpp; each returns
// the rule's halo contribution in tech-file units (mask-layer rules: in
// internal units), as Magic's handlers return it to DRCTechAddRule.
//
// (Magic ref: drc/DRCtech.c DRCTechStyleInit, DRCTechLine, DRCTechAddRule,
// drcFindBucket, drcAssign, drcWhyCreate, drcExceptionCreate,
// DRCTechFinal, drcTechFinalStyle; drc/DRCcif.c drcCifSetStyle,
// drcCifWidth, drcCifSpacing, drcCifArea, drcCifMaxwidth)
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "drc/rules/style_machine.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/drc/rules/cif_catalog.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/drc/types.hpp"
#include "hefesto/tech/technology.hpp"
#include "tech/build_state.hpp"

namespace hefesto::drc::rules_detail {

/// The per-pair rule lists of a style being loaded (DRCRulesTbl without the
/// dummy head), indexed A * n + B.
class RuleLists {
 public:
  explicit RuleLists(std::size_t type_count) : n_(type_count), lists_(type_count * type_count) {}

  [[nodiscard]] std::size_t type_count() const noexcept { return n_; }
  [[nodiscard]] std::vector<Rule>& at(tech::TileType a, tech::TileType b) { return lists_[a * n_ + b]; }
  [[nodiscard]] const std::vector<Rule>& at(tech::TileType a, tech::TileType b) const { return lists_[a * n_ + b]; }
  [[nodiscard]] std::vector<std::vector<Rule>>& all() noexcept { return lists_; }
  [[nodiscard]] const std::vector<std::vector<Rule>>& all() const noexcept { return lists_; }

  /// drcFindBucket(a, b, key) and insertion of `rules` (a trigger rule
  /// followed by the rule it triggers, or one rule) at that point.
  void insert(tech::TileType a, tech::TileType b, std::int32_t key, std::initializer_list<Rule> rules);
  /// The multi-plane spacing coincidence rule: appended at the end.
  void append(tech::TileType a, tech::TileType b, const Rule& rule) { at(a, b).push_back(rule); }
  /// The rectangle size rule: before the first rule with dist >= key, with
  /// no trigger-pair awareness.
  void insert_plain(tech::TileType a, tech::TileType b, std::int32_t key, const Rule& rule);

 private:
  std::size_t n_;
  std::vector<std::vector<Rule>> lists_;
};

/// The inputs of one style load.
struct CompileInput {
  const tech::Technology& tech;
  const CifCatalog& cif;
  compat::CompatSettings compat;
  diag::DiagnosticSink& sink;
};

/// Adds the `drc` section to every diagnostic located on one of its lines,
/// so the magic rendering prints TechPrintLine's context.
class SectionStamp final : public diag::DiagnosticSink {
 public:
  explicit SectionStamp(diag::DiagnosticSink& inner) : inner_(inner) {}
  void emit(const diag::Diagnostic& diagnostic) override;

 private:
  diag::DiagnosticSink& inner_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

class StyleCompiler {
 public:
  StyleCompiler(const CompileInput& input, StyleMachine machine);

  /// DRCTechLine for one line of the section.
  void line(const tech::RawLine& line);
  /// DRCTechFinal and drcTechFinalStyle, then the frozen table.
  [[nodiscard]] std::shared_ptr<const RuleSet> finish();
  [[nodiscard]] const StyleMachine& machine() const noexcept { return machine_; }

  // ---- shared by the keyword handlers --------------------------------------

  using Tokens = rules_detail::Tokens;

  /// TechError at the current line (DRC-TECH-001, or -002 for warnings).
  void tech_error(std::string text);
  /// A native-only tech-file lint warning at the current line (DRC-LINT-*).
  void lint(std::string_view code, std::string text);
  /// DBTechNoisyNameMask: the types and the plane mask of all of them.
  struct Names {
    tech::TypeMask types;
    tech::PlaneMask planes = 0;
  };
  [[nodiscard]] Names names(std::string_view list);
  /// DBTechNoisyNamePlane: the plane, or -1.
  [[nodiscard]] int plane_named(std::string_view name);

  [[nodiscard]] tech::TileType type_count() const noexcept { return static_cast<tech::TileType>(types_); }
  [[nodiscard]] int plane_count() const noexcept { return static_cast<int>(planes_); }
  /// CoincidentPlanes(mask, pmask).
  [[nodiscard]] tech::PlaneMask coincident(const tech::TypeMask& types, tech::PlaneMask pmask) const;
  /// LowestMaskBit: the lowest plane of `pmask`, or the plane count when empty.
  [[nodiscard]] int lowest(tech::PlaneMask pmask) const noexcept;
  /// DBTypesOnSamePlane(a, b).
  [[nodiscard]] tech::PlaneMask same_plane(tech::TileType a, tech::TileType b) const noexcept {
    return planes_of_[a] & planes_of_[b];
  }
  [[nodiscard]] bool on_plane(tech::TileType t, int plane) const noexcept {
    return plane >= 0 && plane < 64 && (planes_of_[t] & tech::plane_bit(static_cast<tech::PlaneId>(plane))) != 0;
  }
  /// The types of `mask` below the type count, ascending.
  [[nodiscard]] std::vector<tech::TileType> members(const tech::TypeMask& mask) const;
  /// DBPlaneTypes[plane] (empty for an out-of-range plane).
  [[nodiscard]] tech::TypeMask plane_types(int plane) const;
  [[nodiscard]] const tech::Technology& tech() const noexcept { return tech_; }

  /// drcAssign: a rule with the current exception tag; reports bad planes.
  [[nodiscard]] Rule make(std::int32_t dist, const tech::TypeMask& ok, const tech::TypeMask& corner, WhyIndex why,
                          std::int32_t cdist, RuleFlags flags, int check_plane, int edge_plane);
  /// drcCifAssign: no plane check (mask-layer rules use layer numbers).
  [[nodiscard]] Rule make_cif(std::int32_t dist, const tech::TypeMask& ok, const tech::TypeMask& corner, WhyIndex why,
                              std::int32_t cdist, RuleFlags flags, int to_layer) const;
  /// drcWhyCreate.
  WhyIndex why(std::string_view text) { return why_.intern(text); }
  /// argv[index], or "" past the end (Magic reads the NULL argv[argc] there
  /// and crashes; X-2 tier 1).
  [[nodiscard]] static std::string_view arg(Tokens t, std::size_t index) {
    return index < t.size() ? std::string_view{t[index]} : std::string_view{};
  }

  RuleLists& lists() noexcept { return lists_; }

  /// Calls f(i, j, pset) for every pair i != j, i in `left` and j in
  /// `right`, sharing a plane in `mask` (pset = DBTypesOnSamePlane(i, j) &
  /// mask != 0), i-major: Magic's (i, j) loop restricted to the pairs that
  /// can insert rules, so insertions keep Magic's order.
  template <typename F>
  void for_pairs(tech::PlaneMask mask, const tech::TypeMask& left, const tech::TypeMask& right, F&& f) const {
    const std::vector<tech::TileType> lefts = members(left);
    const std::vector<tech::TileType> rights = members(right);
    for (const tech::TileType i : lefts) {
      for (const tech::TileType j : rights) {
        const tech::PlaneMask pset = i == j ? 0 : planes_of_[i] & planes_of_[j] & mask;
        if (pset != 0) {
          f(i, j, pset);
        }
      }
    }
  }

  // ---- keyword handlers (DRCtech.c / DRCcif.c) ------------------------------

  int kw_angles(Tokens t);
  int kw_edge(Tokens t);
  int kw_exact_overlap(Tokens t);
  int kw_exception(Tokens t);
  int kw_exemption(Tokens t);
  int kw_extend(Tokens t);
  int kw_no_overlap(Tokens t);
  int kw_option(Tokens t);
  int kw_overhang(Tokens t);
  int kw_rect_only(Tokens t);
  int kw_spacing(Tokens t);
  int kw_stepsize(Tokens t);
  int kw_surround(Tokens t);
  int kw_width(Tokens t);
  int kw_area(Tokens t);
  int kw_off_grid(Tokens t);
  int kw_maxwidth(Tokens t);
  int kw_cif_style(Tokens t);
  int kw_cif_width(Tokens t);
  int kw_cif_spacing(Tokens t);
  int kw_cif_area(Tokens t);
  int kw_cif_maxwidth(Tokens t);
  int kw_rectangle(Tokens t);

 private:
  struct SpacingArgs;
  struct SurroundArgs;

  void drc_line(Tokens t);
  void plow_line(Tokens t);
  void plow_edge_line(Tokens t);
  void add_rule(Tokens t);
  void scalefactor_line(Tokens t);
  void init_paint_table();
  void final_scale_and_halo();
  [[nodiscard]] Halo final_halo() const;
  void count_rules();
  [[nodiscard]] std::shared_ptr<const RuleSet> freeze();

  // spacing (keywords_spacing.cpp)
  int spacing3(Tokens t);
  [[nodiscard]] bool spacing_args(Tokens t, SpacingArgs& a);
  int multiplane_spacing(SpacingArgs a);
  [[nodiscard]] bool decode_adjacency(SpacingArgs& a);
  void prepare_pair_tests(SpacingArgs& a) const;
  int mask_spacing(const SpacingArgs& a);
  void spacing_set1_side(const SpacingArgs& a, tech::TileType i, tech::TileType j, tech::PlaneMask pmask1,
                         tech::PlaneMask pmask2);
  void spacing_set2_side(const SpacingArgs& a, tech::TileType i, tech::TileType j, tech::PlaneMask pmask1,
                         tech::PlaneMask pmask2);
  void spacing_coincidence(const SpacingArgs& a, tech::TileType i, tech::TileType j, tech::PlaneMask pmask1,
                           tech::PlaneMask pmask2);
  [[nodiscard]] bool multiplane_ok(const tech::TypeMask& set, tech::PlaneMask pmask, std::string_view which);
  [[nodiscard]] RuleFlags derived_flags(tech::TileType edge_type, const tech::TypeMask& edge_set, int edge_plane,
                                        const tech::TypeMask& target_set, int target_plane) const;
  [[nodiscard]] bool is_derived_type(tech::TileType type, const tech::TypeMask& set, int plane) const;

  // surround (keywords_shapes.cpp)
  void surround_base(const SurroundArgs& a);
  void surround_directional(const SurroundArgs& a);
  void surround_exact(const SurroundArgs& a);
  void surround_presence(const SurroundArgs& a);

  // extend and overhang (keywords_basic.cpp)
  /// The rules of a multi-plane extend/overhang between i and j: a trigger
  /// on the edge plane and the rule it guards on `check_plane`, forward
  /// (i, j) then reverse (j, i).
  struct TriggeredRule {
    tech::TypeMask trigger_ok;
    tech::TypeMask ok;
    tech::TypeMask corner;
    std::int32_t distance = 0;
    std::int32_t cdist = 0;
    WhyIndex why = 0;
    int check_plane = 0;
    int edge_plane = 0;
  };
  void insert_triggered(tech::TileType i, tech::TileType j, const TriggeredRule& r);
  /// CoincidentPlanes of `n`, with the "must be on the same plane" error
  /// (`which` set of `keyword`) when they share none.
  [[nodiscard]] tech::PlaneMask one_plane(const Names& n, std::string_view which, std::string_view keyword);

  // edge (keywords_shapes.cpp)
  [[nodiscard]] std::optional<tech::PlaneMask> edge_ok_planes(const Names& ok, const std::string& ok_types,
                                                              tech::PlaneMask edge_planes, int plane_arg);
  void edge_pair_rules(tech::TileType i, tech::TileType j, tech::PlaneMask pset, tech::PlaneMask ok_planes,
                       const Rule& proto, bool fourway);

  // angles and maxwidth (keywords_basic.cpp)
  void angles_45_only(const tech::TypeMask& set, tech::PlaneMask pmask, WhyIndex why, RuleFlags angles);
  void angles_plane_rules(const tech::TypeMask& set, WhyIndex why, RuleFlags angles);
  struct Exclude {
    tech::TypeMask types;
    int plane = -1;  // -1: no exclude types
  };
  [[nodiscard]] std::optional<Exclude> maxwidth_exclude(Tokens t, tech::PlaneMask pmask, RuleFlags bend);

  // cif (keywords_cif.cpp)
  [[nodiscard]] std::optional<std::size_t> cif_layer(std::string_view name) const;
  int cif_missing_style();
  void add_mask_rule(MaskLayerRule rule);

  const tech::Technology& tech_;
  const tech::Technology::Data& data_;
  const CifCatalog& cif_;
  compat::CompatSettings compat_;
  SectionStamp stamp_;
  tech::Reporter reporter_;
  StyleMachine machine_;
  std::size_t types_;
  std::size_t planes_;
  std::vector<tech::PlaneMask> planes_of_;  // DBTypePlaneMaskTbl per type
  std::vector<tech::TileType> residues_;    // DBPlaneToResidue per (contact, plane), kSpace otherwise
  diag::SourceLocation where_;

  RuleLists lists_;
  WhyTable why_;
  std::vector<std::string> exception_names_;
  std::uint8_t exception_ = kNoException;
  std::int32_t scale_n_ = 1;
  std::int32_t scale_d_ = 1;
  std::int32_t step_size_ = 0;
  std::int32_t halo_all_ = 0;   // DRCTechHalo: every contribution (Magic, magic-compat ON)
  std::int32_t halo_tech_ = 0;  // tech-file-unit contributions only
  std::int32_t halo_mask_ = 0;  // mask-layer contributions (internal units)
  StyleOptions options_;
  std::vector<db::PaintRow> paint_;  // [plane * kMaxTileTypes + painted][existing]
  tech::TypeMask exact_overlap_;
  StyleDetails details_;
  const CifStyleInfo* drc_cif_style_ = nullptr;  // drcCifStyle
  bool cif_warned_ = false;                      // beenWarned
};

// ---- finalization helpers (optimize.cpp, scale.cpp) ------------------------

/// drcScaleDown / drcScaleUp on one list. With magic-compat OFF the
/// ceiling of scale_down tests the true remainder instead of Magic's
/// one-byte copy (Q-DRC-1).
void scale_down(std::vector<Rule>& rules, std::int32_t factor, bool magic_compat);
void scale_up(std::vector<Rule>& rules, std::int32_t factor);
/// The illegal-edge pruning of drcTechFinalStyle for list (i, j); returns
/// the number of rules removed.
std::size_t prune_illegal_edges(std::vector<Rule>& rules, tech::TileType i, tech::TileType j,
                                const tech::Technology& tech);
/// The covered-rule elimination of drcTechFinalStyle (Q-DRC-5 per
/// `magic_compat`); returns the number of rules removed.
std::size_t eliminate_covered(std::vector<Rule>& rules, bool magic_compat);

}  // namespace hefesto::drc::rules_detail
