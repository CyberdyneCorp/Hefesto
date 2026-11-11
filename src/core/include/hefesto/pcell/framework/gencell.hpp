// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The gencell entry points (openspec/changes/m4-scripting/design.md D8/D9;
// capability pcell-toolkit "gencell entry point dispatch", "Create
// algorithm", "Change algorithm", "Non-placing cell generation", "Generated
// cell properties"; Q-SCRIPT-23, 24, 29). Magic implements them in
// toolkit.tcl; Hefesto implements them in C++ under the same `magic::`
// names. Engine commands run nested (as the M2 ports of tools.tcl do);
// the PDK's `<lib>::<type>_defaults|_convert|_dialog|_check|_draw`
// procedures run in the project's interpreter (EditorState::script.interp).
//
// Every function takes the running command's context; the interpreter and
// the framework state come from its editor session.
//
// Owner: PCELL (design D2).

#pragma once

#include <string>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/pcell/framework/parameters.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::pcell {

/// The arguments of `magic::gencell gencell_name ?instname? ?args...?`.
struct GencellRequest {
  std::string gencell;            // "<lib>::<type>", a type of `toolkit`, or empty (the selection)
  std::string instance;           // empty: interactive
  std::vector<std::string> args;  // ?-spice? k v ...
};

/// magic::gencell (toolkit.tcl:641-779): the dispatch table of capability
/// pcell-toolkit; ok with value 0, or a raised PCELL-GEN-001 script error.
[[nodiscard]] cmd::CommandResult gencell(cmd::CommandContext& ctx, const GencellRequest& request);

/// magic::gencell_create type library parameters ?orient? (toolkit.tcl
/// :1200-1283): the new instance's name. An empty `parameters` takes them
/// from the dialog.
[[nodiscard]] cmd::CommandResult create(cmd::CommandContext& ctx, const std::string& type,
                                        const std::string& library, const ParameterDict& parameters,
                                        const std::string& orientation = "0");

/// magic::gencell_change instname type library parameters (toolkit.tcl
/// :867-1042, Q-SCRIPT-29 fixed). An empty `parameters` takes them from the
/// dialog.
[[nodiscard]] cmd::CommandResult change(cmd::CommandContext& ctx, const std::string& instance,
                                        const std::string& type, const std::string& library,
                                        const ParameterDict& parameters);

/// magic::gencell_makecell lib::type ?k v ...? (toolkit.tcl:789-809): the
/// generated cell's name; no instance is placed.
[[nodiscard]] cmd::CommandResult makecell(cmd::CommandContext& ctx, const std::string& fullname,
                                          const std::vector<std::string>& args);

/// magic::gencell_defaults type library parameters: merge(_defaults, parameters).
[[nodiscard]] cmd::CommandResult defaults(cmd::CommandContext& ctx, const std::string& type,
                                          const std::string& library, const ParameterDict& parameters);

/// The Tcl-visible framework procedures registered under `magic::`
/// (toolkit.tcl), in the order of toolkit.tcl.
[[nodiscard]] const std::vector<std::string>& framework_command_names();

/// Registers `gencell` and the framework procedures (framework_command_names())
/// in cmd::kScriptClient.
bool register_pcell_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::pcell
