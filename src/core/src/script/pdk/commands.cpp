// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `ext2spice scale` (scripting-pdk-integration "PDK startup file contract"
// step 5). Ported from Magic 8.3.694 (fc5daa9e)
// ext2spice/ext2spice.c:289-345,421-437: `ext2spice scale` returns "on"
// when esScale < 0 (the default), `ext2spice scale yes|true|on|no|false|off`
// (Magic's Lookup) sets it. Copyright (C) 1985, 1990 Regents of the
// University of California, see THIRD_PARTY_NOTICES.md.

#include "hefesto/script/pdk/commands.hpp"

#include <array>
#include <string>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::script {

namespace {

constexpr std::array<std::string_view, 16> kOptions = {
    "run",       "default", "format",    "rthresh",  "cthresh",  "merge",  "extresist", "resistor",
    "scale",     "short",   "subcircuits", "hierarchy", "blackbox", "renumber", "global", "lvs"};
constexpr std::size_t kScale = 8;

constexpr std::array<std::string_view, 6> kYesNo = {"yes", "true", "on", "no", "false", "off"};

cmd::CommandResult usage() {
  return cmd::CommandResult::failure(
      diag::make("CMD-USAGE-001", "Usage: ext2spice scale [on|off]  (other ext2spice options arrive with M7)"));
}

cmd::CommandResult scale(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return cmd::CommandResult::failure(editor.error());
  }
  auto& settings = editor.value()->settings;
  if (inv.args.size() == 1) {
    const auto value = settings.effective(kExt2spiceScale);
    return cmd::CommandResult::success(std::string{value && value->value == "off" ? "off" : "on"});
  }
  const auto choice = inv.args.size() == 2 ? cmd::lookup(inv.args[1], kYesNo) : cmd::LookupResult{};
  if (choice.match != cmd::Match::found) {
    return usage();
  }
  settings.set(app::SettingLayer::session, kExt2spiceScale, choice.index < 3 ? "on" : "off");
  return cmd::CommandResult::success();
}

// The SPICE netlist writer is an optional module that arrives with
// extraction (M7): UI-CLIENT-002, as `netlist` and `wind3d` do.
cmd::CommandResult writer_unavailable() {
  return cmd::CommandResult::failure(diag::make(ui::codes::kModuleUnavailable,
                                                "ext2spice: the SPICE netlist writer is not available yet (ext2spice, M7)"));
}

cmd::CommandResult ext2spice(cmd::CommandContext& ctx, const cmd::Invocation& inv) {
  if (inv.args.empty()) {
    return writer_unavailable();
  }
  const auto option = cmd::lookup(inv.args.front(), kOptions);
  if (option.match == cmd::Match::found && option.index == kScale) {
    return scale(ctx, inv);
  }
  return writer_unavailable();
}

}  // namespace

bool register_pdk_commands(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{"ext2spice", "ext2spice [option]",
                        "convert extracted file(s) to a SPICE format file; only `scale [on|off]` before M7",
                        ext2spice};
  return !registry.add(std::move(spec), cmd::kLayoutClient).has_value();
}

}  // namespace hefesto::script
