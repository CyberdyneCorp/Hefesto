// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::RuleSet, RuleSetBuilder, WhyTable and CheckStatistics
// (openspec/changes/m3-drc/design.md D4/D5).
// (Magic ref: drc/DRCtech.c drcWhyCreate, DRCTechStyleInit (predefined why
// texts, DRCPaintTable initialisation))
//
// Owner: INTEGRATION (shared).

#include "hefesto/drc/types.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace hefesto::drc {

namespace {

constexpr std::array<std::string_view, 5> kPredefinedWhy = {
    "This layer can't abut or partially overlap between array elements",
    "Can't overlap those layers",
    "This layer can't abut or partially overlap between subcells",
    "See error definition in the subcell",
    "This position does not align with the manufacturing grid",
};

const std::array<db::PaintRow, tech::kMaxTileTypes>& identity_rows() {
  static const auto rows = [] {
    std::array<db::PaintRow, tech::kMaxTileTypes> out{};
    for (std::size_t t = 0; t < out.size(); ++t) {
      out[t] = identity_paint_row(static_cast<tech::TileType>(t));
    }
    return out;
  }();
  return rows;
}

}  // namespace

db::PaintRow identity_paint_row(tech::TileType paint) noexcept {
  db::PaintRow row{};
  row.fill(paint);
  return row;
}

// --- WhyTable --------------------------------------------------------------

WhyTable::WhyTable() : texts_(kPredefinedWhy.begin(), kPredefinedWhy.end()) {}

WhyIndex WhyTable::intern(std::string_view text) {
  if (const WhyIndex found = find(text); found != 0) {
    return found;
  }
  texts_.emplace_back(text);
  return static_cast<WhyIndex>(texts_.size());
}

std::string_view WhyTable::text(WhyIndex index) const noexcept {
  if (index == 0 || index > texts_.size()) {
    return {};
  }
  return texts_[index - 1];
}

WhyIndex WhyTable::find(std::string_view text) const noexcept {
  const auto it = std::find(texts_.begin(), texts_.end(), text);
  return it == texts_.end() ? 0 : static_cast<WhyIndex>(it - texts_.begin() + 1);
}

// --- RuleSet ---------------------------------------------------------------

RuleSet::RuleSet(std::size_t type_count) : type_count_(type_count) {}
RuleSet::~RuleSet() = default;
RuleSet::RuleSet(const RuleSet&) = default;
RuleSet& RuleSet::operator=(const RuleSet&) = default;
RuleSet::RuleSet(RuleSet&&) noexcept = default;
RuleSet& RuleSet::operator=(RuleSet&&) noexcept = default;

std::span<const Rule> RuleSet::rules(tech::TileType a, tech::TileType b) const noexcept {
  if (offsets_.empty() || a >= type_count_ || b >= type_count_) {
    return {};
  }
  const std::size_t pair = std::size_t{a} * type_count_ + b;
  return std::span<const Rule>{rules_}.subspan(offsets_[pair], offsets_[pair + 1] - offsets_[pair]);
}

const db::PaintRow& RuleSet::paint_row(tech::PlaneId plane, tech::TileType paint) const noexcept {
  const std::size_t index = std::size_t{plane} * tech::kMaxTileTypes + paint;
  if (paint >= tech::kMaxTileTypes || index >= paint_rows_.size()) {
    return identity_rows()[paint < tech::kMaxTileTypes ? paint : 0];
  }
  return paint_rows_[index];
}

// --- RuleSetBuilder -----------------------------------------------------------

RuleSetBuilder::RuleSetBuilder(std::size_t type_count)
    : type_count_(type_count), set_(type_count) {}

RuleSetBuilder& RuleSetBuilder::style_name(std::string name) {
  set_.style_name_ = std::move(name);
  return *this;
}

RuleSetBuilder& RuleSetBuilder::add(tech::TileType a, tech::TileType b, const Rule& rule) {
  if (a < type_count_ && b < type_count_) {
    pairs_[std::size_t{a} * type_count_ + b].push_back(rule);
  }
  return *this;
}

RuleSetBuilder& RuleSetBuilder::exception_names(std::vector<std::string> names) {
  set_.exception_names_ = std::move(names);
  return *this;
}

RuleSetBuilder& RuleSetBuilder::halo(const Halo& halo) {
  set_.halo_ = halo;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::step_size(std::int32_t step) {
  set_.step_size_ = step;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::options(const StyleOptions& options) {
  set_.options_ = options;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::scale(const ScaleFactor& scale) {
  set_.scale_ = scale;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::paint_row(tech::PlaneId plane, tech::TileType paint, const db::PaintRow& row) {
  if (paint >= tech::kMaxTileTypes) {
    return *this;
  }
  const std::size_t needed = (std::size_t{plane} + 1) * tech::kMaxTileTypes;
  while (set_.paint_rows_.size() < needed) {
    set_.paint_rows_.push_back(identity_paint_row(static_cast<tech::TileType>(set_.paint_rows_.size() %
                                                                              tech::kMaxTileTypes)));
  }
  set_.paint_rows_[std::size_t{plane} * tech::kMaxTileTypes + paint] = row;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::exact_overlap_types(const tech::TypeMask& types) {
  set_.exact_overlap_ = types;
  return *this;
}

RuleSetBuilder& RuleSetBuilder::details(std::shared_ptr<const StyleDetails> details) {
  set_.details_ = std::move(details);
  return *this;
}

std::shared_ptr<const RuleSet> RuleSetBuilder::build() {
  RuleSet out = std::move(set_);
  out.why_ = std::move(why_);
  const std::size_t pair_count = type_count_ * type_count_;
  out.offsets_.assign(pair_count + 1, 0);
  std::size_t next_pair = 0;  // offsets_[p] is set for every p < next_pair
  for (const auto& [pair, list] : pairs_) {
    for (; next_pair <= pair; ++next_pair) {
      out.offsets_[next_pair] = static_cast<std::uint32_t>(out.rules_.size());
    }
    out.rules_.insert(out.rules_.end(), list.begin(), list.end());
  }
  for (; next_pair <= pair_count; ++next_pair) {
    out.offsets_[next_pair] = static_cast<std::uint32_t>(out.rules_.size());
  }
  pairs_.clear();
  set_ = RuleSet{type_count_};
  why_ = WhyTable{};
  return std::make_shared<const RuleSet>(std::move(out));
}

// --- CheckStatistics ------------------------------------------------------------

CheckStatistics& CheckStatistics::operator+=(const CheckStatistics& other) noexcept {
  squares += other.squares;
  tiles += other.tiles;
  edges += other.edges;
  rules += other.rules;
  slow += other.slow;
  interactions += other.interactions;
  interaction_tiles += other.interaction_tiles;
  cif_tiles += other.cif_tiles;
  array_tiles += other.array_tiles;
  return *this;
}

}  // namespace hefesto::drc
