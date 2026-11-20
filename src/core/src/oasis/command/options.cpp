// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The settings options of the `oasis` command (capability oasis-command
// "Option syntax", "Options shared with the gds command", "OASIS-only
// options", "Settings shared with the gds command"). The value syntax is
// the `gds` command's (commands/CmdCD.c CmdCalma): yes/no and
// allow/disallow words by unique prefix, a query when the value is
// omitted (answered as the command result, oasis-command "Scripting
// access"), and an argument error that changes nothing. The shared read
// settings go to the `gds` command's handlers, so they are one setting
// with `gds`; the others are MaskSession::oasis.
//
// Owner: OASCMD (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <array>
#include <cstdlib>
#include <utility>

#include "../../gds/command/options.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "options.hpp"

namespace hefesto::oasis::command_detail {

namespace {

using cmd::CommandResult;
namespace gc = gds::command_detail;

constexpr std::array<std::string_view, 3> kSchemes{"none", "crc32", "checksum"};
constexpr std::int32_t kMinCirclePoints = 8;
constexpr std::int32_t kMaxCirclePoints = 4096;

CommandResult boolean(bool value) {
  return CommandResult::success(cmd::Value{std::int64_t{value ? 1 : 0}});
}

CommandResult yes_no(const cmd::Invocation& invocation, bool& field) {
  if (invocation.args.size() == 1) {
    return boolean(field);
  }
  const auto index =
      invocation.args.size() == 2 ? gc::lookup_index(invocation.args[1], gc::kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  field = *index >= 4;
  return CommandResult::success();
}

CommandResult allow(const cmd::Invocation& invocation, bool& field) {
  if (invocation.args.size() == 1) {
    return boolean(field);
  }
  const auto index =
      invocation.args.size() == 2 ? gc::lookup_index(invocation.args[1], gc::kAllowDisallow) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  field = *index >= 2;
  return CommandResult::success();
}

/// An integer option bounded to [low, high]; out of range is an error and
/// the value is kept (oasis-command "Compression out of range", "Circle
/// points bound").
CommandResult bounded(const cmd::Invocation& invocation, std::int32_t& field, std::int32_t low, std::int32_t high,
                      std::string_view what) {
  if (invocation.args.size() == 1) {
    return CommandResult::success(cmd::Value{std::int64_t{field}});
  }
  if (invocation.args.size() != 2 || !cu::str_is_int(invocation.args[1])) {
    return wrong_args(invocation);
  }
  const std::int32_t value = cu::c_atoi(invocation.args[1]);
  if (value < low || value > high) {
    return CommandResult::failure(diag::make(kBadValue, "Bad " + std::string{what} + " value " +
                                                            std::to_string(value) + ".  Value must be in the range " +
                                                            std::to_string(low) + " to " + std::to_string(high) +
                                                            "."));
  }
  field = value;
  return CommandResult::success();
}

CommandResult contacts(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  if (invocation.args.size() == 1) {
    return boolean(false);
  }
  const auto index =
      invocation.args.size() == 2 ? gc::lookup_index(invocation.args[1], gc::kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  if (*index >= 4) {
    ctx.emit(diag::make(kUnimplemented, "Unimplemented function:  Contacts will be output as individual cuts."));
  }
  return CommandResult::success();
}

// `datestamp yes|no|<value>`: nullopt is `yes` (the default stamp of the
// read, Q-OASIS-30), `no` is 0.
CommandResult datestamp(const cmd::Invocation& invocation, OasisSettings& o) {
  if (invocation.args.size() == 1) {
    return o.datestamp ? CommandResult::success(cmd::Value{*o.datestamp})
                       : CommandResult::success(cmd::Value{"default"});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  if (const auto index = gc::lookup_index(invocation.args[1], gc::kYesNo)) {
    o.datestamp = *index >= 4 ? std::nullopt : std::optional<std::int64_t>{0};
    return CommandResult::success();
  }
  if (!cu::str_is_int(invocation.args[1])) {
    return wrong_args(invocation, "Unrecognizable date stamp \"" + invocation.args[1] + "\".\n");
  }
  o.datestamp = cu::c_atoi(invocation.args[1]);
  return CommandResult::success();
}

CommandResult nodatestamp(const cmd::Invocation& invocation, OasisSettings& o) {
  if (invocation.args.size() == 1) {
    if (!o.datestamp) {
      return boolean(false);
    }
    return *o.datestamp == 0 ? boolean(true) : CommandResult::success(cmd::Value{"fixed"});
  }
  const auto index =
      invocation.args.size() == 2 ? gc::lookup_index(invocation.args[1], gc::kYesNo) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  o.datestamp = *index >= 4 ? std::optional<std::int64_t>{0} : std::nullopt;
  return CommandResult::success();
}

// Accepted with the `gds` syntax; no effect on OASIS (no text
// magnification).
CommandResult magscale(const cmd::Invocation& invocation, OasisSettings& o) {
  if (invocation.args.size() == 1) {
    return CommandResult::success(cmd::Value{o.magscale});
  }
  if (invocation.args.size() != 2) {
    return wrong_args(invocation);
  }
  const std::string& value = invocation.args[1];
  if (cu::str_is_numeric(value)) {
    o.magscale = static_cast<double>(static_cast<float>(std::atof(value.c_str())));
  } else if (value == "default") {
    o.magscale = 1.0;
  } else {
    return wrong_args(invocation);
  }
  return CommandResult::success();
}

CommandResult validation(const cmd::Invocation& invocation, OasisSettings& o) {
  if (invocation.args.size() == 1) {
    return CommandResult::success(cmd::Value{std::string{kSchemes.at(static_cast<std::size_t>(o.validation))}});
  }
  const auto index =
      invocation.args.size() == 2 ? gc::lookup_index(invocation.args[1], kSchemes) : std::nullopt;
  if (!index) {
    return wrong_args(invocation);
  }
  o.validation = static_cast<ValidationScheme>(*index);
  return CommandResult::success();
}

std::optional<CommandResult> boolean_setting(std::size_t option, const cmd::Invocation& invocation,
                                             OasisSettings& o) {
  switch (option) {
    case kAbstract:
      return allow(invocation, o.allow_abstract);
    case kUndefined:
      return allow(invocation, o.allow_undefined);
    case kAddendum:
      return yes_no(invocation, o.addendum);
    case kArrays:
      return yes_no(invocation, o.arrays);
    case kLabels:
      return yes_no(invocation, o.labels);
    case kLibrary:
      return yes_no(invocation, o.library);
    case kLower:
      return yes_no(invocation, o.lower);
    case kMerge:
      return yes_no(invocation, o.merge);
    case kStrict:
      return yes_no(invocation, o.strict);
    case kRepetitions:
      return yes_no(invocation, o.repetitions);
    case kLayerNames:
      return yes_no(invocation, o.layernames);
    default:
      return std::nullopt;
  }
}

/// The GDS-CMD-* code of the `gds` handlers as its OASIS-CMD-* twin.
void renumber(CommandResult& result) {
  for (diag::Diagnostic& d : result.diagnostics) {
    if (d.code == gc::kWrongArgs) {
      d.code = kWrongArgs;
    }
  }
}

}  // namespace

cmd::CommandResult wrong_args(const cmd::Invocation& invocation, std::string prefix) {
  return CommandResult::failure(diag::make(kWrongArgs, prefix + "Wrong number of arguments in \"" + invocation.name +
                                                           "\" command.  Try \":" + invocation.name +
                                                           " help\" for help."));
}

std::optional<CommandResult> settings_option(cmd::CommandContext& ctx, std::size_t option,
                                             const cmd::Invocation& invocation, OasisSettings& settings) {
  switch (option) {
    case kCompress:
      return bounded(invocation, settings.compress, 0, 9, "compression");
    case kCirclePoints:
      return bounded(invocation, settings.circlepoints, kMinCirclePoints, kMaxCirclePoints, "circlepoints");
    case kContacts:
      return contacts(ctx, invocation);
    case kDatestamp:
      return datestamp(invocation, settings);
    case kNoStamp:
      return nodatestamp(invocation, settings);
    case kMagScale:
      return magscale(invocation, settings);
    case kValidation:
      return validation(invocation, settings);
    default:
      return boolean_setting(option, invocation, settings);
  }
}

std::optional<CommandResult> shared_option(const cu::Session& s, std::size_t option,
                                           const cmd::Invocation& invocation) {
  if (option == kMagScale) {
    return std::nullopt;  // an `oasis` setting of its own
  }
  std::optional<CommandResult> result = gc::read_settings_option(s, option, invocation);
  if (result) {
    renumber(*result);
  }
  return result;
}

}  // namespace hefesto::oasis::command_detail
