// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic circuit source (capability schematic-simulation-integration
// "Schematic circuit source", "Testbench code blocks"; design D13): the
// testbench's in-memory `hier` netlist, its control blocks translated
// (control.cpp), its `.include` and `.lib` paths resolved, then handed to
// simulation-netlist-preparation through DeckOptions::validate.
//
// Owner: SIMINT.

#include "hefesto/schem/sim/deck.hpp"

#include <optional>
#include <string>
#include <utility>

#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/session.hpp"
#include "schem/sim/text.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kUnresolvedCode = "SCHEM-SIM-002";
constexpr std::string_view kNetlistFailedCode = "SCHEM-SIM-004";

// A path word of a `.include`/`.lib` line, without its quotes.
std::string_view unquoted(std::string_view word) {
  if (word.size() >= 2 && (word.front() == '"' || word.front() == '\'') && word.back() == word.front()) {
    return word.substr(1, word.size() - 2);
  }
  return word;
}

// True when `path` lies under `root`, component by component.
bool is_under(const platform::Path& path, const platform::Path& root) {
  auto p = path.begin();
  for (auto r = root.begin(); r != root.end(); ++r, ++p) {
    if (p == path.end() || *p != *r) return false;
  }
  return !root.empty();
}

// An absolute `written` path as it is, or through the first PDK path
// mapping under which the file exists; nullopt when none does.
std::optional<platform::Path> mapped_pdk_path(const platform::Path& written, platform::FileSystem& fs,
                                              const DeckOptions& options) {
  if (fs.exists(written)) return written;
  for (const PdkPathMapping& mapping : options.pdk_paths) {
    if (!is_under(written, mapping.from)) continue;
    const platform::Path mapped = mapping.to / written.lexically_relative(mapping.from);
    if (fs.exists(mapped)) return mapped;
  }
  return std::nullopt;
}

// "Testbench code blocks" steps 1-3: the file `written` names, or nullopt.
std::optional<platform::Path> resolve_path(const platform::Path& written, platform::FileSystem& fs,
                                           const platform::Path& schematic_folder, const DeckOptions& options) {
  if (written.is_absolute()) return mapped_pdk_path(written, fs, options);
  for (const platform::Path& folder : {options.netlist_folder, schematic_folder}) {
    if (folder.empty()) continue;
    const platform::Path candidate = (folder / written).lexically_normal();
    if (fs.exists(candidate)) return candidate;
  }
  return std::nullopt;
}

// The index of the path word of a deck line, or nullopt for a line that
// names no file (`.lib <section>` opens a section of a library file).
std::optional<std::size_t> path_word(const std::vector<std::string_view>& words) {
  if (words.size() < 2) return std::nullopt;
  const std::string keyword = sim_text::lower(words.front());
  if (keyword == ".include" || keyword == ".inc") return 1;
  if (keyword == ".lib" && words.size() >= 3) return 1;
  return std::nullopt;
}

// `line` with its path word resolved, or with a SCHEM-SIM-002 warning in
// `deck` when the file is not found.
std::string resolve_line(std::string_view line, Deck& deck, platform::FileSystem& fs,
                         const platform::Path& schematic_folder, const DeckOptions& options) {
  const std::vector<std::string_view> words = sim_text::words(line);
  const std::optional<std::size_t> index = path_word(words);
  if (!index) return std::string{line};
  const std::string_view word = words[*index];
  const std::string written{unquoted(word)};
  const std::optional<platform::Path> found = resolve_path(written, fs, schematic_folder, options);
  if (!found) {
    deck.diagnostics.push_back(diag::make(kUnresolvedCode, "Simulation deck: cannot find \"" + written + "\" of `" +
                                                               std::string{sim_text::trim(line)} +
                                                               "`; kept as written"));
    return std::string{line};
  }
  const std::size_t at = static_cast<std::size_t>(word.data() - line.data());
  std::string out{line};
  out.replace(at, word.size(), found->generic_string());
  return out;
}

diag::Outcome<Deck> finish_deck(NetlistResult netlisted, platform::FileSystem& fs, const Document& testbench,
                                const DeckOptions& options) {
  if (netlisted.failed) {
    std::string why = "the testbench netlist failed";
    for (const diag::Diagnostic& d : netlisted.diagnostics) {
      if (d.severity == diag::Severity::error) {
        why = d.text;
        break;
      }
    }
    return diag::make(kNetlistFailedCode, "Simulation deck not built: " + why);
  }
  diag::Outcome<Deck> translated = translate_control(netlisted.text);
  if (!translated.ok()) return translated.error();
  Deck deck = std::move(translated).value();
  deck.diagnostics = std::move(netlisted.diagnostics);
  resolve_deck_paths(deck, fs, testbench.path.parent_path(), options);
  if (options.validate) {
    const diag::Status valid = options.validate(deck);
    if (!valid.ok()) return valid.error();
  }
  return deck;
}

}  // namespace

void resolve_deck_paths(Deck& deck, platform::FileSystem& fs, const platform::Path& schematic_folder,
                        const DeckOptions& options) {
  std::string out;
  bool inside = false;
  for (const std::string_view line : sim_text::lines(deck.text)) {
    const std::string keyword = sim_text::lower(sim_text::first_word(line));
    if (inside || keyword == ".control") {
      inside = keyword != ".endc";
      out.append(line).push_back('\n');
      continue;
    }
    out.append(resolve_line(line, deck, fs, schematic_folder, options)).push_back('\n');
  }
  if (!deck.text.empty() && deck.text.back() != '\n') out.pop_back();
  deck.text = std::move(out);
}

diag::Outcome<Deck> build_deck(const NetlistEnvironment& environment, const Document& testbench,
                               const DeckOptions& options, const compat::CompatSettings& compat) {
  return finish_deck(netlist(environment, testbench, NetlistOptions{}, compat), environment.fs, testbench, options);
}

diag::Outcome<Deck> build_deck(const Session& session, const Document& testbench) {
  const compat::CompatSettings compat = session.compat != nullptr ? *session.compat : compat::CompatSettings{};
  platform::LocalFileSystem local;
  platform::FileSystem& fs = session.files != nullptr ? *session.files : local;
  return finish_deck(netlist(session, testbench, NetlistOptions{}, compat), fs, testbench, DeckOptions{});
}

}  // namespace hefesto::schem
