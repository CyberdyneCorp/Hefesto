// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader over a token stream (capability lef-read "Supported LEF
// file structure", "LEF LAYER section", "LEF NONDEFAULTRULE section";
// design D6): LefRead's top-level loop, with one handler per section.
// lef::read (lef/read/read.hpp) opens the file, prints the read's
// messages and totals and calls read_stream.
//
// Not implemented yet (they skip their section to its `END <name>`):
// - VIA and VIARULE sections, and a new VIA inside a NONDEFAULTRULE: LEFVIA
//   (tasks.md 4.1);
// - MACRO and SITE sections: LEFR (tasks.md 5.4, 5.5).
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/lef/lex/statements.hpp"
#include "hefesto/lef/read/layers.hpp"
#include "hefesto/lef/tech/table.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::lef {

/// The session state a LEF read changes.
struct StreamTarget {
  LayerAccess& layers;
  ViaTable& vias;
  RuleTable& rules;
};

/// What a LEF read depends on.
struct StreamOptions {
  const tech::Technology& tech;
  float scale = 1.0F;  // lef_scale of the current output style
  compat::CompatSettings compat;
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
