// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef::read (capability lef-read "lef read side effects", "MACRO and SITE
// cell creation"; design D6): LefRead's set-up and clean-up
// (lefRead.c:2996-3024, 3186-3203) around read_stream.
//
// - The file is found by find_lef_file (Q-LEFDEF-24) and may be gzip
//   compressed.
// - The read is one undo step (Q-LEFDEF-22, platform): recording is
//   suspended while it runs and one native event (session_host.cpp)
//   records the cells it created and annotated. Magic's "This action
//   cannot be undone." is not printed.
// - The LEF tables are the session's (lef::LefSession); an empty layer
//   table is a table never initialised (lef-tech-section "LEF table
//   lifecycle").
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/read.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/lex/lexer.hpp"
#include "hefesto/lef/read/find_file.hpp"
#include "hefesto/lef/read/session_layers.hpp"
#include "hefesto/lef/read/stream.hpp"
#include "hefesto/lef/read/units.hpp"
#include "lef/read/session_host.hpp"

namespace hefesto::lef {

namespace {

constexpr std::string_view kReadingCode = "LEF-READ-004";
constexpr std::string_view kProcessedCode = "LEF-READ-005";
constexpr std::string_view kCannotOpenCode = "LEF-READ-006";

cmd::CommandResult cannot_open(const std::string& name, const std::string& reason) {
  return cmd::CommandResult::failure(
      diag::make(kCannotOpenCode, "Cannot open input file " + name + " (" + reason + ")."));
}

}  // namespace

cmd::CommandResult read(editor::EditorState& editor, const platform::Path& path, const LefReadOptions& options) {
  platform::LocalFileSystem local;
  platform::FileSystem& fs = options.files != nullptr ? *options.files : local;
  const std::string name = path.generic_string();
  diag::CollectingSink console;
  cell::LookupContext lookup{fs, editor.search_paths, editor.path_variables, console};
  const auto found = find_lef_file(lookup, name);
  if (!found) {
    return cannot_open(lef_candidate(name), "No such file or directory");
  }
  auto source = gds::open_source(fs, found->path);
  if (!source.ok()) {
    return cannot_open(found->path.generic_string(), source.error().text);
  }
  const db::DatabaseLock::Exclusive lock(editor.database_lock);
  const compat::CompatSettings compat = editor.drc.compat_settings();
  const tech::Technology& technology = *editor.technology;
  const mask::OutputStyle* style = editor.mask.output_styles.current_style();
  console.emit(diag::make(kReadingCode, "Reading LEF data from file " + found->path.generic_string() + "."));

  const auto size = fs.size(found->path);
  Lexer lexer{*source.value(), size.ok() ? size.value() : 0};
  MessageCounter counter;
  const MessageTarget messages{counter, console, lexer};
  LefSession& lefdef = editor.lefdef;
  SessionLayers layers{lefdef.layers, lefdef.vias, technology, editor.drc.rules.get()};
  detail::SessionMacroHost host{editor};
  StreamTarget target{layers, lefdef.vias, lefdef.rules, &lefdef.layers, &host};
  const StreamOptions stream_options{technology, lef_scale(style), compat, style,
                                     MacroOptions{options.annotate, lefdef.datestamp}};
  StreamResult result;
  {
    const undo::ScopedSuspend unrecorded(editor.undo);
    result = read_stream(lexer, messages, stream_options, target);
  }
  if (auto event = host.take_event("lef read " + name)) {
    editor.undo.record(std::move(event));
  }
  if (const auto& failure = lexer.failure()) {
    console.emit(*failure);
  }
  console.emit(diag::make(kProcessedCode, "LEF read: Processed " + std::to_string(result.lines) + " lines."));
  counter.summarize(console, Dialect::lef);
  cmd::CommandResult done = cmd::CommandResult::success();
  done.diagnostics = console.take();
  return done;
}

}  // namespace hefesto::lef
