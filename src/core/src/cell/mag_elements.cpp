// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Persistent display elements in .mag files (cell-file-format "Element
// records", "Writer elements"). Ported from Magic 8.3.694 database/DBio.c
// (dbReadElements), dbwind/DBWelement.c (DBWElementParseFlags,
// DBWPrintElements), utils/geometry.c (GeoNameToPos, GeoPosToName) and
// utils/lookup.c (Lookup); Magic is (c) the Regents of the University of
// California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).

#include "mag_elements.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <span>
#include <vector>

#include "hefesto/db/labels.hpp"
#include "hefesto/util/record_order.hpp"
#include "mag_input.hpp"

namespace hefesto::cell::detail {

namespace {

/// utils/lookup.c Lookup(): case-insensitive prefix match, an exact match
/// wins; -1 when ambiguous or unknown.
int lookup(std::string_view word, std::span<const std::string_view> table) {
  int match = -1;
  bool ambiguous = false;
  for (std::size_t i = 0; i < table.size(); ++i) {
    const std::string_view entry = table[i];
    if (word.size() > entry.size()) {
      continue;
    }
    const bool prefix = std::equal(word.begin(), word.end(), entry.begin(), [](char a, char b) {
      return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
    if (!prefix) {
      continue;
    }
    if (word.size() == entry.size()) {
      return static_cast<int>(i);
    }
    ambiguous = match >= 0;
    match = static_cast<int>(i);
  }
  return ambiguous ? -1 : match;
}

constexpr std::array<std::string_view, 2> kGeneral = {"persistent", "temporary"};
constexpr std::array<std::string_view, 5> kTextSizes = {"small", "medium", "large", "xlarge", "default"};
constexpr std::array<std::string_view, 12> kLineFlags = {"halfx",     "halfy",       "exactx",     "exacty",
                                                          "arrowleft", "arrowbottom", "arrowright", "arrowtop",
                                                          "plainleft", "plainbottom", "plainright", "plaintop"};
// GeoNameToPos's table (names and GEO positions), in its order.
constexpr std::array<std::string_view, 33> kPositionNames = {
    "bl",   "bottom", "br",        "center",    "d",  "dl", "down",      "dr",        "e",  "east", "left",
    "n",    "ne",     "north",     "northeast", "northwest", "nw", "right",     "s",         "se", "south", "southeast",
    "southwest", "sw", "tl",       "top",       "tr", "u",  "ul",        "up",        "ur", "w",    "west"};
constexpr std::array<std::uint8_t, 33> kPositionValues = {6, 5, 4, 0, 5, 6, 5, 4, 3, 3, 7, 1, 2, 1, 2, 8, 8,
                                                          3, 5, 4, 5, 4, 6, 6, 8, 1, 2, 1, 8, 1, 2, 7, 7};
constexpr std::array<std::string_view, 9> kPositionWords = {"CENTER", "NORTH",     "NORTHEAST", "EAST",     "SOUTHEAST",
                                                            "SOUTH",  "SOUTHWEST", "WEST",      "NORTHWEST"};

void apply_text_flag(db::Element& e, std::string_view flag) {
  if (const int size = lookup(flag, kTextSizes); size >= 0) {
    e.text_size = static_cast<std::uint8_t>(size);
  } else if (const int pos = lookup(flag, kPositionNames); pos >= 0) {
    e.text_position = kPositionValues[static_cast<std::size_t>(pos)];
  }
}

void apply_line_flag(db::Element& e, std::string_view flag) {
  switch (lookup(flag, kLineFlags)) {
    case 0:
      e.half_x = true;
      break;
    case 1:
      e.half_y = true;
      break;
    case 2:
      e.half_x = false;
      break;
    case 3:
      e.half_y = false;
      break;
    case 4:
    case 5:
      e.arrow_left = true;
      break;
    case 6:
    case 7:
      e.arrow_right = true;
      break;
    case 8:
    case 9:
      e.arrow_left = false;
      break;
    case 10:
    case 11:
      e.arrow_right = false;
      break;
    default:
      break;
  }
}

void apply_flag_list(db::Element& e, std::string_view flags) {
  while (!flags.empty() && is_space(static_cast<unsigned char>(flags.front()))) {
    flags.remove_prefix(1);
  }
  while (true) {
    const std::size_t comma = flags.find(',');
    apply_element_flag(e, flags.substr(0, comma));
    if (comma == std::string_view::npos) {
      return;
    }
    flags.remove_prefix(comma + 1);
  }
}

std::vector<std::string> split_styles(std::string_view styles) {
  std::vector<std::string> out;
  while (true) {
    const std::size_t comma = styles.find(',');
    out.emplace_back(styles.substr(0, comma));
    if (comma == std::string_view::npos) {
      return out;
    }
    styles.remove_prefix(comma + 1);
  }
}

/// `rectangle|line <name> <styles> x1 y1 x2 y2 [flags]`.
std::optional<db::Element> parse_box_element(std::string_view line, db::Element::Kind kind) {
  Scan s{line};
  db::Element e;
  e.kind = kind;
  std::string styles;
  std::int32_t v[4] = {};
  const std::string_view word = kind == db::Element::Kind::line ? "line" : "rectangle";
  if (!s.lit(word) || !s.str(e.name, 127) || !s.str(styles, 1023) || !s.integer(v[0]) || !s.integer(v[1]) ||
      !s.integer(v[2]) || !s.integer(v[3])) {
    return std::nullopt;
  }
  e.area = db::Rect{v[0], v[1], v[2], v[3]};
  e.styles = split_styles(styles);
  s.ws();
  if (std::string flags; s.rest(flags, 99)) {
    apply_flag_list(e, flags);
  }
  return e;
}

/// `text <name> <styles> x y "<text>" [flags]`.
std::optional<db::Element> parse_text_element(std::string_view line) {
  Scan s{line};
  db::Element e;
  e.kind = db::Element::Kind::text;
  std::string styles;
  std::int32_t x = 0;
  std::int32_t y = 0;
  if (!s.lit("text") || !s.str(e.name, 127) || !s.str(styles, 1023) || !s.integer(x) || !s.integer(y)) {
    return std::nullopt;
  }
  const std::size_t first = line.find('"');
  const std::size_t last = line.rfind('"');
  if (first == std::string_view::npos || last == first) {
    return std::nullopt;
  }
  e.area = db::Rect{x, y, x, y};
  e.text = std::string{line.substr(first + 1, last - first - 1)};
  e.styles = split_styles(styles);
  Scan tail{line.substr(last + 1)};
  if (std::string flags; tail.rest(flags, 99)) {
    apply_flag_list(e, flags);
  }
  return e;
}

std::string flag_suffix(const std::vector<std::string_view>& flags) {
  std::string out;
  for (const std::string_view f : flags) {
    out += out.empty() ? " " : ",";
    out += f;
  }
  return out;
}

std::string element_record(const db::Element& e, std::int64_t r) {
  const auto num = [r](std::int32_t v) { return std::to_string(v / r); };
  static constexpr std::array<std::string_view, 3> kWords = {"rectangle", "line", "text"};
  std::string out = std::string{kWords[static_cast<std::size_t>(e.kind)]} + " " + e.name + " ";
  for (std::size_t i = 0; i < e.styles.size(); ++i) {
    out += (i == 0 ? "" : ",") + e.styles[i];
  }
  out += " " + num(e.area.x0) + " " + num(e.area.y0);
  std::vector<std::string_view> flags;
  switch (e.kind) {
    case db::Element::Kind::rectangle:
      return out + " " + num(e.area.x1) + " " + num(e.area.y1) + "\n";
    case db::Element::Kind::line:
      out += " " + num(e.area.x1) + " " + num(e.area.y1);
      for (const auto& [on, word] : {std::pair{e.half_x, "halfx"}, std::pair{e.half_y, "halfy"},
                                     std::pair{e.arrow_left, "arrowleft"}, std::pair{e.arrow_right, "arrowright"}}) {
        if (on) {
          flags.emplace_back(word);
        }
      }
      return out + flag_suffix(flags) + "\n";
    case db::Element::Kind::text:
      if (e.text_position != 0 && e.text_position < kPositionWords.size()) {
        flags.push_back(kPositionWords[e.text_position]);
      }
      if (e.text_size != 1 && e.text_size < kTextSizes.size()) {
        flags.push_back(kTextSizes[e.text_size]);
      }
      return out + " \"" + e.text + "\"" + flag_suffix(flags) + "\n";
  }
  return out + "\n";
}

}  // namespace

std::optional<db::Element> parse_element(std::string_view line) {
  switch (line.empty() ? '\0' : line.front()) {
    case 'r':
      return parse_box_element(line, db::Element::Kind::rectangle);
    case 'l':
      return parse_box_element(line, db::Element::Kind::line);
    case 't':
      return parse_text_element(line);
    default:
      return std::nullopt;
  }
}

void apply_element_flag(db::Element& element, std::string_view flag) {
  switch (lookup(flag, kGeneral)) {
    case 0:
      element.persistent = true;
      return;
    case 1:
      element.persistent = false;
      return;
    default:
      break;
  }
  if (element.kind == db::Element::Kind::text) {
    apply_text_flag(element, flag);
  } else if (element.kind == db::Element::Kind::line) {
    apply_line_flag(element, flag);
  }
}

void put_element(db::CellDef& def, db::Element element) {
  auto& list = def.elements();
  const auto it =
      std::find_if(list.begin(), list.end(), [&](const db::Element& e) { return e.name == element.name; });
  if (it != list.end()) {
    *it = std::move(element);
  } else {
    list.push_back(std::move(element));
  }
}

std::string element_section(const db::CellDef& def, std::int64_t r) {
  std::vector<const db::Element*> order;
  for (const db::Element& e : def.elements()) {
    if (e.persistent) {
      order.push_back(&e);
    }
  }
  if (order.empty()) {
    return {};
  }
  std::stable_sort(order.begin(), order.end(), [](const db::Element* a, const db::Element* b) {
    return util::compare_natural(a->name, b->name) < 0;
  });
  std::string out = "<< elements >>\n";
  for (const db::Element* e : order) {
    out += element_record(*e, r);
  }
  return out;
}

}  // namespace hefesto::cell::detail
