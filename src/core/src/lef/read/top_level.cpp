// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LefRead's top-level loop (lefRead.c:2956-3196; capability lef-read
// "Supported LEF file structure") and the reader's message helpers.
//
// Where Magic dereferences a missing token at the end of the input (a
// section keyword as the last token), the read stops instead (X-2 tier 1).
//
// Owner: LEFR (design D2).

#include <array>

#include "lef/read/reader.hpp"
#include "hefesto/lef/read/units.hpp"

namespace hefesto::lef {

namespace detail {

namespace {

// LefRead's `sections` table, in its order.
enum class Section : std::uint8_t {
  version,
  busbitchars,
  dividerchar,
  manufacturinggrid,
  useminspacing,
  clearancemeasure,
  namescasesensitive,
  propertydefinitions,
  units,
  layer,
  via,
  viarule,
  nondefaultrule,
  nowireextensionatpin,
  spacing,
  site,
  noisetable,
  correctiontable,
  irdrop,
  array,
  timing,
  beginext,
  macro,
  end,
};

constexpr std::array<std::string_view, 24> kSections{
    "VERSION",   "BUSBITCHARS",          "DIVIDERCHAR", "MANUFACTURINGGRID", "USEMINSPACING",   "CLEARANCEMEASURE",
    "NAMESCASESENSITIVE", "PROPERTYDEFINITIONS", "UNITS", "LAYER", "VIA", "VIARULE", "NONDEFAULTRULE",
    "NOWIREEXTENSIONATPIN", "SPACING", "SITE", "NOISETABLE", "CORRECTIONTABLE", "IRDROP", "ARRAY", "TIMING",
    "BEGINEXT", "MACRO", "END",
};

constexpr std::string_view kErrorCode = "LEF-READ-001";
constexpr std::string_view kWarningCode = "LEF-READ-002";
constexpr std::string_view kInfoCode = "LEF-READ-003";

// Statements read to their `;` and otherwise ignored.
bool ignored_statement(Section section) noexcept {
  switch (section) {
    case Section::version:
    case Section::busbitchars:
    case Section::dividerchar:
    case Section::manufacturinggrid:
    case Section::useminspacing:
    case Section::clearancemeasure:
    case Section::namescasesensitive:
    case Section::nowireextensionatpin:
      return true;
    default:
      return false;
  }
}

// Sections skipped to `END <keyword>` (BEGINEXT to ENDEXT).
bool skipped_section(Section section) noexcept {
  switch (section) {
    case Section::propertydefinitions:
    case Section::units:
    case Section::spacing:
    case Section::noisetable:
    case Section::correctiontable:
    case Section::irdrop:
    case Section::array:
    case Section::timing:
    case Section::beginext:
      return true;
    default:
      return false;
  }
}

}  // namespace

void Reader::error(std::string_view text) const {
  messages_.report(Dialect::lef, diag::Severity::error, kErrorCode, text);
}

void Reader::warning(std::string_view text) const {
  messages_.report(Dialect::lef, diag::Severity::warning, kWarningCode, text);
}

void Reader::info(std::string_view text, Dialect dialect) const {
  messages_.report(dialect, diag::Severity::info, kInfoCode, text);
}

std::optional<db::Coord> Reader::read_value(bool half_units) {
  const auto value_token = token();
  if (!value_token) {
    return std::nullopt;
  }
  const auto value = parse_number(value_token->text);
  if (!value) {
    return std::nullopt;
  }
  return to_internal(half_units ? 2.0F * *value : *value, options_.scale);
}

StreamResult Reader::run() {
  while (const auto next = token()) {
    if (!top_statement(*next)) {
      break;
    }
  }
  return StreamResult{lexer_.line(), end_library_};
}

// One top-level statement; false when reading stops.
bool Reader::top_statement(const Token& keyword) {
  const auto index = lookup_full(keyword.text, kSections);
  if (!index) {
    info("Unknown keyword \"" + keyword.text + "\" in LEF file; ignoring.");
    skip_statement();
    return true;
  }
  const auto section = static_cast<Section>(*index);
  if (ignored_statement(section)) {
    skip_statement();
    return true;
  }
  if (skipped_section(section)) {
    skip_section(kSections[*index]);
    return true;
  }
  if (section == Section::end) {
    return end_statement();
  }
  if (section == Section::layer) {
    layer_section();
    return true;
  }
  const auto name = token();
  if (!name) {
    return false;
  }
  if (section == Section::nondefaultrule) {
    nondefault_rule(truncate_name(name->text));
  } else if (section == Section::via || section == Section::viarule) {
    via_section(name->text, section == Section::viarule);
  } else {
    macro_section(name->text);  // SITE, MACRO
  }
  return true;
}

// `END LIBRARY` ends the read; `END END` too, as in Magic.
bool Reader::end_statement() {
  if (parse_end("LIBRARY") == EndMatch::not_matched) {
    error("END statement out of context.");
    return true;
  }
  end_library_ = true;
  return false;
}

}  // namespace detail

StreamResult read_stream(Lexer& lexer, const MessageTarget& messages, const StreamOptions& options,
                         StreamTarget& target) {
  detail::Reader reader{lexer, messages, options, target};
  return reader.run();
}

}  // namespace hefesto::lef
