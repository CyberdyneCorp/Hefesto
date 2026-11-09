// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Text and JSON-lines renderings of diagnostics (capability
// hefesto-diagnostics, requirement "Diagnostic presentation and logging").

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::diag {

/// `magic` renders a diagnostic the way Magic prints the same message: only
/// its text, with TechPrintLine's "<file>: line <n>: section <s>:" line for
/// technology-file diagnostics (golden replay against Magic).
enum class Format { text, json, magic };

/// Parses "text", "json" or "magic".
[[nodiscard]] std::optional<Format> parse_format(std::string_view name) noexcept;

/// `<severity>: <code>: <text>[ at <file>:<line>| in <cell> (<x>, <y>)]`
/// with no trailing newline.
[[nodiscard]] std::string format_text(const Diagnostic& diagnostic);

/// One JSON object with keys in the fixed order severity, code, text,
/// location, related; absent optional fields are `null`. No trailing newline,
/// no insignificant whitespace, so the output is byte-stable across platforms.
/// Invalid UTF-8 in strings is replaced by U+FFFD.
[[nodiscard]] std::string format_json(const Diagnostic& diagnostic);

/// Magic's console text: `text`, preceded by "<file>: line <n>: section
/// <s>:\n\t" (utils/tech.c TechPrintLine) when the diagnostic has a source
/// location with a line and a related `section` object. No severity, no code.
[[nodiscard]] std::string format_magic(const Diagnostic& diagnostic);

[[nodiscard]] std::string format(const Diagnostic& diagnostic, Format fmt);

}  // namespace hefesto::diag
