// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// In-process SPICE netlisting (openspec/changes/m8-schematic design.md D9,
// "Area contracts" NETLIST; capability schematic-netlisting "Netlist
// options", "Netlist file structure", "In-process netlisting", "Errors and
// warnings").
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

struct Session;

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

/// A `lab` value computed while netlisting (Q-SCHEM-29), for the instance
/// at `instance` of the schematic `schematic`.
struct ComputedLabel {
  platform::Path schematic;
  std::size_t instance = 0;
  std::string lab;

  friend bool operator==(const ComputedLabel&, const ComputedLabel&) = default;
};

struct NetlistResult {
  std::string text;  // the whole netlist, LF line ends
  std::vector<diag::Diagnostic> diagnostics;
  bool failed = false;  // an error of "Errors and warnings"
  std::vector<ComputedLabel> labels;
};

/// Netlists `top` with the symbols of `session`. Never writes a file and
/// never changes a document.
[[nodiscard]] NetlistResult netlist(const Session& session, const Document& top, const NetlistOptions& options,
                                    const compat::CompatSettings& compat);

}  // namespace hefesto::schem
