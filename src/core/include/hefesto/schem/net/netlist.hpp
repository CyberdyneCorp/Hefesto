// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// In-process SPICE netlisting (openspec/changes/m8-schematic design.md D9,
// "Area contracts" NETLIST; capability schematic-netlisting "Netlist
// options", "Netlist file structure", "Subcircuit body", "Top-level port
// order", "Subcircuit definitions", "Schematic resolution", "Device models",
// "LVS netlist mode", "Flat netlist", "Errors and warnings", "In-process
// netlisting").
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/postprocess.hpp"

namespace hefesto::schem {

struct Session;
class SearchPath;
class SymbolCache;
class TclBridge;

/// `hierarchy` option of "Netlist options".
enum class Hierarchy : std::uint8_t { hier, top, flat };

/// The run options of "Netlist options", with their defaults; a
/// `netlist_options` instance in the top schematic overrides them per run.
struct NetlistOptions {
  Hierarchy hierarchy = Hierarchy::hier;
  bool lvs_netlist = false;
  bool top_is_subckt = false;
  bool lvs_ignore = false;
  bool spiceprefix = true;
  std::string bus_replacement_char;  // empty, or exactly two characters
  bool uppercase_subckt = false;
  std::string hiersep = ".";
  /// The project setting `portable-netlist-paths` ("Netlist file structure",
  /// Q-SCHEM-13).
  bool portable_paths = false;

  friend bool operator==(const NetlistOptions&, const NetlistOptions&) = default;
};

/// What a computed `lab` value is stored into (Q-SCHEM-29).
enum class LabelTarget : std::uint8_t { instance, wire };

/// A `lab` value computed while netlisting (Q-SCHEM-29): for the instance
/// or wire at index `instance` of the schematic `schematic`. Wires get their
/// net name (an unnamed net with its `#`), labels and ports without `lab`
/// their net name without `#`. Storing them is the caller's job (set_token,
/// without marking the document modified).
struct ComputedLabel {
  platform::Path schematic;
  std::size_t instance = 0;
  std::string lab;
  LabelTarget target = LabelTarget::instance;

  friend bool operator==(const ComputedLabel&, const ComputedLabel&) = default;
};

struct NetlistResult {
  std::string text;  // the whole netlist, LF line ends
  std::vector<diag::Diagnostic> diagnostics;
  bool failed = false;  // an error of "Errors and warnings"
  std::vector<ComputedLabel> labels;
};

/// Deepest subcircuit nesting "Flat netlist" expands (Q-SCHEM-42, user
/// decision M8-D1 of 2026-11-22; both modes): an instance below it is kept
/// as an `X` line with warning SCHEM-NET-017 (a subcircuit that places
/// itself would otherwise expand forever).
inline constexpr int kMaxFlatDepth = 64;

/// Most bytes "Flat netlist" writes (Q-SCHEM-42, M8-D1; both modes): 256
/// MiB. Each kept line counts its text, its LF and kFlatLineOverhead (its
/// in-memory record), and each expanded instance counts its own line too,
/// so a hierarchy of empty subcircuits cannot expand forever. When a line
/// would pass it, substitution stops with warning SCHEM-NET-017: the
/// instances not yet substituted are written as their X lines, every other
/// line is still written (the output stays a complete netlist ending in
/// `.end`), and the run does not fail.
inline constexpr std::size_t kMaxFlatBytes = std::size_t{256} << 20;

/// The bytes each kept flat-netlist line adds to its text against
/// kMaxFlatBytes: at least sizeof(ProcessedLine) on every supported
/// standard library (24-byte libc++ and 32-byte libstdc++ strings).
inline constexpr std::size_t kFlatLineOverhead = 40;

/// The limits of flatten_netlist(); tests lower them.
struct FlatLimits {
  int depth = kMaxFlatDepth;
  std::size_t bytes = kMaxFlatBytes;
};

struct FlatNetlist {
  std::vector<ProcessedLine> lines;
  /// SCHEM-NET-017 (at most one per limit).
  std::vector<diag::Diagnostic> warnings;
};

/// What netlisting reads besides the top document: the symbol cache and
/// search path (symbols and subcircuit schematics are resolved like symbol
/// references), the files, the Tcl bridge for `tcleval` values (nullptr: every
/// evaluation gives `?` LF), and the active PDK root of portable paths.
struct NetlistEnvironment {
  SymbolCache& symbols;
  const SearchPath& search_path;
  platform::FileSystem& fs;
  const TclBridge* tcl = nullptr;
  /// `$PDK_ROOT` of "portable netlist paths"; empty when no PDK is active.
  platform::Path pdk_root;
  /// The `ev_precision` setting used by `expr_eng`.
  int ev_precision = 4;
  /// Bytes of expanded bus text the run may produce in total (Q-SCHEM-41).
  std::uint64_t bus_budget = kMaxBusTotalBytes;
  /// The limits of the flat netlist (Q-SCHEM-42).
  FlatLimits flat_limits{};
};

/// Netlists `top` in memory ("In-process netlisting"): never writes a file,
/// never changes a document or its undo history. `top.path` names the top
/// cell and the `sch_path` line; relative references resolve against its
/// folder.
[[nodiscard]] NetlistResult netlist(const NetlistEnvironment& environment, const Document& top,
                                    const NetlistOptions& options, const compat::CompatSettings& compat);

/// The area-contract entry point: netlists `top` with the search path of
/// `session`, a fresh symbol cache (the session's is left untouched), the
/// local file system and no Tcl bridge (`tcleval` values give `?`). Callers
/// that have a platform file system or a Tcl bridge use the overload above.
[[nodiscard]] NetlistResult netlist(const Session& session, const Document& top, const NetlistOptions& options,
                                    const compat::CompatSettings& compat);

/// "Netlist file structure": the text of a `sch_path` or `sym_path` comment
/// line for `file`: the absolute path, or with `portable` the path as
/// `$PDK_ROOT/<path>` when it is inside `pdk_root`, else relative to
/// `top_folder` (Q-SCHEM-13).
[[nodiscard]] std::string netlist_path(const platform::Path& file, bool portable, const platform::Path& pdk_root,
                                       const platform::Path& top_folder);

/// "Device models": the key of a `device_model` value.
[[nodiscard]] std::string device_model_key(std::string_view value);

/// "Device models": the bucket (0..366) of a key.
[[nodiscard]] std::size_t device_model_bucket(std::string_view key) noexcept;

/// "Flat netlist": the post-processed hierarchical netlist `lines` with every
/// instance of a subcircuit it defines replaced by the subcircuit's body
/// (instance names suffixed `_<path>`, local nets prefixed `<path>_`, the
/// `.GLOBAL` nets and `0` kept, parameters whose whole value names a
/// subcircuit parameter replaced), and the definitions removed, within
/// `limits`.
[[nodiscard]] FlatNetlist flatten_netlist(std::span<const ProcessedLine> lines, const FlatLimits& limits = {});

}  // namespace hefesto::schem
