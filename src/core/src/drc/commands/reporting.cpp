// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DRC reporting: why-text substitution, the ignore list, error counts,
// `drc why` and the `drc find` loop (drc-commands; openspec/changes/m3-drc
// design.md D9). Ported from Magic 8.3.694 (fc5daa9e):
//   drc/DRCmain.c     drcSubstitute, drcPrintError, drcListError,
//                     drcListallError, DRCWhy, DRCWhyAll, drcWhyFunc,
//                     drcWhyAllFunc, DRCCount, drcCountFunc, DRCFind,
//                     drcFindFunc
//   dbwind/DBWprocs.c dbwPrintValue0 (the mask-unit branch)
//   commands/CmdCD.c  CmdDrc `find` loop and `ignore` scan
// Copyright (C) 1985, 1990 Regents of the University of California
// (Magic's license; see THIRD_PARTY_NOTICES.md).
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/commands/reporting.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>

#include "db/command_util.hpp"
#include "hefesto/connect/tree.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/editor/coordinate_units.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::drc {

namespace {

thread_local RecheckFn t_recheck;  // the ScopedRecheck in force

// --- why-text substitution ----------------------------------------------------

constexpr std::size_t kMaxSubstituted = 19;  // snprintf(wptr, 20, "%s", value)

// dbwValueFormat: trailing zeros, then a trailing point, removed.
std::string trimmed(double v, int places) {
  std::array<char, 64> buf{};
  std::snprintf(buf.data(), buf.size(), "%.*f", places, v);
  std::string s = buf.data();
  while (s.size() > 1 && s.back() == '0') {
    s.pop_back();
  }
  if (s.size() > 1 && s.back() == '.') {
    s.pop_back();
  }
  return s;
}

std::string suffixed(std::string text, const cmd::UnitsState& units, std::string_view suffix) {
  if (units.print) {
    text += suffix;
  }
  return text;
}

// CIFGetScale(100) / CIFGetOutputScale(1000): mask units to internal units.
float mask_to_internal(const cmd::CoordinateUnits& scale) {
  if (!scale.cif_scale) {
    return 1.0F;
  }
  const auto [s, e] = *scale.cif_scale;
  const auto cif_scale = static_cast<float>(1.0 / static_cast<double>(static_cast<float>(e * 100)));
  const float output_scale = static_cast<float>(10 * s) / static_cast<float>(e * 1000);
  return cif_scale / output_scale;
}

float output_scale(const cmd::CoordinateUnits& scale) {
  if (!scale.cif_scale) {
    return 1.0F;
  }
  const auto [s, e] = *scale.cif_scale;
  return static_cast<float>(10 * s) / static_cast<float>(e * 1000);
}

// The factor applied after the mask-unit conversion, per unit (lambda,
// microns, user grid), and the suffixes of a length and an area.
struct UnitScale {
  float factor = 1.0F;       // lambda or microns per internal unit; the grid pitch(es)
  float area_factor = 1.0F;  // user grid: the pitch product
  bool divide = false;       // user grid: value / pitch
  std::string_view suffix;
  std::string_view area_suffix;
};

std::optional<UnitScale> unit_scale(const cmd::UnitsState& units, const cmd::CoordinateUnits& scale) {
  switch (units.unit) {
    case cmd::InputUnit::lambda: {
      const float l = static_cast<float>(scale.lambda_per) / static_cast<float>(scale.internal_per);
      return UnitScale{l, l, false, "l", "l^2"};
    }
    case cmd::InputUnit::microns:
      return UnitScale{output_scale(scale), output_scale(scale), false, "um", "um^2"};
    case cmd::InputUnit::user: {
      if (!scale.grid) {
        return std::nullopt;
      }
      const auto& g = *scale.grid;
      const auto px = static_cast<float>(g[2] - g[0]);
      return UnitScale{px, px * static_cast<float>(g[3] - g[1]), true, "gx", "gx*gy"};
    }
    case cmd::InputUnit::internal:
      return UnitScale{1.0F, 1.0F, false, "i", "i^2"};
    case cmd::InputUnit::default_units:
      break;
  }
  return std::nullopt;
}

// dbwPrintValue0 with is_cif: `value` is in mask units (centimicrons,
// nanometers or angstroms of the output style).
std::string print_mask_value(std::int64_t value, const cmd::UnitsState& units, const cmd::CoordinateUnits& scale,
                             bool area) {
  const float dscale = mask_to_internal(scale);
  const std::optional<UnitScale> u = unit_scale(units, scale);
  if (!u) {
    return std::to_string(value);
  }
  const auto v = static_cast<float>(value);
  float f = 0.0F;
  if (units.unit == cmd::InputUnit::internal) {
    f = area ? v * dscale * dscale : v * dscale;
  } else if (u->divide) {
    f = area ? v * dscale / u->area_factor * dscale : v * dscale / u->factor;
  } else {
    f = area ? v * u->factor * u->factor * dscale * dscale : v * u->factor * dscale;
  }
  return suffixed(trimmed(static_cast<double>(f), area ? 6 : 3), units, area ? u->area_suffix : u->suffix);
}

std::string substituted_value(const Rule& rule, char escape, const cmd::UnitsState& units,
                              const cmd::CoordinateUnits& scale) {
  const bool area = escape == 'a';
  const std::int64_t value = escape == 'd' ? rule.dist : rule.cdist;
  std::string text = rule.has(flags::kCifRule)
                         ? print_mask_value(value, units, scale, area)
                         : cmd::format_value(value, units, scale, area ? cmd::ValueKind::area : cmd::ValueKind::length,
                                             cmd::Axis::x);
  if (text.size() > kMaxSubstituted) {
    text.resize(kMaxSubstituted);
  }
  return text;
}

// --- hierarchy walking (DBCellSrArea) ---------------------------------------------

/// Calls f(element_transform) for every array element of `use` whose
/// bounding box overlaps `area` (parent coordinates), y outer, x inner.
template <typename F>
void for_each_element(const db::CellUse& use, const db::Rect& area, F&& f) {
  if (use.def == nullptr || !db::overlap(use.bbox, area)) {
    return;
  }
  const db::ArrayInfo& a = use.array;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      const db::Transform et = db::element_transform(use, x, y);
      if (db::overlap(db::apply(et, use.def->bbox()), area)) {
        f(et);
      }
    }
  }
}

tech::TypeMask all_but_space() {
  tech::TypeMask mask = tech::TypeMask::all();
  mask.clear_type(tech::kSpace);
  return mask;
}

std::size_t error_tile_count(const db::CellDef& def) {
  std::size_t count = 0;
  if (def.plane_count() > tech::kPlaneDrcError) {
    def.plane(tech::kPlaneDrcError).enumerate(def.bbox(), all_but_space(), [&](const db::TileView&) {
      ++count;
      return true;
    });
  }
  return count;
}

// drcCountFunc: each definition once (the first visit decides the descent).
void count_in(const db::CellDef& def, const db::Rect& area, bool descend,
              std::map<const db::CellDef*, std::size_t>& counts) {
  if (counts.contains(&def)) {
    return;  // seen before: neither counted nor searched again
  }
  counts.emplace(&def, error_tile_count(def));
  if (!descend || !def.flags().available) {
    return;
  }
  for (const db::CellUse* use : db::uses_in_search_order(def)) {
    for_each_element(*use, area, [&](const db::Transform& et) {
      count_in(*use->def, connect::to_local(et, area), true, counts);
    });
  }
}

// --- drc why ------------------------------------------------------------------------

/// DRCWhy / DRCWhyAll state: DRCErrorList (per why index: 0 not yet
/// reported, n reported n times, -1 ignored), DRCErrorCount, the groups.
class WhyCollector {
 public:
  WhyCollector(editor::EditorState& editor, const WhyOptions& options) : editor_(editor), options_(options) {
    if (!options_.recheck) {
      options_.recheck = t_recheck ? t_recheck : RecheckFn{engine_recheck};
    }
    init_error_list();
  }

  /// drcWhyFunc / drcWhyAllFunc: the recheck of `area` in `def`.
  void check(const db::CellDef& def, const db::Rect& area, const db::Transform& to_root) {
    CollectingErrorSink sink;
    sink.set_statistics(&editor_.drc.statistics);
    options_.recheck(editor_, def, area, sink);
    for (const CollectingErrorSink::Error& e : sink.errors()) {
      report(def, area, to_root, e.area, e.rule);
    }
  }

  [[nodiscard]] std::vector<WhyGroup> take() { return std::move(groups_); }

 private:
  void init_error_list() {
    const std::size_t size = editor_.drc.rules->why().size() + 1;
    const IgnoreList& filter = options_.filter != nullptr ? *options_.filter : editor_.drc.ignore;
    error_list_.assign(size, options_.only_filtered ? -1 : 0);
    for (const WhyIndex why : filter.items()) {
      if (why < size) {
        error_list_[why] = options_.only_filtered ? 0 : -1;
      }
    }
  }

  // drcPrintError / drcListError / drcListallError.
  void report(const db::CellDef& def, const db::Rect& area, const db::Transform& to_root, const db::Rect& rect,
              const Rule& rule) {
    if (!db::overlap(area, rect)) {
      return;
    }
    const std::size_t saved = error_count_;
    if (rule.why == why::kInSubcell) {
      resolve_in_subcells(def, rect, to_root);
    }
    if (saved != error_count_) {
      return;
    }
    const db::Rect root_rect = db::apply(to_root, rect);
    if (options_.all) {
      record_all(rule, root_rect);
    } else {
      record_distinct(rule, root_rect);
    }
  }

  // DBTreeSrCells(xMask 0) from `def` over `rect`: every element of every
  // use below, depth first, rechecked over `rect` in its coordinates.
  void resolve_in_subcells(const db::CellDef& def, const db::Rect& rect, const db::Transform& to_root) {
    for (const db::CellUse* use : db::uses_in_search_order(def)) {
      for_each_element(*use, rect, [&](const db::Transform& et) {
        const db::Rect local = connect::to_local(et, rect);
        const db::Transform child_to_root = db::compose(to_root, et);
        check(*use->def, local, child_to_root);
        resolve_in_subcells(*use->def, local, child_to_root);
      });
    }
  }

  void record_distinct(const Rule& rule, const db::Rect& root_rect) {
    if (rule.why >= error_list_.size()) {
      error_list_.resize(rule.why + std::size_t{1}, 0);
    }
    int& seen = error_list_[rule.why];
    if (seen < 0) {
      return;
    }
    if (seen == 0) {
      groups_.push_back(WhyGroup{why_text(editor_, rule), rule.why, {}});
    }
    ++seen;
    ++error_count_;
    const auto group = std::find_if(groups_.begin(), groups_.end(), [&](const WhyGroup& g) { return g.why == rule.why; });
    group->areas.push_back(root_rect);
  }

  void record_all(const Rule& rule, const db::Rect& root_rect) {
    ++error_count_;
    std::string text = why_text(editor_, rule);
    const auto [it, added] = by_text_.emplace(text, groups_.size());
    if (added) {
      groups_.push_back(WhyGroup{std::move(text), rule.why, {}});
    }
    groups_[it->second].areas.push_back(root_rect);
  }

  editor::EditorState& editor_;
  WhyOptions options_;
  std::vector<int> error_list_;
  std::size_t error_count_ = 0;
  std::vector<WhyGroup> groups_;
  std::map<std::string, std::size_t> by_text_;
};

// --- drc find ---------------------------------------------------------------------

/// DRCFind's result: the index (> 0) and area of the error found, 0
/// without errors, -1 when there are fewer.
struct Located {
  std::int64_t result = 0;
  db::Rect area;
};

Located locate(const std::vector<db::Rect>& tiles, std::int64_t index) {
  if (tiles.empty()) {
    return {};
  }
  if (index < 1 || static_cast<std::size_t>(index) > tiles.size()) {
    return Located{-1, {}};
  }
  return Located{index, tiles[static_cast<std::size_t>(index - 1)]};
}

/// The state of one `drc find` loop.
struct FindSearch {
  std::int64_t nth = 0;      // drc_nth
  bool incremental = false;  // no index given: step (and wrap)
  IgnoreList searched;       // `drc find <text>`: the matching why indices
  WhyOptions options;
};

/// DRCFind of error `nth`; a stepping search past the end wraps to the first.
Located next_area(const std::vector<db::Rect>& tiles, FindSearch& search) {
  const Located found = locate(tiles, search.nth);
  if (!search.incremental || found.result >= 0) {
    return found;
  }
  search.nth = 1;
  return locate(tiles, search.nth);
}

/// CmdDrc's FIND loop: try error `nth`, report its whys, step past areas
/// that report nothing until one does or the search returns to its start.
FindResult run_search(editor::EditorState& editor, const db::CellDef& root, FindSearch& search) {
  const std::vector<db::Rect> tiles = error_tiles(root);
  FindResult out;
  std::int64_t start = -1;
  for (;;) {
    const Located found = next_area(tiles, search);
    if (start > 0 && found.result == start) {
      out.status = FindStatus::wrapped;
      return out;
    }
    if (found.result <= 0) {
      out.status = found.result < 0 ? FindStatus::out_of_range : FindStatus::no_errors;
      return out;
    }
    out.attempts.push_back(
        FindAttempt{found.result, found.area, why_errors(editor, root, found.area, search.options)});
    if (!out.attempts.back().why.empty()) {
      out.status = FindStatus::found;
      return out;
    }
    ++search.nth;
    start = start < 0 ? found.result : start;
  }
}

}  // namespace

// --- public functions ---------------------------------------------------------------

std::string why_text(const RuleSet& rules, const Rule& rule, const cmd::UnitsState& units,
                     const cmd::CoordinateUnits& scale) {
  const std::string_view text = rules.why().text(rule.why);
  if (text.find('%') == std::string_view::npos) {
    return std::string{text};
  }
  // Default units print microns with the unit suffix (backwards compatibility).
  const cmd::UnitsState shown =
      units.unit == cmd::InputUnit::default_units ? cmd::UnitsState{cmd::InputUnit::microns, true} : units;
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%') {
      out += text[i];
      continue;
    }
    const char escape = i + 1 < text.size() ? text[i + 1] : '\0';
    if (escape == 'd' || escape == 'c' || escape == 'a') {
      out += substituted_value(rule, escape, shown, scale);
      ++i;
    } else {
      out += '%';  // Q-DRC-16: any other escape is copied literally, a trailing % as is
      if (escape != '\0') {
        out += escape;
        ++i;
      }
    }
  }
  return out;
}

std::string why_text(const editor::EditorState& editor, const Rule& rule) {
  const bool compat = editor.drc.compat_settings().magic_compat;
  return why_text(*editor.drc.rules, rule, editor.units, editor::coordinate_units(editor, compat));
}

bool IgnoreList::contains(WhyIndex why) const noexcept {
  return std::find(items_.begin(), items_.end(), why) != items_.end();
}

IgnoreList matching_whys(const WhyTable& why, std::string_view text, bool magic_compat) {
  IgnoreList out;
  const std::size_t last = magic_compat ? why.size() - 1 : why.size();
  for (std::size_t i = 1; i <= last; ++i) {
    const auto index = static_cast<WhyIndex>(i);
    if (why.text(index).find(text) != std::string_view::npos) {
      out.add(index);
    }
  }
  return out;
}

std::vector<CellErrorCount> count_errors(const db::CellDef& root, const db::Rect& area, bool recurse) {
  std::vector<CellErrorCount> out;
  if (!root.flags().available) {
    return out;
  }
  std::map<const db::CellDef*, std::size_t> counts;
  count_in(root, area, recurse, counts);
  for (const auto& [def, count] : counts) {
    if (count > 0) {
      out.push_back(CellErrorCount{def, count});
    }
  }
  std::sort(out.begin(), out.end(),
            [](const CellErrorCount& a, const CellErrorCount& b) { return a.cell->name() < b.cell->name(); });
  return out;
}

std::vector<db::Rect> error_tiles(const db::CellDef& cell) {
  std::vector<db::Rect> out;
  if (cell.plane_count() > tech::kPlaneDrcError) {
    cell.plane(tech::kPlaneDrcError).enumerate(cell.bbox(), all_but_space(), [&](const db::TileView& t) {
      out.push_back(t.area);
      return true;
    });
  }
  return out;
}

ScopedRecheck::ScopedRecheck(RecheckFn recheck) : previous_(std::move(t_recheck)) {
  t_recheck = std::move(recheck);
}

ScopedRecheck::~ScopedRecheck() {
  t_recheck = std::move(previous_);
}

void engine_recheck(editor::EditorState& editor, const db::CellDef& cell, const db::Rect& area, ErrorSink& sink) {
  (void)interaction_check(editor, cell, area, area, sink);
}

std::vector<WhyGroup> why_errors(editor::EditorState& editor, const db::CellDef& root, const db::Rect& area,
                                 const WhyOptions& options) {
  WhyCollector collector(editor, options);
  collector.check(root, area, db::kIdentity);
  return collector.take();
}

FindResult find_error(editor::EditorState& editor, const db::CellDef& root, std::optional<std::string_view> argument,
                      const RecheckFn& recheck) {
  DrcSession& drc = editor.drc;
  const bool compat = drc.compat_settings().magic_compat;
  FindSearch search;
  search.nth = drc.find_counter.value_or(compat ? 1 : 0);  // Q-DRC-15
  search.options.recheck = recheck;
  if (argument && db::cmdutil::str_is_int(*argument)) {
    search.nth = std::max<std::int64_t>(db::cmdutil::c_atoi(*argument), 1);
  } else {
    search.incremental = true;
    ++search.nth;
    if (argument) {
      search.searched = matching_whys(drc.rules->why(), *argument, compat);
      search.options.only_filtered = true;
    }
  }
  if (search.options.only_filtered) {
    search.options.filter = &search.searched;
  }
  FindResult out = run_search(editor, root, search);
  drc.find_counter = search.nth;
  return out;
}

}  // namespace hefesto::drc
