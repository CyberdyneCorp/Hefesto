// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Technology loading (capability tech-loading-and-scaling, and the loader
// parts of tech-file-format): name resolution, the readability pre-check,
// the section registry with prerequisites, error accounting, missing
// sections, atomic full loads, partial reloads, post-load adjustments, the
// built-in `minimum` technology and the grid-limit check.
//
// Ported from Magic VLSI 8.3.694 utils/tech.c (TechLoad), cif/CIFtech.c
// (CIFParseScale, CIFTechLimitScale and the scalefactor/gridlimit/options
// lines of the first output style) and scmos/minimum.tech.in, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).

#include "hefesto/tech/loader.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

#include "tech/build_state.hpp"
#include "tech/reader.hpp"

namespace hefesto::tech {

namespace {

using Data = Technology::Data;

constexpr std::string_view kMinimumTech =
    "tech\n"
    "    format 28\n"
    "    minimum\n"
    "end\n"
    "\n"
    "version\n"
    "    version 0.0\n"
    "    description \"Minimum technology file structure\"\n"
    "end\n"
    "\n"
    "planes\n"
    "end\n"
    "\n"
    "types\n"
    "end\n"
    "\n"
    "contact\n"
    "end\n"
    "\n"
    "styles\n"
    "    styletype\tmos\n"
    "end\n"
    "\n"
    "compose\n"
    "end\n"
    "\n"
    "connect\n"
    "end\n"
    "\n"
    "cifoutput\n"
    "end\n"
    "\n"
    "cifinput\n"
    "end\n"
    "\n"
    "drc\n"
    "end\n"
    "\n"
    "extract\n"
    "end\n";

std::string section_name(int section) {
  return std::string{section_table()[static_cast<std::size_t>(section)].name};
}

bool has_section(SectionMask mask, int section) noexcept {
  return (mask & section_bit(section)) != 0;
}

bool is_raw_section(int section) noexcept {
  return section >= kSecCifOutput;
}

// The sections whose repetition is an error with magic-compat OFF (Q-TECH-7).
bool repeat_is_error(int section) noexcept {
  return section == kSecTypes || section == kSecPlanes || section == kSecContact || section == kSecCompose ||
         section == kSecConnect;
}

// --- the first cifoutput style's grid values ---------------------------

// CIFParseScale: "12.5" gives 25 with expander 2.
std::int64_t parse_cif_scale(std::string_view text, std::int64_t& expander) {
  const auto dot = text.find('.');
  if (dot == std::string_view::npos) {
    expander = 1;
    return str_to_int(text);
  }
  std::int64_t d = 1;
  for (std::size_t i = dot + 1; i < text.size(); ++i) {
    d *= 10;
  }
  const std::int64_t n = str_to_int(text.substr(0, dot)) * d + str_to_int(text.substr(dot + 1));
  const GridScale reduced = reduce(GridScale{n, d});
  expander = reduced.lambda;
  return reduced.internal;
}

class CifGridReader {
 public:
  void init() {
    styles_ = 0;
    grid_.reset();
  }

  void line(Tokens tokens) {
    if (tokens[0] == "style") {
      if (styles_++ == 0) {
        grid_ = CifGridInfo{};
      }
      return;
    }
    if (styles_ != 1 || !grid_) {
      return;  // only the first style is the current one after a load
    }
    if (tokens[0] == "scalefactor" && tokens.size() >= 2) {
      scale_factor(tokens);
    } else if (tokens[0] == "gridlimit" && tokens.size() == 2) {
      grid_->grid_limit = std::max<std::int64_t>(str_to_int(tokens[1]), 0);
    } else if (tokens[0] == "options") {
      for (const auto& option : tokens.subspan(1)) {
        grid_->minimum_grid = grid_->minimum_grid || option == "set-minimum-grid";
      }
    }
  }

  [[nodiscard]] const std::optional<CifGridInfo>& grid() const noexcept { return grid_; }

 private:
  // `scalefactor <n> [nanometers|angstroms]` of the current style.
  void scale_factor(Tokens tokens) {
    grid_->scale_factor = parse_cif_scale(tokens[1], grid_->expander);
    const std::string& last = tokens.back();
    if (tokens.size() >= 3 && last.compare(0, 5, "nanom") == 0) {
      grid_->expander *= 10;
    } else if (tokens.size() >= 3 && last.compare(0, 6, "angstr") == 0) {
      grid_->expander *= 100;
    }
  }

  int styles_ = 0;
  std::optional<CifGridInfo> grid_;
};

// --- the section client of TechLoad -----------------------------------

class LoadClient final : public SectionClient {
 public:
  LoadClient(BuildState& s, diag::DiagnosticSink& sink, SectionMask excluded)
      : s_(s), sink_(sink), excluded_(excluded) {}

  bool begin(int section, const TechLine& header) override {
    if (has_section(excluded_, section)) {
      return false;  // partial reload: skipped silently, counts as read
    }
    s_.r().set_location(header.where, "(none)");
    if (!prerequisites_met(section, header)) {
      return false;
    }
    if (has_section(read_, section)) {
      repeated(section, header);
    }
    s_.r().set_location(header.where, section_name(section));
    init(section);
    return true;
  }

  void line(int section, const TechLine& line) override {
    s_.r().set_location(line.where, section_name(section));
    const Tokens tokens{line.tokens};
    const bool accepted = is_raw_section(section) ? raw_line(section, line) : section_line(s_, section, tokens);
    if (!accepted) {
      bad_ |= section_bit(section);
    }
  }

  void end(int section, const TechLine& end_line) override {
    s_.r().set_location(end_line.where, section_name(section));
    read_ |= section_bit(section);
    section_final(s_, section);
    if (section == kSecCifOutput) {
      s_.d.cif_grid = cif_.grid();
    } else if (section == kSecExtract && s_.magic_compat) {
      s_.d.fringe_halo = s_.d.format_version >= 34;  // Q-TECH-32
    }
    s_.r().set_location(end_line.where, "(none)");  // techCurrentSection = NULL after `end`
  }

  [[nodiscard]] SectionMask read() const noexcept { return read_; }
  [[nodiscard]] SectionMask bad() const noexcept { return bad_; }
  [[nodiscard]] bool repeat_failed() const noexcept { return repeat_failed_; }

 private:
  bool prerequisites_met(int section, const TechLine& header) {
    const SectionMask missing =
        section_table()[static_cast<std::size_t>(section)].prerequisites & ~(read_ | excluded_);
    if (missing == 0) {
      return true;
    }
    std::string text = "Section " + section_name(section) + " appears too early.\n\tMissing prerequisite sections:";
    for (int other = 0; other < kSecCount; ++other) {
      if (has_section(missing, other)) {
        text += "\n\t\t" + section_name(other);
      }
    }
    text += "\n[Skipping to \"end\"]";
    sink_.emit(diag::make("TECH-SECTION-003", std::move(text), header.where));
    return false;
  }

  void repeated(int section, const TechLine& header) {
    if (!s_.magic_compat && repeat_is_error(section)) {
      sink_.emit(diag::make("TECH-SECTION-006", "Section " + section_name(section) + " appears more than once",
                            header.where));
      repeat_failed_ = true;
      return;
    }
    sink_.emit(diag::make("TECH-SECTION-005",
                          "Section " + section_name(section) + " appears more than once; it is read again (Q-TECH-7)",
                          header.where));
  }

  void init(int section) {
    section_init(s_, section);
    if (!is_raw_section(section)) {
      return;
    }
    s_.d.raw_sections.push_back(RawSection{section_name(section), {}});
    if (section == kSecCifOutput) {
      cif_.init();
    }
  }

  bool raw_line(int section, const TechLine& line) {
    if (section == kSecCifOutput) {
      cif_.line(Tokens{line.tokens});
    }
    s_.d.raw_sections.back().lines.push_back(RawLine{line.tokens, line.where.file, line.where.line});
    return true;
  }

  BuildState& s_;
  diag::DiagnosticSink& sink_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  SectionMask excluded_;
  SectionMask read_ = 0;
  SectionMask bad_ = 0;
  bool repeat_failed_ = false;
  CifGridReader cif_;
};

struct RunResult {
  bool ok = false;
  Data data;
};

// Reports bad and missing sections; returns whether the load succeeded.
bool account(const LoadClient& client, const std::string& file, SectionMask excluded, SectionMask required_extra,
             diag::DiagnosticSink& sink) {
  bool ok = client.bad() == 0 && !client.repeat_failed();
  if (client.bad() != 0) {
    std::string text = "The following sections of " + file + " contained errors:";
    for (int s = 0; s < kSecCount; ++s) {
      if (has_section(client.bad(), s)) {
        text += "\n    " + section_name(s);
      }
    }
    sink.emit(diag::make("TECH-SECTION-007", std::move(text), diag::SourceLocation{file, 0}));
  }
  for (int s = 0; s < kSecCount; ++s) {
    const bool required = !section_table()[static_cast<std::size_t>(s)].optional || has_section(required_extra, s);
    if (has_section(excluded, s) || has_section(client.read(), s) || !required) {
      continue;
    }
    sink.emit(diag::make("TECH-SECTION-004", "Section \"" + section_name(s) + "\" was missing from " + file + ".",
                         diag::SourceLocation{file, 0}));
    ok = false;
  }
  return ok;
}

struct RunInput {
  std::string_view text;
  const platform::Path& file;
  platform::FileSystem& fs;
  const LoadOptions& options;
  diag::DiagnosticSink& sink;
  SectionMask excluded = 0;
  SectionMask required_extra = 0;  // selected sections of a partial reload
};

// Adds the current section (Magic's techCurrentSection, "(none)" outside a
// section) to every diagnostic located on a line of the file, so a Magic-style
// rendering can print TechPrintLine's context (diag::format_magic).
class SectionStampSink final : public diag::DiagnosticSink {
 public:
  explicit SectionStampSink(diag::DiagnosticSink& inner) : inner_(inner) {}
  void attach(const Reporter& rep) noexcept { rep_ = &rep; }

  void emit(const diag::Diagnostic& diagnostic) override {
    const auto* where = std::get_if<diag::SourceLocation>(&diagnostic.location);
    if (where == nullptr || where->line == 0 || diagnostic.related.has_value() || rep_ == nullptr) {
      inner_.emit(diagnostic);
      return;
    }
    diag::Diagnostic stamped = diagnostic;
    const std::string& section = rep_->section();
    stamped.related = diag::ObjectRef{diag::ObjectKind::section, section.empty() ? "(none)" : section};
    inner_.emit(stamped);
  }

 private:
  diag::DiagnosticSink& inner_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  const Reporter* rep_ = nullptr;
};

RunResult run_load(const RunInput& in, Data start) {
  SectionStampSink sink(in.sink);
  Reporter rep(sink);
  sink.attach(rep);
  BuildState s;
  s.d = std::move(start);
  s.rep = &rep;
  s.magic_compat = in.options.magic_compat;
  s.d.magic_compat = in.options.magic_compat;

  ReadOptions read_options;
  read_options.magic_compat = in.options.magic_compat;
  read_options.search_path = in.options.search_path;
  read_options.include_suffix = in.options.include_suffix;
  LineReader reader(in.fs, sink, read_options);
  reader.open(in.text, in.file.string(), in.file.parent_path());

  LoadClient client(s, sink, in.excluded);
  scan_sections(reader, client, sink, in.options.magic_compat);
  bool ok = account(client, in.file.string(), in.excluded, in.required_extra, sink) && !reader.failed();
  s.d.end_line = reader.end_line();
  return RunResult{ok, std::move(s.d)};
}

std::shared_ptr<const Technology> publish(Data d) {
  publish_contacts(d);
  return std::make_shared<const Technology>(std::make_shared<const Data>(std::move(d)));
}

diag::Diagnostic load_failure(const platform::Path& file) {
  return diag::make("TECH-LOAD-001", "Error in loading technology file", diag::SourceLocation{file.string(), 0});
}

bool has_suffix(std::string_view text, std::string_view suffix) noexcept {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::string join_paths(const std::vector<platform::Path>& paths) {
  std::string out = ".";
  for (const auto& p : paths) {
    out += " " + p.string();
  }
  return out;
}

}  // namespace

diag::Outcome<platform::Path> resolve_tech_file(platform::FileSystem& fs, std::string_view name,
                                                const LoadOptions& options, std::string* include_suffix) {
  SearchOptions search;
  search.magic_compat = options.magic_compat;
  search.search_path = options.search_path;
  std::string base{name};
  const auto slash = base.rfind('/');
  const std::string_view leaf = std::string_view{base}.substr(slash == std::string::npos ? 0 : slash + 1);
  const auto dot = leaf.rfind('.');
  if (dot != std::string_view::npos && leaf.substr(dot) == ".tech") {
    base.resize(base.size() - 5);
  } else if (dot != std::string_view::npos) {
    if (auto verbatim = find_file(fs, base, "", search)) {
      if (include_suffix != nullptr) {
        *include_suffix = ".tech";
      }
      return *verbatim;
    }
  }
  for (const std::string_view suffix : {std::string_view{".tech"}, std::string_view{".tech27"}}) {
    if (auto found = find_file(fs, base, suffix, search)) {
      if (include_suffix != nullptr) {
        *include_suffix = std::string{suffix};
      }
      return *found;
    }
  }
  return diag::make("TECH-FILE-001", "Could not find file '" + base + ".tech' in any of these directories:\n         " +
                                         join_paths(options.search_path));
}

diag::Status precheck_tech_file(platform::FileSystem& fs, const platform::Path& path) {
  const auto text = fs.read(path);
  if (!text || first_logical_line(text.value()) != std::vector<std::string>{"tech"}) {
    return diag::make("TECH-FILE-002", "Technology file does not exist or is not readable.",
                      diag::SourceLocation{path.string(), 0});
  }
  return diag::Done{};
}

diag::Outcome<std::shared_ptr<const Technology>> load_technology_text(std::string_view text,
                                                                      const platform::Path& file_name,
                                                                      platform::FileSystem& fs,
                                                                      const LoadOptions& options,
                                                                      diag::DiagnosticSink& sink) {
  Data start = Data::builtin();
  start.name.clear();  // Q-TECH-8: name, version and description start empty
  RunResult result = run_load(RunInput{text, file_name, fs, options, sink}, std::move(start));
  if (result.ok && result.data.name.empty()) {
    sink.emit(diag::make("TECH-NAME-001", "The tech section names no technology",
                         diag::SourceLocation{file_name.string(), 0}));
    result.ok = false;
  }
  if (!result.ok) {
    return load_failure(file_name);
  }
  if (!options.magic_compat) {
    result.data.fringe_halo = result.data.format_version >= 34;  // Q-TECH-32, magic-compat OFF
  }
  result.data.file_name = file_name.string();
  return publish(std::move(result.data));
}

diag::Outcome<std::shared_ptr<const Technology>> load_technology(platform::FileSystem& fs, const platform::Path& path,
                                                                 const LoadOptions& options,
                                                                 diag::DiagnosticSink& sink) {
  const auto text = fs.read(path);
  if (!text) {
    sink.emit(diag::make("TECH-FILE-002", "Technology file does not exist or is not readable.",
                         diag::SourceLocation{path.string(), 0}));
    return load_failure(path);
  }
  LoadOptions effective = options;
  if (has_suffix(path.string(), ".tech27") && options.include_suffix == ".tech") {
    effective.include_suffix = ".tech27";
  }
  return load_technology_text(text.value(), path, fs, effective, sink);
}

diag::Outcome<std::shared_ptr<const Technology>> reload_sections(platform::FileSystem& fs, const Technology& current,
                                                                 const std::vector<std::string>& sections,
                                                                 const LoadOptions& options,
                                                                 diag::DiagnosticSink& sink) {
  if (current.file_name().empty()) {
    return diag::make("TECH-FILE-003", "Invalid technology file load.");
  }
  const platform::Path path{current.file_name()};
  const auto text = fs.read(path);
  if (!text) {
    return diag::make("TECH-FILE-001", "Could not find file '" + path.string() +
                                           "' in any of these directories:\n         " +
                                           join_paths(options.search_path));
  }
  SectionMask selected = 0;
  for (const auto& name : sections) {
    const int id = find_section(name);
    if (id < 0) {
      return diag::make("TECH-SECTION-001", "Unrecognized section name: " + name);
    }
    selected |= section_bit(id);
  }
  Data start = current.data();
  std::erase_if(start.raw_sections, [&](const RawSection& raw) {
    const int id = find_section(raw.name);
    return id >= 0 && has_section(selected, id);
  });
  const SectionMask all = section_bit(kSecCount) - 1;
  RunResult result =
      run_load(RunInput{text.value(), path, fs, options, sink, all & ~selected, selected}, std::move(start));
  if (!result.ok) {
    return load_failure(path);
  }
  return publish(std::move(result.data));
}

GridScale apply_post_load(const Technology& tech, GridScale grid, diag::DiagnosticSink& sink) {
  const auto& cif = tech.cif_output_grid();
  if (cif && cif->minimum_grid) {
    // Q-TECH-27: a grid limit of 0 is treated as 1.
    grid = GridScale{std::max<std::int64_t>(cif->scale_factor / std::max<std::int64_t>(cif->grid_limit, 1), 1), 1};
  }
  if (grid.internal != 1 || grid.lambda != 1) {
    sink.emit(diag::make("TECH-LOAD-002", "Scaled tech values by " + std::to_string(grid.internal) + " / " +
                                              std::to_string(grid.lambda) + " to match internal grid scaling"));
    if (!check_grid_limit(tech, 1, 1)) {
      sink.emit(diag::make("TECH-LOAD-003", "WARNING:  Current grid scale is smaller than the minimum for the process!"));
    }
  }
  return grid;
}

std::string_view builtin_technology_text() noexcept {
  return kMinimumTech;
}

std::shared_ptr<const Technology> builtin_technology() {
  static const std::shared_ptr<const Technology> minimum = [] {
    platform::MemoryFileSystem fs;
    diag::NullSink sink;
    Data start = Data::builtin();
    start.name.clear();
    const platform::Path file{"minimum.tech"};
    RunResult result = run_load(RunInput{kMinimumTech, file, fs, LoadOptions{}, sink}, std::move(start));
    result.data.file_name.clear();  // the bootstrap does not count as a user choice
    return publish(std::move(result.data));
  }();
  return minimum;
}

diag::Status check_grid_limit(const Technology& tech, std::int64_t n, std::int64_t d) {
  const auto& cif = tech.cif_output_grid();
  if (!cif) {
    return diag::Done{};
  }
  const std::int64_t gridup = std::max<std::int64_t>(cif->grid_limit, 1) * cif->expander * d;
  const std::int64_t scaledown = cif->scale_factor * n * 10;
  if (gridup == 0 || scaledown / gridup == 0 || scaledown % gridup != 0) {
    return diag::make("TECH-GRID-002", "Grid scaling is finer than limit set by the process!");
  }
  return diag::Done{};
}

GridScale reduce(GridScale scale) noexcept {
  if (scale.internal <= 0 || scale.lambda <= 0) {
    return GridScale{};
  }
  const auto g = std::gcd(scale.internal, scale.lambda);
  return GridScale{scale.internal / g, scale.lambda / g};
}

}  // namespace hefesto::tech
