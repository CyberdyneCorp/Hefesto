// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the run path of `ext2spice` shared by the command, run() and
// render() (netlist/spice/spice.hpp).
//
// Owner: SPICE.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extflat/args.hpp"
#include "hefesto/extflat/hierarchy.hpp"
#include "hefesto/netlist/spice/input.hpp"
#include "hefesto/netlist/spice/settings.hpp"
#include "hefesto/netlist/spice/writer.hpp"

namespace hefesto::editor {
struct EditorState;
}
namespace hefesto::platform {
class FileSystem;
}

namespace hefesto::netlist::spice::detail {

/// EFReadFile and EFFlatBuild of `cell`, and the writer's input from the
/// current extraction style.
[[nodiscard]] diag::Outcome<SpiceInput> read_input(editor::EditorState& editor, std::string_view cell,
                                                   const SpiceSettings& settings,
                                                   const compat::CompatSettings& compat, platform::FileSystem& fs,
                                                   diag::DiagnosticSink& sink, const extflat::SymbolTable& symbols);

[[nodiscard]] std::vector<diag::Diagnostic> to_diagnostics(const std::vector<WriteMessage>& messages);

/// EFReadFile's options for a run with `settings`.
[[nodiscard]] extflat::ReadOptions read_options(const editor::EditorState& editor, const SpiceSettings& settings,
                                                const compat::CompatSettings& compat);
/// EFFlatBuild's flags and settings for a run.
[[nodiscard]] extflat::FlatFlags flat_flags(const SpiceSettings& settings);
[[nodiscard]] extflat::FlattenOptions flatten_options(const editor::EditorState& editor, const SpiceSettings& settings,
                                                      const extflat::SymbolTable& symbols);
/// esFetInfo, the resistance classes and $GND from the current style.
void apply_style(SpiceInput& input, const editor::EditorState& editor);

/// What one run writes with.
struct RunRequest {
  SpiceSettings settings;
  RunFlags flags;
  compat::CompatSettings compat;
  extflat::SymbolTable symbols;
  std::string trim_chars;  // EFArgs' -t
};

/// The netlist of one run, flat or hierarchical.
[[nodiscard]] diag::Outcome<WriteResult> write_run(editor::EditorState& editor, std::string_view cell,
                                                   const RunRequest& request, platform::FileSystem& fs,
                                                   diag::DiagnosticSink& sink, extflat::NameFlags& names);

/// ESGenerateHierarchy: the read with subcircuit marking, port promotion,
/// then one section per definition (hier_run.cpp).
[[nodiscard]] diag::Outcome<WriteResult> write_hierarchy(editor::EditorState& editor, std::string_view cell,
                                                         const RunRequest& request, platform::FileSystem& fs,
                                                         diag::DiagnosticSink& sink, extflat::NameFlags& names);

/// runexttospice with the run arguments.
[[nodiscard]] cmd::CommandResult run_session(editor::EditorState& editor, const std::vector<std::string>& args,
                                             const compat::CompatSettings& compat, platform::FileSystem& fs);

}  // namespace hefesto::netlist::spice::detail
