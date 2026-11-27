// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `property <c> series|serial|parallel`, the deprecated `property <c>
// merge`, and the global `property parallel|series` forms (lvs-setup-file
// "property series and parallel controls"). The merge rules are the
// merge_rule bits of PropertyDef::merge (lvs-device-reduction "Combination
// flags"); "add" and "par" exclude each other within one direction.
//
// Owner: LVSSETUP.

#include <array>

#include "runtime.hpp"

namespace hefesto::lvs::setup {

namespace {

using namespace merge_rule;

constexpr std::uint16_t kSeriesBits = kSeriesAdd | kSeriesPar | kSeriesCritical | kLegacySeriesCritical;
constexpr std::uint16_t kParallelBits = kParallelAdd | kParallelPar | kParallelCritical | kLegacyParallelCritical;

constexpr std::array<std::string_view, 10> kSwitches{"on",  "yes", "true",  "enable",  "allow",
                                                     "off", "no",  "false", "disable", "prohibit"};
constexpr std::array<std::string_view, 4> kCombine{"none", "par", "add", "critical"};
enum Combine : std::size_t { kNone, kPar, kAdd, kCritical };

constexpr std::string_view kBadCombinePair = "Not a {key merge_type} pair list.";

void enable(Cell& cell, bool series, bool on) {
  if (series) {
    cell.flags.series_combinable = on;
  } else {
    cell.flags.no_parallel = !on;
  }
}

std::uint16_t combined(std::uint16_t merge, bool series, std::size_t word) {
  const std::uint16_t bits = series ? kSeriesBits : kParallelBits;
  const std::uint16_t add = series ? kSeriesAdd : kParallelAdd;
  const std::uint16_t par = series ? kSeriesPar : kParallelPar;
  const std::uint16_t critical = series ? kSeriesCritical : kParallelCritical;
  switch (word) {
    case kNone:
      return static_cast<std::uint16_t>(merge & ~bits);
    case kPar:
      // series: `par` replaces the other series settings; parallel: adds.
      return static_cast<std::uint16_t>((series ? (merge & ~bits) : (merge & ~add)) | par);
    case kAdd:
      return static_cast<std::uint16_t>((merge & ~par) | add);
    default:
      return static_cast<std::uint16_t>(merge | critical);
  }
}

/// One `{key combine}` pair on every target; false when malformed.
bool combine_pair(Runtime& runtime, const Resolved& cells, bool series, const std::string& text) {
  auto pair = split_list(text);
  if (!pair || pair.value().size() != 2) {
    return false;
  }
  const auto word = match_keyword(pair.value()[1], kCombine);
  if (!word) {
    return false;
  }
  for (const auto& target : cells.targets) {
    Cell& cell = target.get();
    PropertyDef* def = find_definition(cell, pair.value()[0]);
    if (def == nullptr) {
      report_missing_property(runtime, cell, pair.value()[0]);
      continue;
    }
    def->merge = combined(def->merge, series, *word);
    enable(cell, series, *word != kNone);
  }
  return true;
}

std::optional<std::uint16_t> legacy_merge(std::string_view type) {
  struct Entry {
    std::string_view word;
    std::uint16_t merge;
  };
  static constexpr std::array<Entry, 13> kTable{{
      {"none", 0},
      {"add", kParallelAdd},
      {"add_critical", kParallelAdd | kLegacyParallelCritical},
      {"par", kParallelPar},
      {"parallel", kParallelPar},
      {"par_critical", kParallelPar | kLegacyParallelCritical},
      {"parallel_critical", kParallelPar | kLegacyParallelCritical},
      {"ser", kSeriesAdd},
      {"serial", kSeriesAdd},
      {"series", kSeriesAdd},
      {"ser_critical", kSeriesAdd | kLegacySeriesCritical},
      {"serial_critical", kSeriesAdd | kLegacySeriesCritical},
      {"series_critical", kSeriesAdd | kLegacySeriesCritical},
  }};
  for (const auto& entry : kTable) {
    if (entry.word == type) {
      return entry.merge;
    }
  }
  return std::nullopt;
}

}  // namespace

cmd::CommandResult combine_option(Runtime& runtime, const Resolved& cells, bool series,
                                  std::span<const std::string> args) {
  if (args.empty()) {
    const Cell& cell = cells.targets.front().get();
    return cmd::CommandResult::success(series ? cell.flags.series_combinable : !cell.flags.no_parallel);
  }
  cmd::CommandResult result = cmd::CommandResult::success();
  for (const auto& arg : args) {
    if (const auto word = match_keyword(arg, kSwitches)) {
      for (const auto& target : cells.targets) {
        enable(target.get(), series, *word < 5);
      }
      continue;
    }
    if (!combine_pair(runtime, cells, series, arg)) {
      result.value = std::string{kBadCombinePair};
    }
  }
  return result;
}

cmd::CommandResult merge_option(Runtime& runtime, const Resolved& cells, std::span<const std::string> args) {
  cmd::CommandResult result = cmd::CommandResult::success();
  for (const auto& arg : args) {
    auto pair = split_list(arg);
    const auto merge = pair && pair.value().size() == 2 ? legacy_merge(pair.value()[1]) : std::nullopt;
    if (!merge) {
      result.value = std::string{kBadCombinePair};
      continue;
    }
    for (const auto& target : cells.targets) {
      Cell& cell = target.get();
      if (PropertyDef* def = find_definition(cell, pair.value()[0])) {
        def->merge = *merge;  // `merge` replaces the whole rule of the key
      } else {
        report_missing_property(runtime, cell, pair.value()[0]);
      }
    }
  }
  return result;
}

cmd::CommandResult global_combine(Runtime& runtime, bool series, std::string_view word) {
  SetupState& state = runtime.state();
  const bool none = word == "none";
  if (word == "none" || word == "all") {
    for (Circuit* circuit : runtime.circuits()) {
      for (auto& cell : circuit->db.cells) {
        enable(cell, series, !none);
      }
    }
    if (!series) {
      state.parallel_none = none;
    }
    return cmd::CommandResult::success();
  }
  if (series) {
    return command_error("Bad option, should be property series none|all");
  }
  if (word == "connected" || word == "open") {
    state.parallel_open = word == "open";
    return cmd::CommandResult::success();
  }
  return command_error("Bad option, should be property parallel none|all|connected");
}

}  // namespace hefesto::lvs::setup
