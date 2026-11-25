// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic circuit source and simulator vector names, M8 part
// (openspec/changes/m8-schematic design.md D13, "Area contracts" SIMINT;
// capability schematic-simulation-integration "Schematic circuit source",
// "Testbench code blocks", "Control block translation", "Simulator vector
// names").
//
// The deck is the testbench's `hier` netlist (schem/net) with its `.include`
// and `.lib` paths resolved and its `.control` blocks translated. Running it
// is M9's: a deck goes to `simulation-netlist-preparation` through
// DeckOptions::validate (none until M9 provides it) and the `write`/`wrdata`
// lines become capture points (Deck::captures) that the job runner acts on.
//
// Owner: SIMINT.

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

struct Session;
struct NetlistEnvironment;

/// The text a capture point is written as in a translated `.control` block:
/// `echo hefesto-capture <n>`, n indexing Deck::captures. The job runner
/// (M9) captures the current plot, or the named vectors, when ngspice
/// prints it.
inline constexpr std::string_view kCaptureEcho = "echo hefesto-capture ";

/// One `write` or `wrdata` line of a `.control` block ("Control block
/// translation").
struct DeckCapture {
  std::string name;                  // the file name: the results' name in the job
  std::vector<std::string> vectors;  // the named vectors; empty: the current plot
  bool text = false;                 // `wrdata` (exported as text only if the setting is on)
  bool append = false;               // `set appendwrite` in effect: add a plot to `name`

  friend bool operator==(const DeckCapture&, const DeckCapture&) = default;
};

/// The deck of a `schematic <file>` source for simulation-netlist-preparation:
/// the testbench's `hier` netlist (not LVS) with its control blocks
/// translated, before that capability's validation.
struct Deck {
  std::string text;
  std::vector<diag::Diagnostic> diagnostics;  // the netlister's warnings, then SCHEM-SIM-002
  std::vector<DeckCapture> captures;          // in deck order
  /// The deck has a `.control` block: allowed for a schematic source without
  /// `allowControl`.
  bool control = false;
};

/// The hand-off to `simulation-netlist-preparation` (M9): validates a built
/// deck; an error fails build_deck() with it.
using DeckValidator = std::function<diag::Status(const Deck&)>;

/// A prefix of absolute paths written in a testbench (an `xschemrc`'s
/// `PDK_ROOT`, `SKYWATER_MODELS`, ...) and where that tree is in the PDK
/// store (capability hefesto-files-and-pdks).
struct PdkPathMapping {
  platform::Path from;
  platform::Path to;
};

struct DeckOptions {
  /// Relative `.include`/`.lib` paths are tried here first, then in the
  /// schematic's folder; empty: the netlist folder is skipped.
  platform::Path netlist_folder;
  /// Absolute paths under a `from` that do not exist are tried under `to`.
  std::vector<PdkPathMapping> pdk_paths;
  /// simulation-netlist-preparation's validation; empty until M9.
  DeckValidator validate;
};

/// The area-contract entry point: the deck of `testbench` netlisted with the
/// session's search path as netlist(const Session&, ...) does (no Tcl
/// bridge), with default DeckOptions.
[[nodiscard]] diag::Outcome<Deck> build_deck(const Session& session, const Document& testbench);

/// The deck of `testbench` netlisted in `environment` (the session
/// environment of `schematic netlist`: its Tcl bridge, files and PDK root).
/// Fails with the netlister's first error, a SCHEM-SIM-001 control line, or
/// the validator's error.
[[nodiscard]] diag::Outcome<Deck> build_deck(const NetlistEnvironment& environment, const Document& testbench,
                                             const DeckOptions& options, const compat::CompatSettings& compat);

/// "Control block translation" of a whole deck text: every `.control` ...
/// `.endc` block translated line by line. Fails with SCHEM-SIM-001 on the
/// first refused command.
[[nodiscard]] diag::Outcome<Deck> translate_control(std::string_view text);

/// "Testbench code blocks": the `.include` and `.lib <file> <section>` paths
/// of `deck` (outside `.control` blocks) resolved through `fs` against
/// `options` and `schematic_folder`; an unresolved path is kept as written
/// with warning SCHEM-SIM-002.
void resolve_deck_paths(Deck& deck, platform::FileSystem& fs, const platform::Path& schematic_folder,
                        const DeckOptions& options);

/// The simulator node name of `net` seen at `path` ("Simulator vector
/// names"), counted from the top. Port nets resolve upwards through the
/// session's descents (Session::edit.hierarchy) while they match `path`.
[[nodiscard]] std::string vector_name(const Session& session, const HierPath& path, std::string_view net);

/// The device name of `device` (its instance name with any `spiceprefix`
/// applied) seen at `path`: the path prefix of "Simulator vector names" rule
/// 3, then the name, lowercased.
[[nodiscard]] std::string device_vector_name(const HierPath& path, std::string_view device);

}  // namespace hefesto::schem
