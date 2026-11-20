// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `lef read` and `lef datestamp` (lef/read/args.hpp; lef/lefCmd.c:223-269,
// 446-482). `lef datestamp <int>` saturates a value outside the int range
// (Magic's atoi is undefined there), and -1, Magic's "default" sentinel,
// clears the stamp as in Magic. `lef read` reads through the session's
// file system.
//
// Owner: LEFR (design D2, D14).

#include "lef/read/args.hpp"

#include <string>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/read/read.hpp"
#include "hefesto/mask/gen/progress.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::lef {

namespace cu = db::cmdutil;

namespace {

/// strncmp(arg, option, length) == 0 with length = the option's length.
bool matches(const std::string& arg, std::string_view option) {
  return arg.starts_with(option);
}

}  // namespace

cmd::CommandResult read_option(const cu::Session& s, const cmd::Invocation& invocation, const HelpPrinter& help) {
  const std::vector<std::string>& args = invocation.args;
  if (args.size() < 2) {
    return help();
  }
  LefReadOptions options;
  for (std::size_t i = 2; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (matches(arg, "-import")) {
      options.import = true;
    } else if (matches(arg, "-anno")) {
      options.annotate = true;
    } else if (matches(arg, "-label")) {
      cu::print(s, "The \"-labels\" option is only for def read");
    } else if (matches(arg, "-noblock")) {
      cu::print(s, "The \"-noblockage\" option is only for def read");
    }
  }
  options.typed_name = args[1];
  // The session's file system (the sandbox's granted roots, the iPadOS
  // document model), as `gds read`, `def read` and `lef write` use.
  if (s.ctx != nullptr) {
    options.files = &s.ctx->services().files;
  }
  // Through the session's read host (the desktop shows a card and pumps
  // frames, design D16); lef::read reports no progress. Without a host it
  // runs inline.
  const mask::ProgressHost& host = s.editor->mask.read_host;
  if (!host) {
    return read(*s.editor, args[1], options);
  }
  return host(platform::Path{args[1]}.filename().string(),
              [&](const mask::Progress*) { return read(*s.editor, args[1], options); });
}

cmd::CommandResult datestamp_option(const cu::Session& s, const cmd::Invocation& invocation, const HelpPrinter& help) {
  const std::vector<std::string>& args = invocation.args;
  std::optional<std::int64_t>& stamp = s.editor->lefdef.datestamp;
  if (args.size() == 1) {
    return stamp ? cmd::CommandResult::success(cmd::Value{*stamp})
                 : cmd::CommandResult::success(cmd::Value{std::string{"default"}});
  }
  if (args.size() != 2) {
    return help();
  }
  if (args[1] == "default") {
    stamp.reset();
  } else if (cu::str_is_int(args[1])) {
    const std::int32_t value = cu::c_atoi(args[1]);
    stamp = value == -1 ? std::nullopt : std::optional<std::int64_t>{value};
  } else {
    cu::warn(s, "Unrecognizable date stamp \"" + args[1] + "\".");
    return help();
  }
  return cmd::CommandResult::success();
}

}  // namespace hefesto::lef
