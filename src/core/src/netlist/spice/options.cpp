// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` options (capability netlist-spice-options "Command
// options"): queries return Magic's Tcl results, sets change
// EditorState::netlist.spice, and a bad value prints the option table as
// Magic's `goto usage` does. `scale` is the setting M4's PDK startup files
// use (user decision M7-3).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:381-820
// (CmdExtToSpice's option switch), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include "netlist/spice/options.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

#include "db/command_util.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

namespace cu = db::cmdutil;

constexpr std::array<std::string_view, 6> kYesNo{"yes", "true", "on", "no", "false", "off"};
constexpr std::array<std::string_view, 9> kSubcktOpts{"yes", "true", "on", "no", "false", "off",
                                                      "automatic", "top", "descend"};
constexpr std::array<std::string_view, 5> kFormats{"spice2", "spice3", "hspice", "ngspice", "cdl"};
constexpr std::array<std::string_view, 3> kMergeTypes{
    "none\t\t\tdon't merge parallel devices",
    "conservative\t\tmerge devices with same L, W",
    "aggressive\t\tmerge devices with same L",
};
constexpr std::array<std::string_view, 3> kShortTypes{
    "none\t\t\tmerge shorted ports",
    "resistor\t\tseparate shorted ports with 0 ohm resistor",
    "voltage\t\t\tseparate shorted ports with 0 volt source",
};

enum SubcktIdx : int { kYes, kTrue, kOn, kNo, kFalse, kOff, kAuto, kTop, kDescend };

// INFINITE_THRESHOLD and INFINITE_THRESHOLD_F (EFtypes.h).
constexpr std::int64_t kInfiniteResist = std::numeric_limits<std::int32_t>::max();
constexpr float kInfiniteCapF = 1.0E38F;

struct Call {
  cmd::CommandContext& ctx;
  const cmd::Invocation& invocation;
  std::span<const std::string_view> table;
  cu::Session session;
  SpiceSettings& settings;

  // Magic's tx_argc: the command word and its arguments.
  [[nodiscard]] std::size_t argc() const { return invocation.args.size() + 1; }
  // Magic's tx_argv[i] (i >= 1).
  [[nodiscard]] const std::string& argv(std::size_t i) const { return invocation.args[i - 1]; }

  [[nodiscard]] cmd::CommandResult usage() const {
    netlist::detail::print_table(ctx, table);
    return cmd::CommandResult::success();
  }
};

cmd::CommandResult on_off(bool value) {
  return cmd::CommandResult::success(std::string{value ? "on" : "off"});
}

// An on/off setting: query with no value, otherwise Lookup in yes/no.
// `exact` is the argc a set needs (0: any; Magic ignores extra words for
// most switches).
cmd::CommandResult switch_option(const Call& call, bool& value, std::size_t query_argc, std::size_t exact) {
  if (call.argc() == query_argc) {
    return on_off(value);
  }
  if (exact != 0 && call.argc() != exact) {
    return call.usage();
  }
  const int idx = cu::lookup_in(call.argv(query_argc), kYesNo);
  if (idx < 0) {
    return call.usage();
  }
  value = idx < 3;
  return cmd::CommandResult::success();
}

cmd::CommandResult scale_option(const Call& call) {
  bool scale = call.settings.scale;
  cmd::CommandResult result = switch_option(call, scale, 2, 3);
  call.settings.scale = scale;
  return result;
}

cmd::CommandResult short_option(const Call& call) {
  if (call.argc() == 2) {
    switch (call.settings.shorts) {
      case ShortMode::none:
        return cmd::CommandResult::success(std::string{"none"});
      case ShortMode::resistor:
        return cmd::CommandResult::success(std::string{"resistor"});
      case ShortMode::voltage:
        return cmd::CommandResult::success(std::string{"voltage source"});
    }
  }
  const int idx = cu::lookup_in(call.argv(2), kShortTypes);
  if (idx < 0) {
    return call.usage();
  }
  call.settings.shorts = static_cast<ShortMode>(idx);
  return cmd::CommandResult::success();
}

cmd::CommandResult top_value(TopSubckt top) {
  switch (top) {
    case TopSubckt::automatic:
      return cmd::CommandResult::success(std::string{"auto"});
    case TopSubckt::on:
      return cmd::CommandResult::success(std::string{"on"});
    case TopSubckt::off:
      break;
  }
  return cmd::CommandResult::success(std::string{"off"});
}

// The second value of `subcircuits top|descend <value>`.
cmd::CommandResult subckt_value(const Call& call, int idx) {
  const int idx2 = cu::lookup_in(call.argv(3), kSubcktOpts);
  const bool descend = idx == kDescend;
  if (idx2 >= kYes && idx2 <= kOff) {
    const bool on = idx2 <= kOn;
    if (descend) {
      call.settings.subckt_descend = on;
    } else {
      call.settings.subckt_top = on ? TopSubckt::on : TopSubckt::off;
    }
    return cmd::CommandResult::success();
  }
  // Q-NETLIST-41: `descend auto` sets the top mode with magic-compat ON.
  if (idx2 == kAuto && (!descend || call.session.magic_compat)) {
    call.settings.subckt_top = TopSubckt::automatic;
    return cmd::CommandResult::success();
  }
  return call.usage();
}

cmd::CommandResult subcircuits_option(const Call& call) {
  if (call.argc() == 2) {
    return on_off(call.settings.subckt_descend);
  }
  const int idx = cu::lookup_in(call.argv(2), kSubcktOpts);
  if (idx >= kYes && idx <= kOff) {
    call.settings.subckt_descend = idx <= kOn;
    return cmd::CommandResult::success();
  }
  if (idx != kTop && idx != kDescend) {
    return call.usage();
  }
  if (call.argc() == 3) {
    return idx == kTop ? top_value(call.settings.subckt_top) : on_off(call.settings.subckt_descend);
  }
  if (call.argc() != 4) {
    return call.usage();
  }
  return subckt_value(call, idx);
}

cmd::CommandResult format_option(const Call& call) {
  if (call.argc() == 2) {
    return cmd::CommandResult::success(std::string{kFormats[static_cast<std::size_t>(call.settings.format)]});
  }
  const int idx = cu::lookup_in(call.argv(2), kFormats);
  if (idx < 0) {
    return cmd::CommandResult::success(
        std::string{"Bad format type.  Formats are:spice2, spice3, hspice, ngspice, and cdl."});
  }
  call.settings.format = static_cast<Dialect>(idx);
  if (call.settings.format == Dialect::hspice) {
    call.settings.scale = true;  // by default, .option scale in HSPICE mode
  }
  return cmd::CommandResult::success();
}

bool cap_finite(double cthresh) {
  return std::isfinite(cthresh) && static_cast<float>(cthresh) != kInfiniteCapF;
}

cmd::CommandResult cthresh_option(const Call& call) {
  if (call.argc() == 2) {
    if (!cap_finite(call.settings.cthresh)) {
      return cmd::CommandResult::success(std::string{"infinite"});
    }
    return cmd::CommandResult::success(call.settings.cthresh);
  }
  const std::string& word = call.argv(2);
  if (word.starts_with("inf")) {
    call.settings.cthresh = std::numeric_limits<double>::infinity();
  } else if (cu::str_is_numeric(word)) {
    // EFCapValue is a float.
    const float value = static_cast<float>(std::atof(word.c_str()));
    call.settings.cthresh =
        value == kInfiniteCapF ? std::numeric_limits<double>::infinity() : static_cast<double>(value);
  } else {
    cu::warn(call.session, "exttospice: numeric value or \"infinite\" expected.");
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult rthresh_option(const Call& call) {
  if (call.argc() == 2) {
    if (call.settings.rthresh >= kInfiniteResist) {
      return cmd::CommandResult::success(std::string{"infinite"});
    }
    return cmd::CommandResult::success(call.settings.rthresh);
  }
  const std::string& word = call.argv(2);
  if (cu::str_is_int(word)) {
    const std::int64_t value = cu::c_atoi(word);
    call.settings.rthresh = value >= kInfiniteResist ? std::numeric_limits<std::int64_t>::max() : value;
  } else if (word.starts_with("inf")) {
    call.settings.rthresh = std::numeric_limits<std::int64_t>::max();
  } else {
    cu::warn(call.session, "exttospice: integer value or \"infinite\" expected.");
  }
  return cmd::CommandResult::success();
}

cmd::CommandResult merge_option(const Call& call) {
  constexpr std::array<std::string_view, 3> kNames{"none", "conservative", "aggressive"};
  if (call.argc() == 2) {
    return cmd::CommandResult::success(std::string{kNames[static_cast<std::size_t>(call.settings.merge)]});
  }
  const int value = cu::lookup_in(call.argv(2), kMergeTypes);
  if (value < 0) {
    cu::warn(call.session, "Merge types are:");
    std::string text;
    for (const std::string_view entry : kMergeTypes) {
      text += "    " + std::string{entry} + "\n";
    }
    cu::print_block(call.session, std::move(text));
    return cmd::CommandResult::success();
  }
  call.settings.merge = static_cast<MergeMode>(value);
  return cmd::CommandResult::success();
}

// `default`: the thresholds, the name-conversion flags (the `short` mode
// kept), and the run's -T and -p.
cmd::CommandResult default_option(const Call& call) {
  call.settings.cthresh = 2.0;
  call.settings.rthresh = std::numeric_limits<std::int64_t>::max();
  call.settings.search_path.clear();
  call.settings.tech.reset();
  extflat::NameFlags& names = call.session.editor->netlist.names;
  names.trim_global = false;
  names.trim_local = false;
  names.convert_brackets = false;
  names.convert_comma = true;
  names.convert_equal = true;
  return cmd::CommandResult::success();
}

cmd::CommandResult flag_option(const Call& call, std::size_t option) {
  SpiceSettings& s = call.settings;
  switch (option) {
    case kExtresist:
      return switch_option(call, s.extresist, 2, 3);
    case kResistorTee:
      return switch_option(call, s.resistor_tee, 3, 4);
    case kHierarchy:
      return switch_option(call, s.hierarchy, 2, 0);
    case kBlackbox:
      return switch_option(call, s.blackbox, 2, 0);
    case kRenumber:
      return switch_option(call, s.renumber, 2, 0);
    case kGlobal:
      return switch_option(call, s.global, 2, 0);
    default:
      break;
  }
  return call.usage();
}

cmd::CommandResult valued_option(const Call& call, std::size_t option) {
  switch (option) {
    case kDefault:
      return default_option(call);
    case kFormat:
      return format_option(call);
    case kRthresh:
      return rthresh_option(call);
    case kCthresh:
      return cthresh_option(call);
    case kMerge:
      return merge_option(call);
    case kScale:
      return scale_option(call);
    case kShort:
      return short_option(call);
    case kSubcircuits:
      return subcircuits_option(call);
    case kLvs:
      call.settings = SpiceSettings::lvs(call.settings);
      return cmd::CommandResult::success();
    default:
      break;
  }
  return flag_option(call, option);
}

}  // namespace

std::optional<cmd::CommandResult> spice_option(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                               std::size_t option, std::span<const std::string_view> table) {
  if (option == kRun || option == kHelp) {
    return std::nullopt;
  }
  auto session = cu::session(ctx);
  if (!session) {
    return cu::fail(session.error());
  }
  const Call call{ctx, invocation, table, session.value(), session.value().editor->netlist.spice};
  return valued_option(call, option);
}

}  // namespace hefesto::netlist::spice::detail
