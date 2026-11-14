// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Display-style files (capability display-style-files) and the type to
// style mapping (tech-styles-section). Ported from Magic 8.3.694
// (graphics/grCMap.c, graphics/grDStyle.c, graphics/grGlyphs.c,
// dbwind/DBWdisplay.c), Copyright (C) 1985, 1990 Regents of the University
// of California, used under Magic's permissive license (see
// THIRD_PARTY_NOTICES.md); the behavior follows the resolved open questions
// Q-UI-1..8, Q-UI-34, Q-UI-35 and Q-UI-39.

#include "hefesto/display/styles.hpp"

#include <algorithm>
#include <cctype>

#include "display/c_scan.hpp"
#include "hefesto/display/diag_codes.hpp"

namespace hefesto::display {

namespace {

namespace dc = codes;

// fgets() buffer sizes of Magic's readers.
constexpr std::size_t kCmapLine = 128;
constexpr std::size_t kDstyleLine = 200;
constexpr std::size_t kGlyphLine = 500;
constexpr std::int32_t kMaxGlyphDimension = 4096;
// Upper bound on count * width * height of a glyph file (Magic's own cursor
// files hold a few thousand pixels). A size line above it is refused instead
// of allocating memory the file never fills.
constexpr std::int64_t kMaxGlyphPixels = std::int64_t{1} << 22;

std::string quote(std::string_view s) {
  return "\"" + std::string(s) + "\"";
}

std::string trim_newline(std::string_view line) {
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
    line.remove_suffix(1);
  }
  return std::string(line);
}

// --- color map -------------------------------------------------------------

/// "%*d %*d %*d %d": the fourth integer of a line.
std::optional<std::int64_t> cmap_max_index(std::string_view line) {
  cscan::Scanner scan(line);
  for (int i = 0; i < 3; ++i) {
    if (!scan.integer(10)) {
      return std::nullopt;
    }
  }
  return scan.integer(10);
}

struct CmapLine {
  std::int64_t r = 0;
  std::int64_t g = 0;
  std::int64_t b = 0;
  std::int64_t max = 0;
  std::string name;
};

/// "%d %d %d %d %99[^\n]"; std::nullopt when fewer than four values.
std::optional<CmapLine> cmap_entry(std::string_view line) {
  cscan::Scanner scan(line);
  CmapLine out;
  for (std::int64_t* field : {&out.r, &out.g, &out.b, &out.max}) {
    const auto v = scan.integer(10);
    if (!v) {
      return std::nullopt;
    }
    *field = *v;
  }
  scan.skip_space();
  out.name = scan.until_newline(99).value_or("");
  return out;
}

bool cmap_ignorable(std::string_view line) {
  return (!line.empty() && line.front() == '#') || cscan::is_white(line, false);
}

diag::Outcome<std::size_t> cmap_size(const std::vector<std::string_view>& lines, std::string_view file_name,
                                     std::size_t max_entries) {
  std::int64_t max = 0;
  for (const auto line : lines) {
    const auto newmax = cmap_max_index(line);
    if (newmax) {
      max = std::max(max, *newmax);
      continue;
    }
    if (!cmap_ignorable(line)) {
      return diag::make(dc::kCmapSyntax, "Syntax error in color map file \"" + std::string(file_name) +
                                             "\"\nLast color read was index " + std::to_string(max));
    }
  }
  // max >= limit, not max + 1 > limit: max may be INT64_MAX (fuzz_cmap).
  if (static_cast<std::uint64_t>(max) >= max_entries) {
    return diag::make(dc::kCmapSyntax, "Color map file \"" + std::string(file_name) + "\" has too many colors");
  }
  return static_cast<std::size_t>(max + 1);
}

}  // namespace

std::string_view display_class_name(DisplayClass display_class) noexcept {
  switch (display_class) {
    case DisplayClass::bit7:
      return "7bit";
    case DisplayClass::bit24:
      return "24bit";
    case DisplayClass::bw:
      return "bw";
    case DisplayClass::opengl:
      break;
  }
  return "OpenGL";
}

std::int32_t display_plane_mask(DisplayClass display_class) noexcept {
  switch (display_class) {
    case DisplayClass::bit7:
      return 0x7F;
    case DisplayClass::bw:
      return 0x1;
    case DisplayClass::opengl:
    case DisplayClass::bit24:
      break;
  }
  return 0xFFFFFF;
}

std::optional<std::size_t> ColorMap::find(std::string_view name) const {
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].name.empty() && entries[i].name == name) {
      return i;
    }
  }
  return std::nullopt;
}

diag::Outcome<ColorMap> parse_color_map(std::string_view text, std::string_view file_name, bool /*magic_compat*/,
                                        std::size_t max_entries) {
  const auto lines = cscan::fgets_chunks(text, kCmapLine);
  const auto size = cmap_size(lines, file_name, max_entries);
  if (!size) {
    return size.error();
  }
  ColorMap map;
  map.entries.resize(size.value());
  std::size_t i = 0;
  std::size_t next_line = 0;
  while (i < map.entries.size()) {
    if (next_line >= lines.size()) {
      return diag::make(dc::kCmapShort,
                        "Premature end-of-file in color map file \"" + std::string(file_name) + "\"");
    }
    const auto line = lines[next_line++];
    const auto entry = cmap_entry(line);
    if (!entry) {
      if (cmap_ignorable(line)) {
        continue;  // comments; blank lines too (Q-UI-7)
      }
      return diag::make(dc::kCmapSyntax, "Syntax error in color map file \"" + std::string(file_name) +
                                             "\"\nExpecting to read color index " + std::to_string(i));
    }
    if (entry->max < static_cast<std::int64_t>(i)) {
      return diag::make(dc::kCmapOrder, "Colors in map are out of order!");
    }
    for (; static_cast<std::int64_t>(i) <= entry->max; ++i) {
      map.entries[i] = ColorEntry{static_cast<std::uint8_t>(entry->r & 0xFF), static_cast<std::uint8_t>(entry->g & 0xFF),
                                  static_cast<std::uint8_t>(entry->b & 0xFF), entry->name};
    }
  }
  return map;
}

std::string write_color_map(const ColorMap& map) {
  std::string out;
  const auto& e = map.entries;
  for (std::size_t i = 0; i < e.size(); ++i) {
    const ColorEntry& first = e[i];
    while (i + 1 < e.size() && e[i + 1].r == first.r && e[i + 1].g == first.g && e[i + 1].b == first.b) {
      ++i;
    }
    out += std::to_string(first.r) + " " + std::to_string(first.g) + " " + std::to_string(first.b) + " " +
           std::to_string(i);
    if (!first.name.empty()) {
      out += " " + first.name;
    }
    out += "\n";
  }
  return out;
}

// --- dstyle ------------------------------------------------------------------

namespace {

enum Section : int { kNone = 0, kDisplay = 1, kLayout = 2, kPale = 4, kStipples = 8, kVersion = 16, kIgnore = -1 };

/// LookupFull over fillStyles: an exact or case-insensitive full match.
std::optional<FillKind> parse_fill(std::string_view word) {
  static constexpr std::array<std::string_view, 5> kNames = {"solid", "cross", "outline", "stipple", "grid"};
  for (std::size_t i = 0; i < kNames.size(); ++i) {
    const bool same = word.size() == kNames[i].size() &&
                      std::equal(word.begin(), word.end(), kNames[i].begin(), [](char a, char b) {
                        return std::toupper(static_cast<unsigned char>(a)) == std::toupper(static_cast<unsigned char>(b));
                      });
    if (same) {
      return static_cast<FillKind>(i);
    }
  }
  return std::nullopt;
}

/// The fields of one style line as sscanf() reads them, and how many it read.
struct StyleFields {
  int count = 0;
  std::string ordinal;
  std::int64_t mask = 0;
  std::string color;
  std::int64_t outline = 0;
  std::string fill;
  std::int64_t stipple = 0;
  char short_name = ' ';
  std::string long_name;
};

/// "%10s %o %29s %o %40s %d %c %50s" (version <= 6) or the %i variant.
StyleFields scan_style_line(std::string_view line, int version) {
  const int base = version > 6 ? 0 : 8;
  cscan::Scanner scan(line);
  StyleFields f;
  auto take = [&f](auto&& value, auto& field) {
    if (!value) {
      return false;
    }
    field = *value;
    ++f.count;
    return true;
  };
  if (!take(scan.word(10), f.ordinal) || !take(scan.integer(base), f.mask) || !take(scan.word(29), f.color) ||
      !take(scan.integer(base), f.outline) || !take(scan.word(40), f.fill) || !take(scan.integer(10), f.stipple)) {
    return f;
  }
  scan.skip_space();
  if (!take(scan.character(), f.short_name)) {
    return f;
  }
  take(scan.word(50), f.long_name);
  return f;
}

class DstyleReader {
 public:
  DstyleReader(std::string_view file_name, const ColorMap& colors, bool compat, diag::DiagnosticSink& sink,
               std::int32_t plane_mask, int version)
      : file_name_(file_name), colors_(colors), compat_(compat), sink_(sink), plane_mask_(plane_mask) {
    table_.version = version;
  }

  void line(std::string_view text) {
    if (cscan::is_white(text, false)) {
      new_section_ = true;
    } else if (text.front() == '#') {
      // comment: keeps the "next line is a header" state
    } else if (new_section_) {
      header(text);
    } else {
      data(text);
    }
  }

  diag::Outcome<StyleTable> finish();

 private:
  void header(std::string_view text);
  void header_word(const std::string& name, std::string_view text);
  void layout_header();
  void data(std::string_view text);
  void style_line(std::string_view text);
  void stipple_line(std::string_view text);
  std::optional<std::int32_t> resolve_color(const std::string& word);
  void fail(std::string_view code, std::string text);
  void warn(std::string_view code, std::string text) { sink_.emit(diag::make(code, std::move(text))); }

  std::string file_name_;
  const ColorMap& colors_;
  bool compat_;
  diag::DiagnosticSink& sink_;
  std::int32_t plane_mask_;
  StyleTable table_;
  bool new_section_ = true;  // Q-UI-4: the first non-comment line is a header
  int section_ = kNone;
  int processed_ = kVersion;
  std::size_t scount_ = 0;
  std::size_t max_tech_styles_ = 0;
  std::int32_t next_ordinal_ = 1;
  std::vector<DisplayStyle> records_;  // every style line, in file order
  std::optional<diag::Diagnostic> error_;
};

void DstyleReader::fail(std::string_view code, std::string text) {
  auto d = diag::make(code, std::move(text));
  d.severity = diag::Severity::error;
  if (!error_) {
    error_ = d;
  } else {
    sink_.emit(d);
  }
}

void DstyleReader::header(std::string_view text) {
  cscan::Scanner scan(text);
  const std::string name = scan.word(text.size()).value_or("");
  if (name != "version" && name != "display_styles" && name != "layout_styles" && name != "pale_styles" &&
      name != "stipples" && cscan::is_int(name)) {
    warn(dc::kDstyleEmptyLine, "Unexpected empty line in .dstyle file.");
    new_section_ = false;
    data(text);
    return;
  }
  header_word(name, text);
  new_section_ = false;
  processed_ |= section_;
}

void DstyleReader::header_word(const std::string& name, std::string_view text) {
  if (name == "version") {
    cscan::Scanner scan(text);
    (void)scan.word(text.size());
    const auto v = scan.integer(10);
    if (!v) {
      warn(dc::kDstyleVersion, "DStyle format version could not be read: assuming version 6");
    }
    table_.version = v ? static_cast<int>(*v) : 6;
    section_ = kVersion;
  } else if (name == "display_styles") {
    if ((processed_ & (kLayout | kPale)) != 0) {
      fail(dc::kDstyleOrder,
           "DStyle sections out of order: display_styles must come before layout_styles and pale_styles");
    }
    section_ = kDisplay;
    scount_ = 0;
  } else if (name == "layout_styles") {
    layout_header();
  } else if (name == "pale_styles") {
    section_ = kPale;
    max_tech_styles_ = scount_ + kFixedStyleCount;
    scount_ = 0;
  } else if (name == "stipples") {
    section_ = kStipples;
    table_.stipples.clear();
  } else {
    fail(dc::kDstyleSection, "Bad section name " + quote(name) + " in .dstyle file.");
    section_ = kIgnore;
  }
}

void DstyleReader::layout_header() {
  if ((processed_ & kPale) != 0) {  // Magic exits; Hefesto rejects the file (Q-UI-2)
    fail(dc::kDstyleOrder, "DStyle sections out of order: layout_styles must come before pale_styles");
  }
  section_ = kLayout;
  if (scount_ != kFixedStyleCount) {
    const std::string text = scount_ < kFixedStyleCount
                                 ? "Error: Display style file defines only " + std::to_string(scount_) + " of " +
                                       std::to_string(kFixedStyleCount) + " required internal styles."
                                 : "Error: Display style file defines too many (" + std::to_string(scount_) +
                                       ") internal styles; should be " + std::to_string(kFixedStyleCount) + ".";
    if (compat_) {
      warn(dc::kDstyleCount, text);
    } else {
      fail(dc::kDstyleCount, text);
    }
  }
  scount_ = 0;
}

void DstyleReader::data(std::string_view text) {
  switch (section_) {
    case kDisplay:
    case kLayout:
    case kPale:
      style_line(text);
      ++scount_;
      break;
    case kStipples:
      stipple_line(text);
      break;
    default:
      break;  // version, ignored and (Q-UI-4) no section yet
  }
}

std::optional<std::int32_t> DstyleReader::resolve_color(const std::string& word) {
  cscan::Scanner scan(word);
  if (const auto v = scan.integer(table_.version > 6 ? 0 : 8)) {
    return static_cast<std::int32_t>(*v);
  }
  if (const auto index = colors_.find(word)) {
    return static_cast<std::int32_t>(*index);
  }
  if (!compat_) {
    return std::nullopt;  // Q-UI-5: unknown color names are errors without magic-compat
  }
  return -1;
}

void DstyleReader::style_line(std::string_view text) {
  const StyleFields f = scan_style_line(text, table_.version);
  const auto fill = f.count >= 7 ? parse_fill(f.fill) : std::nullopt;
  const auto color = f.count >= 7 ? resolve_color(f.color) : std::nullopt;
  if (f.count < 7 || !fill || !color) {
    fail(dc::kDstyleLine, "Style line contained format error: " + trim_newline(text));
    return;
  }
  DisplayStyle s;
  s.mask = static_cast<std::int32_t>(f.mask) & plane_mask_;
  s.color = *color & plane_mask_;
  s.outline = static_cast<std::int32_t>(f.outline);
  s.fill = *fill;
  s.stipple = static_cast<std::int32_t>(f.stipple);
  s.short_name = static_cast<char>(f.short_name & 0x7F);
  if (f.count == 8) {
    s.long_name = f.long_name;
  }
  if (cscan::is_int(f.ordinal)) {
    s.ordinal = static_cast<std::int32_t>(cscan::atoi(f.ordinal));
  } else {
    s.ordinal = compat_ ? 1 : next_ordinal_;  // Q-UI-5
  }
  next_ordinal_ = s.ordinal + 1;
  records_.push_back(std::move(s));
}

void DstyleReader::stipple_line(std::string_view text) {
  const int base = table_.version > 6 ? 16 : 8;
  cscan::Scanner scan(text);
  const auto number = scan.integer(10);
  Stipple rows{};
  bool ok = number.has_value() && *number >= 0 && *number < 4096;
  for (std::size_t i = 0; ok && i < rows.size(); ++i) {
    const auto v = scan.integer(base);
    ok = v.has_value();
    rows[i] = static_cast<std::uint8_t>(v.value_or(0) & 0xFF);
  }
  if (!ok) {
    fail(dc::kDstyleLine, "Style line contained format error: " + trim_newline(text));
    return;
  }
  const auto n = static_cast<std::size_t>(*number);
  if (table_.stipples.size() <= n) {
    table_.stipples.resize(n + 1, Stipple{});
  }
  table_.stipples[n] = rows;
}

diag::Outcome<StyleTable> DstyleReader::finish() {
  constexpr int kRequired = kDisplay | kLayout | kPale;
  if ((processed_ & kRequired) != kRequired) {
    std::string text = "Not all required style sections were read.  Missing sections are:";
    for (const auto& [bit, name] : {std::pair{kDisplay, " display_styles"}, std::pair{kLayout, " layout_styles"},
                                    std::pair{kPale, " pale_styles"}}) {
      if ((processed_ & bit) == 0) {
        text += name;
      }
    }
    fail(dc::kDstyleMissing, text);
  } else if (max_tech_styles_ - kFixedStyleCount != scount_) {
    fail(dc::kDstylePale, "Error:  Number of pale styles (" + std::to_string(scount_) +
                              ") is different from the number of layout styles (" +
                              std::to_string(max_tech_styles_ - kFixedStyleCount) + ")");
  }
  if (error_) {
    return *error_;
  }
  // Magic fills the table backwards from its last slot, so a wrong number of
  // display styles shifts (or drops) the earliest entries.
  const std::size_t total = max_tech_styles_ + scount_;
  table_.layout_count = scount_;
  table_.styles.assign(total, DisplayStyle{});
  for (auto& s : table_.styles) {
    s.defined = false;
  }
  std::size_t from = records_.size();
  for (std::size_t i = total; i-- > 0 && from > 0;) {
    table_.styles[i] = records_[--from];
    table_.by_short_name[static_cast<unsigned char>(table_.styles[i].short_name) & 0x7FU] = i;
  }
  return std::move(table_);
}

}  // namespace

Stipple StyleTable::stipple(std::int32_t number) const noexcept {
  if (number < 0 || static_cast<std::size_t>(number) >= stipples.size()) {
    return Stipple{};
  }
  return stipples[static_cast<std::size_t>(number)];
}

std::optional<std::size_t> style_from_name(const StyleTable& table, std::string_view name) {
  for (std::size_t i = 0; i < table.styles.size(); ++i) {
    const auto& long_name = table.styles[i].long_name;
    if (long_name && *long_name == name) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> StyleTable::find_layout_style(std::string_view ref) const {
  const bool numeric = cscan::is_int(ref);
  const auto wanted = numeric ? cscan::atoi(ref) : 0;
  const std::size_t end = std::min(styles.size(), kFixedStyleCount + layout_count);
  for (std::size_t i = kFixedStyleCount; i < end; ++i) {
    const DisplayStyle& s = styles[i];
    const bool match = numeric ? s.defined && s.ordinal == wanted : s.long_name && *s.long_name == ref;
    if (match) {
      return i;
    }
  }
  return std::nullopt;
}

diag::Outcome<StyleTable> parse_dstyle(std::string_view text, std::string_view file_name, const ColorMap& colors,
                                       bool magic_compat, diag::DiagnosticSink& sink, std::int32_t plane_mask,
                                       int default_version) {
  DstyleReader reader(file_name, colors, magic_compat, sink, plane_mask, default_version);
  for (const auto line : cscan::fgets_chunks(text, kDstyleLine)) {
    reader.line(line);
  }
  return reader.finish();
}

// --- style files -------------------------------------------------------------

namespace {

std::optional<platform::Path> search(platform::FileSystem& fs, const std::string& name,
                                     const std::vector<platform::Path>& dirs) {
  for (const auto& dir : dirs) {
    const platform::Path candidate = dir / name;
    if (fs.exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

}  // namespace

diag::Outcome<StyleFiles> find_style_files(platform::FileSystem& fs, std::string_view family,
                                           DisplayClass display_class, const std::vector<platform::Path>& search_dirs,
                                           std::string_view monitor) {
  StyleFiles files;
  const std::string klass(display_class_name(display_class));
  const std::string base = std::string(family) + "." + klass;
  if (display_class != DisplayClass::bw) {
    const std::string cmap = base + "." + std::string(monitor);
    auto found = search(fs, cmap + ".cmap", search_dirs);
    if (!found) {
      found = search(fs, cmap + ".cmap1", search_dirs);
    }
    if (!found) {
      return diag::make(dc::kCmapOpen, "Couldn't open color map file \"" + cmap + ".cmap\"");
    }
    files.cmap = *found;
  }
  if (auto found = search(fs, base + ".dstyle", search_dirs)) {
    files.dstyle = *found;
  } else if (auto v5 = search(fs, base + ".dstyle5", search_dirs)) {
    files.dstyle = *v5;
    files.dstyle_is_v5 = true;
  } else {
    return diag::make(dc::kDstyleOpen, "Couldn't open display styles file \"" + base + ".dstyle5\"");
  }
  const std::string glyphs = display_class == DisplayClass::bw ? "bw" : "color";
  const auto found = search(fs, glyphs + ".glyphs", search_dirs);
  if (!found) {
    return diag::make(dc::kGlyphOpen, "Couldn't read glyphs file \"" + glyphs + ".glyphs\"");
  }
  files.glyphs = *found;
  return files;
}

// --- glyphs ------------------------------------------------------------------

namespace {

class GlyphReader {
 public:
  GlyphReader(std::string_view file_name, const StyleTable& table) : file_name_(file_name), table_(table) {}

  /// Returns false when reading stops (error, or extra lines).
  bool line(std::string_view text);
  diag::Outcome<std::vector<Glyph>> finish();
  bool extra() const noexcept { return extra_; }

 private:
  bool size_line(std::string_view text);
  bool row(std::string_view text);
  Glyph& current_glyph();
  bool fail(std::string_view code, const std::string& what) {
    error_ = diag::make(code, "Error in glyph file '" + file_name_ + "', " + what);
    return false;
  }

  std::string file_name_;
  const StyleTable& table_;
  std::vector<Glyph> glyphs_;
  bool sized_ = false;
  int y_ = -1;
  int glyph_ = -1;
  int last_glyph_ = -1;
  int xmax_ = -1;
  int ymax_ = -1;
  bool extra_ = false;
  std::optional<diag::Diagnostic> error_;
};

bool GlyphReader::line(std::string_view text) {
  if (cscan::is_white(text, true)) {
    return true;
  }
  return sized_ ? row(text) : size_line(text);
}

bool GlyphReader::size_line(std::string_view text) {
  cscan::Scanner scan(text);
  std::array<std::int64_t, 3> v{};
  bool ok = scan.literal("size");
  for (auto& value : v) {
    const auto parsed = ok ? scan.integer(10) : std::nullopt;
    ok = parsed && *parsed > 0 && *parsed <= kMaxGlyphDimension;
    value = parsed.value_or(0);
  }
  if (!ok) {
    error_ = diag::make(dc::kGlyphFormat, "Format error in  file '" + file_name_ + "'");
    return false;
  }
  if (v[0] * v[1] * v[2] > kMaxGlyphPixels) {
    return fail(dc::kGlyphTooLarge, "size " + std::to_string(v[0]) + " " + std::to_string(v[1]) + " " +
                                        std::to_string(v[2]) + " exceeds " + std::to_string(kMaxGlyphPixels) +
                                        " pixels.");
  }
  sized_ = true;
  last_glyph_ = static_cast<int>(v[0]) - 1;
  xmax_ = static_cast<int>(v[1]) - 1;
  ymax_ = static_cast<int>(v[2]) - 1;
  glyph_ = 0;
  y_ = ymax_ + 1;
  return true;
}

// Glyphs are allocated when their first row arrives, so memory follows the
// file's content rather than its size line.
Glyph& GlyphReader::current_glyph() {
  const auto index = static_cast<std::size_t>(glyph_);
  if (index >= glyphs_.size()) {
    Glyph blank;
    blank.width = xmax_ + 1;
    blank.height = ymax_ + 1;
    blank.pixels.assign(static_cast<std::size_t>(blank.width) * static_cast<std::size_t>(blank.height), 0);
    glyphs_.push_back(std::move(blank));
  }
  return glyphs_[index];
}

bool GlyphReader::row(std::string_view text) {
  if (y_ > 0) {
    --y_;  // rows are listed from the top down
  } else {
    y_ = ymax_;
    if (++glyph_ > last_glyph_) {
      extra_ = true;
      return false;
    }
  }
  Glyph& g = current_glyph();
  auto at = [text](std::size_t i) { return i < text.size() ? text[i] : '\0'; };
  std::size_t cp = 0;
  for (int x = 0; x <= xmax_; ++x) {
    const char c = at(cp);
    if (cscan::is_space(c)) {
      return fail(dc::kGlyphFormat, "white space is not a valid glyph character");
    }
    g.pixels[static_cast<std::size_t>(x + (xmax_ + 1) * y_)] = table_.by_short_name[static_cast<unsigned char>(c) & 0x7FU];
    const char trailing = at(++cp);
    if (trailing == '*') {
      g.hot_x = x;
      g.hot_y = y_;
    }
    if (x != xmax_ && (trailing == '\0' || at(++cp) == '\0')) {
      return fail(dc::kGlyphFormat, "line is too short.");
    }
  }
  return true;
}

diag::Outcome<std::vector<Glyph>> GlyphReader::finish() {
  if (error_) {
    return *error_;
  }
  if (!extra_ && !(y_ == 0 && glyph_ == last_glyph_)) {
    return diag::make(dc::kGlyphEof, "Unexpected end of file in file '" + file_name_ + "'");
  }
  return std::move(glyphs_);
}

}  // namespace

diag::Outcome<std::vector<Glyph>> parse_glyphs(std::string_view text, std::string_view file_name,
                                               const StyleTable& table, diag::DiagnosticSink& sink) {
  GlyphReader reader(file_name, table);
  for (const auto line : cscan::fgets_chunks(text, kGlyphLine)) {
    if (!reader.line(line)) {
      break;
    }
  }
  if (reader.extra()) {
    sink.emit(diag::make(dc::kGlyphExtra, "Extra lines at end of glyph file '" + std::string(file_name) + "'"));
  }
  return reader.finish();
}

// --- type to style mapping -----------------------------------------------------

const std::vector<std::size_t>& TypeStyleMap::of(tech::TileType type) const {
  static const std::vector<std::size_t> kNone;
  return type < styles_of_type.size() ? styles_of_type[type] : kNone;
}

tech::TypeMask TypeStyleMap::types_of(std::size_t style) const {
  tech::TypeMask mask;
  for (std::size_t t = 0; t < styles_of_type.size(); ++t) {
    if (std::binary_search(styles_of_type[t].begin(), styles_of_type[t].end(), style)) {
      mask.set(static_cast<tech::TileType>(t));
    }
  }
  return mask;
}

namespace {

void bind(TypeStyleMap& map, tech::TileType type, std::size_t style) {
  if (type >= map.styles_of_type.size()) {
    return;
  }
  auto& list = map.styles_of_type[type];
  const auto it = std::lower_bound(list.begin(), list.end(), style);
  if (it == list.end() || *it != style) {
    list.insert(it, style);
  }
}

/// "Contact style propagation": stacked types whose component is `contact`
/// and whose home plane is the contact's.
void propagate_to_stacked(TypeStyleMap& map, const tech::Technology& tech, tech::TileType contact, std::size_t style) {
  const auto home = tech.home_plane(contact);
  for (const auto& info : tech.contacts()) {
    const bool component = info.component_a == contact || info.component_b == contact;
    if (info.stacked && component && tech.home_plane(info.type) == home) {
      bind(map, info.type, style);
    }
  }
}

}  // namespace

namespace {

/// Binds `line.type` to `style`, with the stacked types the tech reader
/// recorded for the line and the propagation over the current contacts.
void bind_reference(TypeStyleMap& map, const tech::Technology& tech, const tech::StyleRefLine& line, std::size_t style) {
  bind(map, line.type, style);
  for (std::size_t t = 0; t < map.styles_of_type.size(); ++t) {
    if (line.stacked.test(static_cast<tech::TileType>(t))) {
      bind(map, static_cast<tech::TileType>(t), style);
    }
  }
  const auto* contact = tech.contact(line.type);
  if (contact != nullptr && !contact->stacked) {
    propagate_to_stacked(map, tech, line.type, style);
  }
}

}  // namespace

TypeStyleMap resolve_type_styles(const tech::Technology& tech, const tech::StylesSection& section,
                                 const StyleTable& table, diag::DiagnosticSink& sink) {
  TypeStyleMap map;
  map.styles_of_type.resize(std::max<std::size_t>(tech.type_count(), tech::kFirstUserType));
  for (const auto& line : section.lines) {
    for (const auto& ref : line.refs) {
      const auto style = table.find_layout_style(ref);
      if (style) {
        bind_reference(map, tech, line, *style);
      } else {
        sink.emit(diag::make(dc::kStyleInvalid, "Invalid style " + quote(ref) + " for tile type " +
                                                    std::string(tech.type_name(line.type))));
      }
    }
  }
  return map;
}

TypeStyleMap resolve_type_styles(const tech::Technology& tech, const StyleTable& table, diag::DiagnosticSink& sink) {
  return resolve_type_styles(tech, tech.styles(), table, sink);
}

// --- loading -------------------------------------------------------------------

namespace {

diag::Outcome<std::string> read_file(platform::FileSystem& fs, const platform::Path& path) {
  return fs.read(path);
}

}  // namespace

diag::Outcome<StyleSet> load_style_set(platform::FileSystem& fs, const tech::Technology& tech,
                                       const StyleLoadOptions& options, diag::DiagnosticSink& sink) {
  const auto& section = tech.styles();
  if (section.style_family.empty()) {
    return diag::make(dc::kStyleNoFamily, "Technology \"" + tech.name() + "\" has no styletype");
  }
  std::vector<platform::Path> dirs{platform::Path{"."}};
  for (const auto& d : section.search_dirs) {
    dirs.emplace_back(d);
  }
  dirs.insert(dirs.end(), options.library_dirs.begin(), options.library_dirs.end());
  const auto files = find_style_files(fs, section.style_family, options.display_class, dirs, options.monitor);
  if (!files) {
    return files.error();
  }
  StyleSet set;
  set.display_class = options.display_class;
  if (files.value().cmap) {
    const auto text = read_file(fs, *files.value().cmap);
    if (!text) {
      return diag::make(dc::kCmapOpen, text.error().text);
    }
    auto map = parse_color_map(text.value(), files.value().cmap->filename().string(), options.magic_compat);
    if (!map) {
      return map.error();
    }
    set.colors = std::move(map).value();
  }
  const auto dtext = read_file(fs, files.value().dstyle);
  if (!dtext) {
    return diag::make(dc::kDstyleOpen, dtext.error().text);
  }
  auto table = parse_dstyle(dtext.value(), files.value().dstyle.filename().string(), set.colors, options.magic_compat,
                            sink, display_plane_mask(options.display_class), 5);
  if (!table) {
    return table.error();
  }
  set.table = std::move(table).value();
  const auto gtext = read_file(fs, files.value().glyphs);
  if (!gtext) {
    return diag::make(dc::kGlyphOpen, gtext.error().text);
  }
  auto glyphs = parse_glyphs(gtext.value(), files.value().glyphs.string(), set.table, sink);
  if (!glyphs) {
    return glyphs.error();
  }
  set.cursors = std::move(glyphs).value();
  set.types = resolve_type_styles(tech, set.table, sink);
  return set;
}

}  // namespace hefesto::display
