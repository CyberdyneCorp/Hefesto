// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader over a token stream (capability lef-read "Supported LEF
// file structure", "LEF LAYER section", "LEF NONDEFAULTRULE section";
// design D6): LefRead's top-level loop, with one handler per section.
// lef::read (lef/read/read.hpp) opens the file, prints the read's
// messages and totals and calls read_stream.
//
// VIA and VIARULE sections define vias in the target's via table through
// LEFVIA's store (lef/via/store.hpp); MACRO and SITE sections make or
// annotate cells through the target's MacroHost, and are skipped to their
// END when it has none.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/lef/lex/statements.hpp"
#include "hefesto/lef/read/layers.hpp"
#include "hefesto/lef/read/macro.hpp"
#include "hefesto/lef/tech/table.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

/// The session state a LEF read changes.
struct StreamTarget {
  LayerAccess& layers;
  ViaTable& vias;
  RuleTable& rules;
  /// The table `layers` views, for via redefinition (LefRedefined counts
  /// the names of a layer record and moves a shared record's canonical
  /// name, Q-LEFDEF-8); nullptr: an empty table.
  LayerTable* layer_table = nullptr;
  /// The cells of MACRO and SITE sections; nullptr skips those sections.
  MacroHost* macros = nullptr;
};

/// What a LEF read depends on.
struct StreamOptions {
  const tech::Technology& tech;
  float scale = 1.0F;  // lef_scale of the current output style
  compat::CompatSettings compat;
  /// The current output style (cut growing, Q-LEFDEF-17); nullptr: none.
  const mask::OutputStyle* style = nullptr;
  MacroReadOptions macros;
};

/// What a LEF read reports back.
struct StreamResult {
  std::uint32_t lines = 0;  // lines read ("LEF read: Processed <n> lines.")
  bool end_library = false;  // reading stopped at END LIBRARY
};

/// LefRead's loop: reads `lexer` to its end or to END LIBRARY, sending
/// LefError messages to `messages`. It neither prints the totals nor resets
/// the counter (lef::read does, after the "Processed" line).
StreamResult read_stream(Lexer& lexer, const MessageTarget& messages, const StreamOptions& options,
                         StreamTarget& target);

}  // namespace hefesto::lef
