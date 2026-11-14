// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell kinds and structure naming of the GDS writer (gds-write "Structure
// naming", Q-MASK-209). Ports Magic 8.3.694 (fc5daa9e) calma/CalmaWrite.c
// calmaOutStructName and the property reads of calmaProcessDef, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). The collision renaming is Hefesto's fix.
// Owner: GDSW.

#include <string>
#include <variant>

#include "codes.hpp"
#include "writer.hpp"

namespace hefesto::gds::detail {

namespace {

constexpr std::size_t kCalmaNameLength = 32;  // CALMANAMELENGTH

std::string payload_key(const ElementWriter& out, std::string_view text) {
  const EncodedString encoded =
      encode_string(text, out.table(), string_options(out.style(), out.settings()), out.compat());
  std::string key;
  key.reserve(encoded.data.size());
  for (const std::byte b : encoded.data) {
    key.push_back(static_cast<char>(b));
  }
  return key;
}

std::string numbered_name(std::int64_t id) {
  return "XXXXX" + std::to_string(id < 0 ? -id : id);
}

// calmaOutStructName's per-character warnings, up to the first character
// that makes the name illegal.
void warn_changed_characters(ElementWriter& out, const std::string& name) {
  for (const char ch : name) {
    const char mapped = out.table().map(std::string_view{&ch, 1}).front();
    if (static_cast<unsigned char>(ch) > 127 || mapped == '\0') {
      return;
    }
    if (mapped != ch) {
      out.warn("Warning: character '" + std::string(1, ch) + "' changed to '" + std::string(1, mapped) +
               "' in name " + name);
    }
  }
}

}  // namespace

CellKind cell_kind(const db::CellDef& def) {
  const db::PropertyStore& p = def.properties();
  CellKind kind;
  kind.abstract = p.find("LEFview") != nullptr;
  kind.has_content = p.find("GDS_START") != nullptr;
  kind.has_end = p.find("GDS_END") != nullptr;
  kind.needs_hier = p.find("CIFhier") != nullptr;
  const db::PropertyValue* file = p.find("GDS_FILE");
  kind.read_only = file != nullptr && std::holds_alternative<std::string>(*file);
  return kind;
}

bool legal_name(const ElementWriter& out, std::string_view name) {
  for (const char ch : name) {
    const auto c = static_cast<unsigned char>(ch);
    if (c > 127 || out.table().map(std::string_view{&ch, 1}).front() == '\0') {
      return false;
    }
  }
  return !out.style().has_option(mask::style_option::string_limit) || name.size() <= kCalmaNameLength;
}

std::string StructureNames::choose(ElementWriter& out, const db::CellDef& def, std::int64_t id, bool legal) {
  std::string candidate = legal ? def.name() : numbered_name(id);
  const auto taken_by_other = [&](const std::string& text) {
    const auto it = taken_.find(payload_key(out, text));
    return it != taken_.end() && it->second != &def;
  };
  if (taken_by_other(candidate)) {
    const std::string base = numbered_name(id);
    candidate = base;
    for (std::size_t k = 1; taken_by_other(candidate); ++k) {
      candidate = base + "_" + std::to_string(k);
    }
    const auto first = taken_.find(payload_key(out, legal ? def.name() : base));
    const std::string other = first != taken_.end() ? first->second->name() : std::string{};
    out.warn("Warning: structure name of cell " + def.name() + " collides with that of cell " + other +
                 "; cell " + def.name() + " is written as '" + candidate + "'",
             codes::kNameCollision);
  }
  taken_.emplace(payload_key(out, candidate), &def);
  chosen_.emplace(&def, candidate);
  return candidate;
}

diag::Status StructureNames::write(ElementWriter& out, RecordType type, const db::CellDef& def, std::int64_t id) {
  const bool legal = legal_name(out, def.name());
  const auto found = chosen_.find(&def);
  const std::string text = found != chosen_.end() ? found->second : choose(out, def, id, legal);
  warn_changed_characters(out, def.name());
  if (!legal) {
    out.warn("Warning: string in output unprintable; changed to '" + numbered_name(id) + "'");
  }
  return out.string_record(type, text);
}

}  // namespace hefesto::gds::detail
