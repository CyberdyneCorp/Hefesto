// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The drc section compiler (see compiler.hpp): line dispatch, the keyword
// table and its usage messages, why texts, exceptions, `option`,
// `stepsize`, `scalefactor`, the DRC paint table and the finalization of a
// style into a frozen RuleSet.
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (DRCTechStyleInit,
// DRCTechLine, drcAssign, DRCTechAddRule, drcException, drcExemption,
// drcExceptionCreate, drcOption, drcStepSize, drcNoOverlap,
// drcExactOverlap, DRCTechFinal, drcTechFinalStyle), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include "drc/rules/compiler.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "drc/rules/text_util.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::drc::rules_detail {

// --- RuleLists -------------------------------------------------------------------

void RuleLists::insert(tech::TileType a, tech::TileType b, std::int32_t key, std::initializer_list<Rule> rules) {
  std::vector<Rule>& list = at(a, b);
  std::size_t pos = 0;
  while (pos < list.size()) {
    if (list[pos].has(flags::kTrigger) && pos + 1 < list.size()) {
      if (list[pos + 1].dist >= key) {
        break;
      }
      pos += 2;  // never split a trigger from the rule it triggers
    } else if (list[pos].dist >= key) {
      break;
    } else {
      ++pos;
    }
  }
  list.insert(list.begin() + static_cast<std::ptrdiff_t>(pos), rules.begin(), rules.end());
}

void RuleLists::insert_plain(tech::TileType a, tech::TileType b, std::int32_t key, const Rule& rule) {
  std::vector<Rule>& list = at(a, b);
  const auto it = std::find_if(list.begin(), list.end(), [&](const Rule& r) { return r.dist >= key; });
  list.insert(it, rule);
}

// --- diagnostics ---------------------------------------------------------------

void SectionStamp::emit(const diag::Diagnostic& diagnostic) {
  const auto* where = std::get_if<diag::SourceLocation>(&diagnostic.location);
  if (where == nullptr || where->line == 0 || diagnostic.related.has_value()) {
    inner_.emit(diagnostic);
    return;
  }
  diag::Diagnostic stamped = diagnostic;
  stamped.related = diag::ObjectRef{diag::ObjectKind::section, "drc"};
  inner_.emit(stamped);
}

void StyleCompiler::tech_error(std::string text) {
  const bool warning = text::starts_with(text, "Warning") || text::starts_with(text, "\"surround_ok\" used");
  diag::Location where;
  if (!where_.file.empty()) {
    where = where_;
  }
  stamp_.emit(diag::make(warning ? "DRC-TECH-002" : "DRC-TECH-001", std::move(text), where));
}

void StyleCompiler::lint(std::string_view code, std::string text) {
  diag::Location where;
  if (!where_.file.empty()) {
    where = where_;
  }
  stamp_.emit(diag::make_native(code, std::move(text), where));
}

// --- construction and helpers ------------------------------------------------------

StyleCompiler::StyleCompiler(const CompileInput& input, StyleMachine machine)
    : tech_(input.tech),
      data_(input.tech.data()),
      cif_(input.cif),
      compat_(input.compat),
      stamp_(input.sink),
      reporter_(stamp_),
      machine_(std::move(machine)),
      types_(input.tech.type_count()),
      planes_(input.tech.plane_count()),
      lists_(input.tech.type_count()) {
  planes_of_.resize(types_);
  residues_.assign(types_ * tech::kMaxPlanes, tech::kSpace);
  for (std::size_t t = 0; t < types_; ++t) {
    const auto type = static_cast<tech::TileType>(t);
    planes_of_[t] = tech_.type_planes(type);
    if (!tech_.is_contact(type)) {
      continue;
    }
    for (std::size_t p = 0; p < planes_; ++p) {
      residues_[t * tech::kMaxPlanes + p] = tech::plane_to_residue(data_, type, static_cast<int>(p));
    }
  }
  init_paint_table();
}

std::vector<tech::TileType> StyleCompiler::members(const tech::TypeMask& mask) const {
  std::vector<tech::TileType> out;
  for (tech::TileType t = 0; t < type_count(); ++t) {
    if (mask.test(t)) {
      out.push_back(t);
    }
  }
  return out;
}

StyleCompiler::Names StyleCompiler::names(std::string_view list) {
  const tech::TypeList parsed = tech::name_mask(data_, list, &reporter_, tech::Noise::error);
  return Names{parsed.types, parsed.planes};
}

int StyleCompiler::plane_named(std::string_view name) {
  return tech::noisy_name_plane(data_, name, &reporter_, tech::Noise::error);
}

tech::PlaneMask StyleCompiler::coincident(const tech::TypeMask& types, tech::PlaneMask pmask) const {
  tech::PlaneMask planes = pmask;
  for (tech::TileType t = tech::kMagnet; t < type_count(); ++t) {  // TT_SELECTBASE
    if (types.test(t)) {
      planes &= planes_of_[t];
    }
  }
  return planes;
}

int StyleCompiler::lowest(tech::PlaneMask pmask) const noexcept {
  if (pmask == 0) {
    return plane_count();
  }
  int plane = 0;
  while ((pmask & 1U) == 0) {
    ++plane;
    pmask >>= 1U;
  }
  return plane;
}

tech::TypeMask StyleCompiler::plane_types(int plane) const {
  return plane >= 0 && plane < plane_count() ? tech_.plane_types(static_cast<tech::PlaneId>(plane)) : tech::TypeMask{};
}

Rule StyleCompiler::make(std::int32_t dist, const tech::TypeMask& ok, const tech::TypeMask& corner, WhyIndex why,
                         std::int32_t cdist, RuleFlags flags, int check_plane, int edge_plane) {
  if (check_plane >= plane_count()) {
    tech_error("Bad plane in DRC assignment.");
  }
  if (edge_plane >= plane_count()) {
    tech_error("Bad edge plane in DRC assignment.");
  }
  Rule rule = make_cif(dist, ok, corner, why, cdist, flags, check_plane);
  rule.edge_plane = static_cast<tech::PlaneId>(std::clamp(edge_plane, 0, 255));
  return rule;
}

Rule StyleCompiler::make_cif(std::int32_t dist, const tech::TypeMask& ok, const tech::TypeMask& corner, WhyIndex why,
                             std::int32_t cdist, RuleFlags flags, int to_layer) const {
  Rule rule;
  rule.dist = dist;
  rule.cdist = cdist;
  rule.ok = ok;
  rule.corner = corner;
  rule.why = why;
  rule.flags = flags;
  rule.check_plane = static_cast<tech::PlaneId>(std::clamp(to_layer, 0, 255));
  rule.edge_plane = 0;
  rule.exception = exception_;
  rule.file_dist = dist;
  rule.file_cdist = cdist;
  return rule;
}

// --- line dispatch (DRCTechLine, DRCTechAddRule) -----------------------------------

void StyleCompiler::scalefactor_line(Tokens t) {
  if (t.size() != 2 && t.size() != 3) {
    tech_error("Wrong number of arguments in " + t[0] + " statement.");
    return;
  }
  const std::int32_t n = text::c_atof_int(t[1]);
  const std::int32_t d = t.size() == 3 ? text::c_atof_int(t[2]) : 1;
  if (n <= 0 || d <= 0) {
    tech_error("Scale factor must be greater than 0.");
    tech_error("Setting scale factor to default value 1.");
    scale_n_ = 1;
    scale_d_ = 1;
    return;
  }
  scale_n_ = n;
  scale_d_ = d;
}

void StyleCompiler::line(const tech::RawLine& line) {
  const Tokens t = line.tokens;
  if (t.empty()) {
    return;
  }
  where_ = diag::SourceLocation{line.file, line.line};
  reporter_.set_location(where_, "drc");
  drc_line(t);
  plow_line(t);
}

void StyleCompiler::plow_line(Tokens t) {
  // The plow module reads the same lines (PlowDRCLine), in every style, and
  // its rule parsers repeat the type-name messages of DBTechNoisyNameMask.
  static constexpr std::array<std::string_view, 4> kPlowKeys = {"edge", "edge4way", "spacing", "width"};
  const int which = text::lookup(t[0], kPlowKeys);
  const std::size_t argc = t.size();
  if (which == 3 && argc == 4) {
    (void)names(t[1]);
  } else if (which == 2 && argc == 6) {
    (void)names(t[1]);
    (void)names(t[2]);
  } else if ((which == 0 || which == 1) && (argc == 8 || argc == 9)) {
    plow_edge_line(t);
  }
}

void StyleCompiler::plow_edge_line(Tokens t) {
  // plowEdgeRule: layers1, layers2, then corner and OK types while the
  // planes agree.
  const Names n1 = names(t[1]);
  const Names n2 = names(t[2]);
  const tech::PlaneMask p1 = coincident(n1.types, n1.planes);
  const tech::PlaneMask p2 = coincident(n2.types, n2.planes);
  if (p1 == 0 || p2 == 0 || p1 != p2) {
    return;
  }
  const Names corner = names(t[5]);
  if (coincident(corner.types, corner.planes) == 0) {
    return;
  }
  if (t.size() == 9 && tech::name_plane(data_, t[8]) < 0) {
    return;
  }
  (void)names(t[4]);
}

void StyleCompiler::drc_line(Tokens t) {
  if (t[0] == "style") {
    if (!machine_.style_line(t)) {
      tech_error("Wrong number of arguments in " + t[0] + " statement.");
    }
    return;
  }
  if (!machine_.admit_rule_line()) {
    return;
  }
  if (t[0] == "scalefactor") {
    scalefactor_line(t);
    return;
  }
  if (text::starts_with(t[0], "variant")) {
    if (!machine_.variant_line(t)) {
      tech_error("Wrong number of arguments in " + t[0] + " statement.");
    }
    return;
  }
  if (machine_.loading()) {
    add_rule(t);
  }
}

namespace {

using Handler = int (StyleCompiler::*)(StyleCompiler::Tokens);

struct RuleKey {
  std::string_view keyword;
  std::size_t min_args;
  std::size_t max_args;
  Handler handler;
  std::string_view usage;
  bool mask_layer;
};

constexpr std::array kRuleKeys = {
    RuleKey{"angles", 4, 4, &StyleCompiler::kw_angles, "layers 45|90 why", false},
    RuleKey{"edge", 8, 10, &StyleCompiler::kw_edge,
            "layers1 layers2 distance okTypes cornerTypes cornerDistance [option] why [plane]", false},
    RuleKey{"edge4way", 8, 10, &StyleCompiler::kw_edge,
            "layers1 layers2 distance okTypes cornerTypes cornerDistance [option] why [plane]", false},
    RuleKey{"exact_overlap", 2, 2, &StyleCompiler::kw_exact_overlap, "layers", false},
    RuleKey{"exception", 2, 2, &StyleCompiler::kw_exception, "name", false},
    RuleKey{"exemption", 2, 2, &StyleCompiler::kw_exemption, "name", false},
    RuleKey{"extend", 5, 6, &StyleCompiler::kw_extend, "layers1 layers2 distance [option] why", false},
    RuleKey{"no_overlap", 3, 3, &StyleCompiler::kw_no_overlap, "layers1 layers2", false},
    RuleKey{"option", 2, 2, &StyleCompiler::kw_option, "option_name option_value", false},
    RuleKey{"overhang", 5, 5, &StyleCompiler::kw_overhang, "layers1 layers2 distance why", false},
    RuleKey{"rect_only", 3, 3, &StyleCompiler::kw_rect_only, "layers why", false},
    RuleKey{"spacing", 6, 7, &StyleCompiler::kw_spacing, "layers1 layers2 separation [layers3] adjacency why", false},
    RuleKey{"stepsize", 2, 2, &StyleCompiler::kw_stepsize, "step_size", false},
    RuleKey{"surround", 6, 7, &StyleCompiler::kw_surround, "layers1 layers2 distance presence why", false},
    RuleKey{"width", 4, 5, &StyleCompiler::kw_width, "layers width why", false},
    RuleKey{"widespacing", 7, 8, &StyleCompiler::kw_spacing, "layers1 width layers2 separation adjacency why", false},
    RuleKey{"area", 5, 5, &StyleCompiler::kw_area, "layers area horizon why", false},
    RuleKey{"off_grid", 4, 4, &StyleCompiler::kw_off_grid, "layers pitch why", false},
    RuleKey{"maxwidth", 4, 6, &StyleCompiler::kw_maxwidth, "layers maxwidth bends why", false},
    RuleKey{"cifstyle", 2, 2, &StyleCompiler::kw_cif_style, "cif_style", false},
    RuleKey{"cifwidth", 4, 4, &StyleCompiler::kw_cif_width, "layers width why", true},
    RuleKey{"cifspacing", 6, 6, &StyleCompiler::kw_cif_spacing, "layers1 layers2 separation adjacency why", true},
    RuleKey{"cifarea", 5, 5, &StyleCompiler::kw_cif_area, "layers area horizon why", true},
    RuleKey{"cifmaxwidth", 5, 5, &StyleCompiler::kw_cif_maxwidth, "layers maxwidth bends why", true},
    RuleKey{"rectangle", 5, 5, &StyleCompiler::kw_rectangle, "layers maxwidth [even|odd|any] why", false},
};

const std::array<std::string_view, kRuleKeys.size()>& rule_keywords() {
  static const auto names = [] {
    std::array<std::string_view, kRuleKeys.size()> out{};
    for (std::size_t i = 0; i < kRuleKeys.size(); ++i) {
      out[i] = kRuleKeys[i].keyword;
    }
    return out;
  }();
  return names;
}

std::string valid_rule_types() {
  std::string out = "Valid rule types are:\n";
  for (std::size_t i = 0; i < kRuleKeys.size(); ++i) {
    out += (i == 0 ? "" : ", ");
    out += kRuleKeys[i].keyword;
  }
  return out + ".";
}

}  // namespace

void StyleCompiler::add_rule(Tokens t) {
  ++details_.rules_specified;
  const int which = text::lookup(t[0], rule_keywords());
  if (which < 0) {
    tech_error("Bad DRC rule type \"" + t[0] + "\"\n" + valid_rule_types());
    return;
  }
  const RuleKey& key = kRuleKeys[static_cast<std::size_t>(which)];
  if (t.size() < key.min_args || t.size() > key.max_args) {
    tech_error("Rule type \"" + std::string{key.keyword} + "\" usage: " + std::string{key.keyword} + " " +
               std::string{key.usage});
    return;
  }
  const int distance = (this->*key.handler)(t);
  if (distance < 0) {
    // DRCTechAddRule returns FALSE: Magic marks the drc section as failed
    // and the technology load reports it (see StyleDetails::line_failed).
    details_.line_failed = true;
    return;
  }
  // DRCTechAddRule folds every contribution into one halo (magic-compat
  // ON); the tech-file and mask-layer parts are also kept apart (Q-DRC-2).
  halo_all_ = std::max(halo_all_, distance);
  if (key.mask_layer) {
    halo_mask_ = std::max(halo_mask_, distance);
  } else {
    halo_tech_ = std::max(halo_tech_, distance);
  }
}

// --- simple statements ---------------------------------------------------------------

namespace {
constexpr std::size_t kMaxExceptions = 126;
}

int StyleCompiler::kw_exception(Tokens t) {
  if (t[1] == "none") {
    exception_ = kNoException;
    return 0;
  }
  const auto found = std::find(exception_names_.begin(), exception_names_.end(), t[1]);
  if (found != exception_names_.end()) {
    exception_ = static_cast<std::uint8_t>(found - exception_names_.begin());
    return 0;
  }
  if (exception_names_.size() >= kMaxExceptions + 1) {
    tech_error("Error:  Too many rule exceptions!  Limit is 126.");
    exception_ = kNoException;
    return 0;
  }
  exception_names_.push_back(t[1]);
  exception_ = static_cast<std::uint8_t>(exception_names_.size() - 1);
  return 0;
}

int StyleCompiler::kw_exemption(Tokens t) {
  (void)kw_exception(t);
  if (exception_ != kNoException) {
    exception_ = static_cast<std::uint8_t>(exception_ | kExemptionBit);
  }
  return 0;
}

int StyleCompiler::kw_option(Tokens t) {
  for (std::size_t i = 1; i < t.size(); ++i) {
    if (t[i] == "wide-width-noninclusive") {
      options_.wide_width_noninclusive = true;
    } else {
      tech_error("Unrecognized DRC option \"" + t[i] + "\" (ignored).");
    }
  }
  return 0;
}

int StyleCompiler::kw_stepsize(Tokens t) {
  step_size_ = text::c_atoi(t[1]);
  if (step_size_ <= 0) {
    tech_error("Step size must be a positive integer.");
    step_size_ = 0;
  } else if (step_size_ < 16) {
    tech_error("Warning: abnormally small DRC step size (" + std::to_string(step_size_) + ")");
  }
  return 0;
}

int StyleCompiler::kw_exact_overlap(Tokens t) {
  exact_overlap_ |= names(t[1]).types;
  return 0;
}

int StyleCompiler::kw_no_overlap(Tokens t) {
  const tech::TypeMask set1 = names(t[1]).types;
  const tech::TypeMask set2 = names(t[2]).types;
  details_.no_overlap.push_back(OverlapRule{set1, set2});
  for (tech::TileType i = 0; i < type_count(); ++i) {
    for (tech::TileType j = 0; j < type_count(); ++j) {
      if (!set1.test(i) || !set2.test(j)) {
        continue;
      }
      for (std::size_t plane = 0; plane < planes_; ++plane) {
        paint_[plane * tech::kMaxTileTypes + j][i] = tech::kErrorS;
        paint_[plane * tech::kMaxTileTypes + i][j] = tech::kErrorS;
      }
    }
  }
  return 0;
}

// --- the DRC paint table (DRCTechStyleInit) -----------------------------------------

namespace {

/// The DRC paint table entry for `painted` over `existing` on `plane`.
tech::TileType drc_paint_entry(const tech::Technology& tech, tech::PlaneId plane, tech::TileType painted,
                               tech::TileType existing) {
  const tech::TileType result = tech.paint_result(plane, existing, painted);
  const auto on_plane = [&](tech::TileType t) { return (tech.type_planes(t) & tech::plane_bit(plane)) != 0; };
  const auto affects = [&](tech::TileType a, tech::TileType b) { return (tech.paint_planes(a) & tech.type_planes(b)) != 0; };
  if (painted == tech::kErrorS || existing == tech::kErrorS) {
    return tech::kErrorS;
  }
  if (painted == tech::kSpace || existing == tech::kSpace || !on_plane(existing) || !affects(painted, existing)) {
    return result;
  }
  const std::size_t user_end = tech.user_type_end();
  if (painted >= user_end ||
      (result >= user_end && tech::find_stacking(tech.data(), painted, existing) == static_cast<int>(result))) {
    return result;
  }
  // DBLayerTypeMaskTbl[t] is {t} for every type.
  if ((result != painted && result != existing) ||
      (result != tech.paint_result(plane, painted, existing) && on_plane(painted) && affects(existing, painted))) {
    return tech::kErrorS;
  }
  return result;
}

}  // namespace

void StyleCompiler::init_paint_table() {
  paint_.assign(planes_ * tech::kMaxTileTypes, db::PaintRow{});
  for (std::size_t plane = 0; plane < planes_; ++plane) {
    for (std::size_t painted = 0; painted < tech::kMaxTileTypes; ++painted) {
      db::PaintRow& row = paint_[plane * tech::kMaxTileTypes + painted];
      row.fill(static_cast<tech::TileType>(painted));
      if (painted >= types_) {
        continue;
      }
      for (std::size_t existing = 0; existing < types_; ++existing) {
        row[existing] = drc_paint_entry(tech_, static_cast<tech::PlaneId>(plane), static_cast<tech::TileType>(painted),
                                        static_cast<tech::TileType>(existing));
      }
    }
  }
}

// --- finalization (DRCTechFinal, drcTechFinalStyle) ------------------------------------

void StyleCompiler::final_scale_and_halo() {
  for (std::vector<Rule>& list : lists_.all()) {
    scale_up(list, scale_d_);
    scale_down(list, scale_n_, compat_.magic_compat);
  }
}

std::shared_ptr<const RuleSet> StyleCompiler::finish() {
  machine_.ensure_default();
  final_scale_and_halo();
  for (tech::TileType i = 0; i < type_count(); ++i) {
    for (tech::TileType j = 0; j < type_count(); ++j) {
      details_.rules_optimized += static_cast<std::uint32_t>(prune_illegal_edges(lists_.at(i, j), i, j, tech_));
    }
  }
  for (tech::TileType i = 0; i < type_count(); ++i) {
    for (tech::TileType j = 0; j < type_count(); ++j) {
      details_.rules_optimized += static_cast<std::uint32_t>(eliminate_covered(lists_.at(i, j), compat_.magic_compat));
    }
  }
  return freeze();
}

namespace {

std::int32_t ceil_div(std::int64_t value, std::int64_t divisor) {
  return text::clamp_int((value + divisor - 1) / divisor);
}

}  // namespace

Halo StyleCompiler::final_halo() const {
  // Q-DRC-2: Magic scales every contribution by D/N with ceiling;
  // magic-compat OFF scales the tech-file part only.
  Halo halo;
  halo.tech_contribution = halo_tech_;
  halo.mask_contribution = halo_mask_;
  const std::int64_t d = scale_d_;
  const std::int64_t n = scale_n_;
  halo.value = compat_.magic_compat ? ceil_div(halo_all_ * d, n) : std::max(ceil_div(halo_tech_ * d, n), halo_mask_);
  return halo;
}

void StyleCompiler::count_rules() {
  details_.plane_count = planes_;
  details_.rules_per_pair_histogram.assign(11, 0);
  for (tech::TileType i = 0; i < type_count(); ++i) {
    for (tech::TileType j = 0; j < type_count(); ++j) {
      const std::size_t count = lists_.at(i, j).size();
      details_.edge_rules += static_cast<std::uint32_t>(count);
      if (same_plane(i, j) == 0) {
        continue;
      }
      if (count <= 10) {
        ++details_.rules_per_pair_histogram[count];
      } else {
        ++details_.rules_per_pair_overflow;
      }
    }
  }
}

std::shared_ptr<const RuleSet> StyleCompiler::freeze() {
  const Halo halo = final_halo();
  std::int32_t step = step_size_;
  if (step == 0) {
    step = halo.value == 0 ? cif_.default_step_size() : 16 * halo.value;
  }
  RuleSetBuilder builder(types_);
  builder.style_name(machine_.current().value_or("default"));
  for (tech::TileType i = 0; i < type_count(); ++i) {
    for (tech::TileType j = 0; j < type_count(); ++j) {
      for (const Rule& rule : lists_.at(i, j)) {
        builder.add(i, j, rule);
      }
    }
  }
  builder.why() = why_;
  builder.exception_names(exception_names_);
  builder.halo(halo).step_size(step).options(options_).scale(ScaleFactor{scale_n_, scale_d_});
  for (std::size_t plane = 0; plane < planes_; ++plane) {
    for (std::size_t painted = 0; painted < types_; ++painted) {
      builder.paint_row(static_cast<tech::PlaneId>(plane), static_cast<tech::TileType>(painted),
                        paint_[plane * tech::kMaxTileTypes + painted]);
    }
  }
  builder.exact_overlap_types(exact_overlap_);
  count_rules();
  builder.details(std::make_shared<const StyleDetails>(details_));
  return builder.build();
}

}  // namespace hefesto::drc::rules_detail
