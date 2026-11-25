// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extresist` setting options (extresist/command/options.hpp;
// capability extresist-command "Command options"; design D16).
//
// The decisions applied here:
// - Q-NETLIST-23 (fix, both modes): `skip` takes a comma-separated type
//   list; an unknown or ambiguous type reports an error and leaves every
//   mask unchanged (Magic accepts one type and writes out of bounds for a
//   bad one).
// - Q-NETLIST-28 (replicate): `mindelay` is given and queried in ps.
// - Q-NETLIST-21 (fix): the FastHenry frequency defaults to 10 MHz.
// - Every on/off option rejects a value other than a prefix of `on` or
//   `off` (the spec's rule; Magic checks only simplify, extout, lumped,
//   silent and blackbox and turns debug and stats on for any other word).
// - A negative threshold, minresist or mindelay prints the usage line;
//   with magic-compat ON the value is stored, as Magic does, with it OFF
//   the previous value is kept.
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResRex.c CmdExtResis (the
// option cases), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include "extresist/command/options.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "extflat/text.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::extresist::detail {

namespace {

constexpr double kDefaultFrequency = 10.0e6;  // resisdata->frequency as intended (Q-NETLIST-21)
constexpr std::size_t kInitFlatSize = 1024;   // INITFLATSIZE of ResRex.c
constexpr std::array<std::string_view, 2> kOnOff{"off", "on"};

bool* flag(ResistOptions& o, std::size_t option) {
  switch (option) {
    case kSimplify:
      return &o.simplify;
    case kExtout:
      return &o.extout;
    case kLumped:
      return &o.lumped;
    case kSilent:
      return &o.silent;
    case kDebug:
      return &o.debug;
    case kBlackbox:
      return &o.blackbox;
    case kStats:
      return &o.stats;
    default:
      return nullptr;
  }
}

cmd::CommandResult on_off(bool& value, std::span<const std::string> args, const Console& console) {
  if (args.empty()) {
    console.report(value ? "on" : "off");
    return cmd::CommandResult::success();
  }
  const cmd::LookupResult found = cmd::lookup(args.front(), kOnOff);
  if (found.match != cmd::Match::found) {
    console.message("Value must be either \"on\" or \"off\".");
    return cmd::CommandResult::success();
  }
  value = found.index == 1;
  return cmd::CommandResult::success();
}

// threshold, minresist and mindelay: the query (a Tcl double), or the new
// value (MagAtof); a negative value prints the usage line.
cmd::CommandResult number(double& value, std::string_view usage, std::span<const std::string> args,
                          OptionContext& ctx) {
  if (args.empty()) {
    return cmd::CommandResult::success(cmd::Value{value});
  }
  const double v = extflat::detail::c_atof(args.front());
  if (v < 0) {
    ctx.console.message("Usage:  extresist " + std::string{usage} + " [value]");
    if (!ctx.magic_compat) {
      return cmd::CommandResult::success();  // OFF: the previous value stays
    }
  }
  value = v;
  return cmd::CommandResult::success();
}

std::vector<std::string> split_commas(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t comma = text.find(',', start);
    const std::size_t end = comma == std::string_view::npos ? text.size() : comma;
    out.emplace_back(text.substr(start, end - start));
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return out;
}

// `skip [type,...]` (Q-NETLIST-23).
cmd::CommandResult skip(std::span<const std::string> args, OptionContext& ctx) {
  if (args.empty()) {
    ctx.options.skip = tech::TypeMask{};  // the technology's connectivity again
    return cmd::CommandResult::success();
  }
  if (ctx.tech == nullptr) {
    ctx.console.message("Unrecognized layer (type) name \"" + args.front() + "\"");
    return cmd::CommandResult::success();
  }
  tech::TypeMask added;
  for (const std::string& name : split_commas(args.front())) {
    const auto found = ctx.tech->find_type(name);
    if (!found.ok()) {
      const char* why = found.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized";
      ctx.console.message(std::string{why} + " layer (type) name \"" + name + "\"");
      return cmd::CommandResult::success();  // no mask changes
    }
    added.set(*found.id);
  }
  ctx.options.skip |= added;
  return cmd::CommandResult::success();
}

bool equals_nocase(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

// force, ignore and include: add a name, clear the list, or list it in the
// order Magic's string hash table iterates.
cmd::CommandResult name_list(std::vector<std::string>& list, std::string_view clear_word,
                             std::span<const std::string> args, const Console& console) {
  if (args.empty()) {
    util::MagicHashTable<char> table(kInitFlatSize);
    for (const std::string& name : list) {
      (void)table.find_or_insert(name);
    }
    std::string text;
    for (const std::string& name : table.keys()) {
      text += name + " ";
    }
    console.report(text);
    return cmd::CommandResult::success();
  }
  if (equals_nocase(args.front(), clear_word)) {
    list.clear();
  } else if (std::find(list.begin(), list.end(), args.front()) == list.end()) {
    list.push_back(args.front());
  }
  return cmd::CommandResult::success();
}

}  // namespace

double frequency(const ResistOptions& options) noexcept {
  return options.fasthenry_frequency.value_or(kDefaultFrequency);
}

void fasthenry_frequency(std::span<const std::string> args, OptionContext& ctx) {
  if (args.size() != 1) {
    return;
  }
  const char* text = args.front().c_str();
  char* end = nullptr;
  const double value = std::strtod(text, &end);
  if (end == text) {
    std::array<char, 64> buffer{};
    (void)std::snprintf(buffer.data(), buffer.size(), "%2.1f", frequency(ctx.options));
    ctx.console.message("Cannot parse frequency value.  Assuming default");
    ctx.console.message("Frequency = " + std::string{buffer.data()} + " Hz");
    return;
  }
  ctx.options.fasthenry_frequency = static_cast<double>(static_cast<float>(value));
}

std::optional<cmd::CommandResult> setting_option(std::size_t option, std::span<const std::string> args,
                                                 OptionContext& ctx) {
  if (bool* f = flag(ctx.options, option)) {
    return on_off(*f, args, ctx.console);
  }
  switch (option) {
    case kThreshold:
      return number(ctx.options.threshold, "threshold", args, ctx);
    case kMinResist:
      return number(ctx.options.min_resist, "minres", args, ctx);
    case kMinDelay:
      return number(ctx.options.min_delay_ps, "mindelay", args, ctx);
    case kTolerance:
      ctx.console.message("Note:  This option has been deprecated and is unused.");
      return cmd::CommandResult::success();
    case kSkip:
      return skip(args, ctx);
    case kForce:
      return name_list(ctx.options.force, "none", args, ctx.console);
    case kIgnore:
      return name_list(ctx.options.ignore, "none", args, ctx.console);
    case kInclude:
      return name_list(ctx.options.include, "all", args, ctx.console);
    default:
      return std::nullopt;
  }
}

}  // namespace hefesto::extresist::detail
