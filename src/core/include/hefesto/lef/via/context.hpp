// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the LEF/DEF via code reads besides its arguments
// (openspec/changes/m6-lef-def-oasis/design.md D4, D6, D8): the technology
// (contact types and residues), the current output style (cut growing,
// Q-LEFDEF-17: CIFCurStyle, not the input style), the compat settings and
// where the messages go. The LEF and DEF readers pass a sink that adds
// Magic's "LEF Read, Line N:" / "DEF Read, Line N:" context and counts the
// messages (lef::MessageCounter); the texts here are the message bodies.
//
// Owner: LEFVIA (design D2).

#pragma once

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/diag/sink.hpp"

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

}  // namespace hefesto::lef
