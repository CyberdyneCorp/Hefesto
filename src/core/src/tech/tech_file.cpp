// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The technology-file reader (capability tech-file-format): physical lines,
// comments, continuation, tokenization, limits, `include`, `repeat` and the
// section structure.
//
// Ported from Magic VLSI 8.3.694 utils/tech.c (techGetTokens, TechLoad,
// techFindSection) and utils/path.c (PaOpen search order), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).

#include "hefesto/tech/tech_file.hpp"

#include <cctype>
#include <utility>

#include "tech/reader.hpp"
#include "tech/technology_data.hpp"

namespace hefesto::tech {

namespace {

constexpr std::size_t kMaxLogicalLine = 1023;  // MAXLINESIZE - 1

constexpr SectionMask kTypes = section_bit(kSecTypes);
constexpr SectionMask kPlanes = section_bit(kSecPlanes);
constexpr SectionMask kContact = section_bit(kSecContact);
constexpr SectionMask kConnect = section_bit(kSecConnect);

constexpr std::array<SectionDef, kSecCount> kSections = {{
    {"tech", "", 0, false},
    {"version", "", 0, true},
    {"planes", "", 0, false},
    {"types", "", kPlanes, false},
    {"styles", "", kTypes, false},
    {"contact", "images", kTypes | kPlanes, false},
    {"aliases", "", kPlanes | kTypes | kContact, true},
    {"compose", "", kTypes | kPlanes | kContact, false},
    {"connect", "", kTypes | kPlanes | kContact, false},
    {"cifoutput", "", 0, false},
    {"cifinput", "", 0, false},
    {"mzrouter", "", kTypes | kPlanes, true},
    {"drc", "", kTypes | kPlanes, false},
    {"lef", "", kTypes | kPlanes, true},
    {"extract", "", kTypes | kConnect, false},
    {"wiring", "", kTypes, true},
    {"router", "", kTypes, true},
    {"plowing", "", kTypes | kConnect | kContact, true},
    {"plot", "", kTypes, true},
}};

bool is_c_space(char c) noexcept {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool starts_with(std::string_view text, std::string_view prefix) noexcept {
  return text.substr(0, prefix.size()) == prefix;
}

bool is_comment_line(std::string_view line) noexcept {
  std::size_t i = 0;
  while (i < line.size() && is_c_space(line[i])) {
    ++i;
  }
  return i < line.size() && line[i] == '#';
}

std::vector<std::string> split_physical_lines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    const auto lf = text.find('\n', start);
    const auto end = lf == std::string_view::npos ? text.size() : lf;
    lines.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

// Reads one token starting at `i` (not at whitespace); advances past it and
// its terminating character, as techGetTokens does.
std::string read_token(std::string_view line, std::size_t& i) {
  std::string token;
  const bool in_quote = line[i] == '"';
  if (in_quote) {
    ++i;
  }
  while (i < line.size()) {
    const char c = line[i];
    if (in_quote ? c == '"' : is_c_space(c)) {
      break;
    }
    if (c == '\\' && ++i == line.size()) {
      break;
    }
    token += line[i++];
  }
  if (i < line.size()) {
    ++i;  // the closing quote or the blank that ended the token
  }
  return token;
}

std::optional<platform::Path> existing(platform::FileSystem& fs, const platform::Path& path) {
  return fs.exists(path) ? std::optional<platform::Path>{path} : std::nullopt;
}

// PaOpen's handling of names that bypass the search path; `searched` tells
// whether the caller should go on with the directory list.
std::optional<platform::Path> open_direct(platform::FileSystem& fs, const std::string& full,
                                          const SearchOptions& options, bool& searched) {
  searched = false;
  if (starts_with(full, "~") || starts_with(full, "$")) {
    const auto expanded = options.expand ? options.expand(full) : std::nullopt;
    return existing(fs, expanded ? platform::Path{*expanded} : platform::Path{full});
  }
  if (starts_with(full, "/")) {
    return existing(fs, full);
  }
  searched = true;
  if (full == "." || full == ".." || starts_with(full, "./") || starts_with(full, "../")) {
    return existing(fs, full);
  }
  return std::nullopt;
}

std::optional<platform::Path> find_in_dirs(platform::FileSystem& fs, std::string_view name, std::string_view suffix,
                                           const SearchOptions& options, const std::vector<platform::Path>& dirs) {
  const std::string full = std::string{name} + std::string{suffix};
  bool search = false;
  if (auto direct = open_direct(fs, full, options, search); direct || !search) {
    return direct;
  }
  for (const auto& dir : dirs) {
    if (auto found = existing(fs, dir / full)) {
      return found;
    }
  }
  return std::nullopt;
}

// Collects sections into a TechDocument (read_tech_text).
class DocumentClient final : public SectionClient {
 public:
  explicit DocumentClient(TechDocument& doc) : doc_(doc) {}

  bool begin(int section, const TechLine& header) override {
    doc_.sections.push_back(TechSection{std::string{section_table()[static_cast<std::size_t>(section)].name},
                                        {}, header.where, false});
    return true;
  }
  void line(int /*section*/, const TechLine& line) override { doc_.sections.back().lines.push_back(line); }
  void end(int /*section*/, const TechLine& /*end_line*/) override { doc_.sections.back().closed = true; }

 private:
  TechDocument& doc_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace

const std::array<SectionDef, kSecCount>& section_table() noexcept {
  return kSections;
}

int find_section(std::string_view name) noexcept {
  for (std::size_t i = 0; i < kSections.size(); ++i) {
    if (kSections[i].name == name || (!kSections[i].alias.empty() && kSections[i].alias == name)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

std::vector<std::string> first_logical_line(std::string_view text) {
  std::string logical;
  for (std::string phys : split_physical_lines(text)) {
    if (is_comment_line(phys)) {
      continue;
    }
    if (!phys.empty() && phys.back() == '\r') {
      phys.pop_back();
    }
    const bool continued = !phys.empty() && phys.back() == '\\';
    if (continued) {
      phys.pop_back();
    }
    logical += phys;
    if (continued && logical.size() <= kMaxLogicalLine) {
      continue;
    }
    auto tokens = tokenize_tech_line(logical.substr(0, kMaxLogicalLine));
    if (!tokens.empty()) {
      return tokens;
    }
    logical.clear();
  }
  return {};
}

std::vector<std::string> tokenize_tech_line(std::string_view line) {
  std::vector<std::string> tokens;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && is_c_space(line[i])) {
      ++i;
    }
    if (i == line.size()) {
      break;
    }
    tokens.push_back(read_token(line, i));
  }
  return tokens;
}

std::optional<platform::Path> find_file(platform::FileSystem& fs, std::string_view name, std::string_view suffix,
                                        const SearchOptions& options) {
  std::vector<platform::Path> dirs{platform::Path{"."}};
  dirs.insert(dirs.end(), options.search_path.begin(), options.search_path.end());
  return find_in_dirs(fs, name, suffix, options, dirs);
}

// --- LineReader ------------------------------------------------------------

LineReader::LineReader(platform::FileSystem& fs, diag::DiagnosticSink& sink, ReadOptions options)
    : fs_(fs), sink_(sink), options_(std::move(options)) {}

void LineReader::open(std::string_view text, std::string file_name, platform::Path dir) {
  push_text(text, std::move(file_name), std::move(dir));
}

void LineReader::push_text(std::string_view text, std::string name, platform::Path dir) {
  files_.push_back(File{std::move(name), std::move(dir), split_physical_lines(text), 0, next_file_id_++, 0, false});
}

void LineReader::warn(std::string_view code, const diag::SourceLocation& where, std::string text) {
  sink_.emit(diag::make(code, std::move(text), where));
}

// Line numbers follow Magic's techLineNumber (utils/tech.c:1009-1016): every
// physical read counts, including lines read again by a repeat block. When an
// included file ends, Magic restores the including file's count to the
// include line and the read that continues after it does not count again, so
// Magic reports every later line of that file one lower than its physical
// line. magic-compat reproduces that; otherwise the physical count is kept.
bool LineReader::read_physical(std::string& out, std::uint32_t& line_no) {
  while (!files_.empty() && files_.back().next == files_.back().lines.size()) {
    files_.pop_back();  // end of an included file: continue after its include line
    if (!files_.empty() && options_.magic_compat) {
      files_.back().skip_count = true;
    }
  }
  if (files_.empty()) {
    return false;
  }
  File& file = files_.back();
  if (file.skip_count) {
    file.skip_count = false;
  } else {
    ++file.line_no;
  }
  line_no = file.line_no;
  out = file.lines[file.next++];
  return true;
}

std::optional<TechLine> LineReader::raw_line() {
  std::string logical;
  std::string phys;
  std::uint32_t line_no = 0;
  while (true) {
    if (!read_physical(phys, line_no)) {
      return std::nullopt;  // a continuation pending at end of file is dropped, as in Magic
    }
    if (is_comment_line(phys)) {
      continue;
    }
    if (!phys.empty() && phys.back() == '\r') {
      phys.pop_back();
    }
    const bool continued = !phys.empty() && phys.back() == '\\';
    if (continued) {
      phys.pop_back();
    }
    logical += phys;
    if (logical.size() > kMaxLogicalLine) {
      logical.resize(kMaxLogicalLine);
      warn("TECH-READ-001", {files_.back().name, line_no}, "long line truncated");
      break;
    }
    if (!continued) {
      break;
    }
  }
  return TechLine{tokenize_tech_line(logical), diag::SourceLocation{files_.back().name, line_no}};
}

std::optional<TechLine> LineReader::next() {
  while (auto line = raw_line()) {
    if (line->tokens.empty() || directive(*line)) {
      continue;
    }
    return line;
  }
  return std::nullopt;
}

bool LineReader::directive(const TechLine& line) {
  const auto& t = line.tokens;
  if (t.size() == 1 && t[0] == "endrepeat") {
    end_repeat(line);
    return true;
  }
  if (t.size() == 2 && t[0] == "repeat") {
    start_repeat(line);
    return true;
  }
  if (t.size() > 1 && t[0] == "include") {
    include(line);
    return true;
  }
  return false;
}

void LineReader::start_repeat(const TechLine& line) {
  Repeat repeat{0, files_.back().id, files_.back().next};
  if (str_is_int(line.tokens[1])) {
    repeat.remaining = str_to_int(line.tokens[1]) - 1;
  } else {
    warn("TECH-REPEAT-001", line.where, "Error: \"repeat\" with invalid count " + line.tokens[1]);
  }
  if (options_.magic_compat) {
    repeats_.clear();  // one counter: an inner repeat replaces the outer state (Q-TECH-5)
  }
  repeats_.push_back(repeat);
}

void LineReader::end_repeat(const TechLine& line) {
  if (repeats_.empty()) {
    return;
  }
  Repeat& top = repeats_.back();
  if (top.remaining <= 0) {
    if (!options_.magic_compat) {
      repeats_.pop_back();
    }
    return;
  }
  if (top.file_id != files_.back().id) {
    warn("TECH-REPEAT-002", line.where, "\"repeat\" block crosses an include file boundary");
    failed_ = true;
    repeats_.clear();
    return;
  }
  --top.remaining;
  files_.back().next = top.position;
}

void LineReader::include(const TechLine& line) {
  const std::string& name = line.tokens[1];
  const std::vector<platform::Path> own_dir{files_.back().dir};
  auto found = find_file(fs_, name, options_.include_suffix, options_);
  if (!found && !own_dir.front().empty()) {
    found = find_in_dirs(fs_, name, options_.include_suffix, options_, own_dir);
  }
  if (!found && !options_.magic_compat) {
    found = find_file(fs_, name, "", options_);
    if (!found && !own_dir.front().empty()) {
      found = find_in_dirs(fs_, name, "", options_, own_dir);
    }
    if (found) {
      warn("TECH-INCLUDE-002", line.where,
           "Include file " + name + options_.include_suffix + " not found; using " + found->string());
    }
  }
  const auto text = found ? fs_.read(*found) : diag::Outcome<std::string>{diag::make("TECH-INCLUDE-001", "")};
  if (!text) {
    warn("TECH-INCLUDE-001", line.where, "Warning: Couldn't find include file " + name);
    failed_ = true;  // Q-TECH-2: the include line is discarded and the load fails
    return;
  }
  push_text(text.value(), found->string(), found->parent_path());
}

// --- section scanner -------------------------------------------------------

namespace {

// A line read with no section open: returns the opened section, or -1 with
// `skip` set when the reader must skip to the next `end`.
int open_section(const TechLine& line, SectionClient& client, diag::DiagnosticSink& sink, bool magic_compat,
                 bool& skip) {
  static const std::string kSkipping = "\n[Skipping to \"end\"]";
  const auto& t = line.tokens;
  if (t.size() != 1) {
    sink.emit(diag::make("TECH-SECTION-001", "Bad section header line" + kSkipping, line.where));
    skip = true;
    return -1;
  }
  const int section = find_section(t[0]);
  if (section < 0 && t[0] == "end" && !magic_compat) {
    sink.emit(diag::make("TECH-SECTION-002", "Stray \"end\" outside any section ignored", line.where));
    return -1;
  }
  if (section < 0) {
    sink.emit(diag::make("TECH-SECTION-001", "Unrecognized section name: " + t[0] + kSkipping, line.where));
    skip = true;
    return -1;
  }
  skip = !client.begin(section, line);
  return skip ? -1 : section;
}

}  // namespace

void scan_sections(LineReader& reader, SectionClient& client, diag::DiagnosticSink& sink, bool magic_compat) {
  int current = -1;
  bool skip = false;
  while (auto line = reader.next()) {
    const auto& t = line->tokens;
    if (!skip && current < 0) {
      current = open_section(*line, client, sink, magic_compat, skip);
      continue;
    }
    if (t.size() == 1 && t[0] == "end") {
      if (!skip) {
        client.end(current, *line);
      }
      current = -1;
      skip = false;
    } else if (!skip) {
      client.line(current, *line);
    }
  }
}

// --- public reader ---------------------------------------------------------

diag::Outcome<TechDocument> read_tech_text(std::string_view text, std::string_view file_name,
                                           platform::FileSystem& fs, const platform::Path& base_dir,
                                           diag::DiagnosticSink& sink, const ReadOptions& options) {
  TechDocument doc;
  LineReader reader(fs, sink, options);
  reader.open(text, std::string{file_name}, base_dir);
  DocumentClient client(doc);
  scan_sections(reader, client, sink, options.magic_compat);
  doc.failed = reader.failed();
  return doc;
}

diag::Outcome<TechDocument> read_tech_file(platform::FileSystem& fs, const platform::Path& path,
                                           diag::DiagnosticSink& sink, const ReadOptions& options) {
  auto text = fs.read(path);
  if (!text) {
    return text.error();
  }
  return read_tech_text(text.value(), path.string(), fs, path.parent_path(), sink, options);
}

}  // namespace hefesto::tech
