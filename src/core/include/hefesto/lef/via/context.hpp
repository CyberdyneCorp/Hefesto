// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the LEF/DEF via code reads besides its arguments
// (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D6, D8): the technology
// (contact types and residues), the current output style (cut growing,
// Q-LEFDEF-17: CIFCurStyle, not the input style), the compat settings and
// where the messages go. The LEF and DEF readers pass a sink that adds
// Magic's "LEF read, Line N" / "DEF read, Line N" context and counts the
// messages (lef::MessageCounter); the texts here are the message bodies.
// The prefix is not always the reader's: the sink takes it from
// via_message_dialect(code), so cut growing reports "LEF read" inside
// `def read` too, as Magic's LefGrowVia does.
//
// Owner: LEFVIA (design D2).

#pragma once

#include <optional>
#include <string_view>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lef/lex/lexer.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

struct ViaContext {
  const tech::Technology& tech;
  /// The current output style (CIFCurStyle); nullptr when none is loaded.
  const mask::OutputStyle* style = nullptr;
  const compat::CompatSettings& compat;
  diag::DiagnosticSink& sink;
};

/// The dialect Magic prints a LEF-VIA message under, whichever reader runs
/// the via code: LEF for "Cut type redefined" (LEF-VIA-001) and cut growing
/// (LEF-VIA-003/004; LefGrowVia uses LEF_WARNING in `def read` too), DEF
/// for the DEF VIAS messages (LEF-VIA-002, 005, 006; defRead.c:1987,
/// 2058-2073). nullopt: a message Magic does not print, which takes the
/// reader's own dialect.
[[nodiscard]] std::optional<Dialect> via_message_dialect(std::string_view code) noexcept;

}  // namespace hefesto::lef
