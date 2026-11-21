// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `@` and `%` tokens in displayed texts (capability
// schematic-symbols-and-libraries "Text substitution tokens", "Floater
// texts"; Q-SCHEM-26). Netlist format strings are capability
// schematic-netlisting's (NETLIST, schem/net/substitute.hpp).
//
// Owner: SYMLIB.

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::schem {

/// What a displayed text is substituted against.
struct TextTokenContext {
  /// The owner instance; nullptr resolves the text with no instance (only
  /// instance-independent tokens apply, `@key` is empty and `%key` is
  /// `key`).
  const InstanceRecord* instance = nullptr;
  /// The instance's symbol (its template and pins); may be null.
  const Symbol* symbol = nullptr;
  /// `@index`: the instance's index in its schematic.
  std::size_t index = 0;
  /// `@path`: the hierarchical path of the current schematic without the
  /// leading `.`.
  std::string path;
  /// The current schematic and the top schematic (files; `@schname`,
  /// `@schname_ext`, `@topschname` take their names).
  platform::Path schematic;
  platform::Path top_schematic;
  /// `@sch_last_modified`, `@sym_last_modified`, `@time_last_modified`,
  /// already formatted as `%Y-%m-%d  %H:%M:%S` local time by the caller.
  std::string sch_last_modified;
  std::string sym_last_modified;
  std::string time_last_modified;
  /// `@lvs_ignore`: LVS-ignore mode.
  bool lvs_ignore = false;
  /// The net attached to a pin (`@@pin`, `@#n`, `@#n:net_name`); empty
  /// function: no net.
  std::function<std::string(PinIndex)> net;
  /// Q-SCHEM-26 warnings (SCHEM-SYM-002); nullptr drops them.
  diag::DiagnosticSink* warnings = nullptr;
};

/// The text with every token replaced ("Text substitution tokens"). A token
/// starts at an unescaped `@` or `%`; its second character always belongs
/// to it; it ends before the next unescaped blank, `@` or `%` outside
/// parentheses, or before an escaped character. A backslash escapes the
/// next character and is removed. `tcleval(` texts are evaluated by the
/// caller after substitution (schem::TclBridge).
[[nodiscard]] std::string substitute_text(std::string_view text, const TextTokenContext& context);

}  // namespace hefesto::schem
