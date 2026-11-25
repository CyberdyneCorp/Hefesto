// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Control block translation" (capability schematic-simulation-integration;
// design D13): the `.control` ... `.endc` blocks of a schematic-source deck,
// translated line by line before validation. `write` and `wrdata` become
// capture points (kCaptureEcho), `quit` and `exit` are removed, the commands
// that reach the host (`shell`, `cd`, `load`, `source`, `codemodel`, `osdi`,
// `pre_osdi`) fail with SCHEM-SIM-001, and every other line is kept.
//
// Each `;`-separated command of a line is checked. A command word that the
// deck text cannot name statically (a `$variable` or `!history` word) is
// refused too, as is an `alias` or a backquoted command whose own command
// word is refused: either would reach a refused command at run time
// (hardening; both modes of `xschem-compat`).
//
// Owner: SIMINT.

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/schem/sim/deck.hpp"
#include "schem/sim/text.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kRefusedCode = "SCHEM-SIM-001";

constexpr std::array<std::string_view, 7> kRefused = {"shell", "cd", "load", "source", "codemodel", "osdi",
                                                      "pre_osdi"};

bool refused_name(std::string_view word) {
  const std::string name = sim_text::lower(word);
  return std::find(kRefused.begin(), kRefused.end(), name) != kRefused.end();
}

// A command word whose command is only known at run time.
bool indirect(std::string_view word) {
  return !word.empty() && (word.front() == '$' || word.front() == '!');
}

std::optional<std::string> refused_command(std::string_view command);

// The refused command a backquoted part of `command` runs, if any.
std::optional<std::string> refused_backquote(std::string_view command) {
  for (std::size_t open = command.find('`'); open != std::string_view::npos;) {
    const std::size_t close = command.find('`', open + 1);
    const std::string_view inside =
        command.substr(open + 1, close == std::string_view::npos ? std::string_view::npos : close - open - 1);
    if (auto refused = refused_command(inside)) return refused;
    if (close == std::string_view::npos) break;
    open = command.find('`', close + 1);
  }
  return std::nullopt;
}

// The command word that makes `command` refused, if any.
std::optional<std::string> refused_command(std::string_view command) {
  const std::vector<std::string_view> words = sim_text::words(command);
  if (words.empty()) return std::nullopt;
  const std::string_view first = words.front();
  if (refused_name(first) || indirect(first)) return sim_text::lower(first);
  if (sim_text::lower(first) == "alias" && words.size() >= 3 && (refused_name(words[2]) || indirect(words[2]))) {
    return sim_text::lower(words[2]);
  }
  return refused_backquote(command);
}

diag::Diagnostic refusal(const std::string& command) {
  return diag::make(kRefusedCode, "Control command '" + command + "' is not allowed in simulation decks");
}

class Translator {
 public:
  explicit Translator(Deck& deck) : deck_(deck) {}

  // The translated text of one control line; nullopt when it is removed.
  // An error is left in error().
  std::optional<std::string> line(std::string_view text) {
    const std::string_view body = sim_text::trim(text);
    if (body.empty() || body.front() == '*' || body.front() == '#') return std::string{text};
    std::vector<std::string> kept;
    for (const std::string_view command : sim_text::split(body, ';')) {
      if (auto refused = refused_command(command)) {
        error_ = refusal(*refused);
        return std::nullopt;
      }
      if (auto translated = this->command(sim_text::trim(command))) kept.push_back(std::move(*translated));
    }
    if (kept.empty()) return std::nullopt;
    std::string out{text.substr(0, text.size() - sim_text::trim_left(text).size())};
    for (std::size_t i = 0; i < kept.size(); ++i) out += (i == 0 ? "" : "; ") + kept[i];
    return out;
  }

  [[nodiscard]] const std::optional<diag::Diagnostic>& error() const noexcept { return error_; }

 private:
  std::optional<std::string> command(std::string_view command) {
    const std::vector<std::string_view> words = sim_text::words(command);
    if (words.empty()) return std::string{command};
    const std::string name = sim_text::lower(words.front());
    if (name == "quit" || name == "exit") return std::nullopt;
    if (name == "write" || name == "wrdata") return capture(words, name == "wrdata");
    if ((name == "set" || name == "unset") && words.size() >= 2 &&
        sim_text::lower(words[1].substr(0, words[1].find('='))) == "appendwrite") {
      append_ = name == "set";
    }
    return std::string{command};
  }

  std::string capture(const std::vector<std::string_view>& words, bool text) {
    DeckCapture capture;
    capture.name = words.size() > 1 ? std::string{words[1]} : std::string{"rawspice.raw"};
    for (std::size_t i = 2; i < words.size(); ++i) capture.vectors.emplace_back(words[i]);
    capture.text = text;
    capture.append = append_;
    deck_.captures.push_back(std::move(capture));
    return std::string{kCaptureEcho} + std::to_string(deck_.captures.size() - 1);
  }

  Deck& deck_;
  bool append_ = false;
  std::optional<diag::Diagnostic> error_;
};

}  // namespace

diag::Outcome<Deck> translate_control(std::string_view text) {
  Deck deck;
  Translator translator(deck);
  bool inside = false;
  for (const std::string_view line : sim_text::lines(text)) {
    const std::string keyword = sim_text::lower(sim_text::first_word(line));
    if (!inside || keyword == ".endc") {
      inside = inside ? false : keyword == ".control";
      deck.control = deck.control || inside;
      deck.text.append(line).push_back('\n');
      continue;
    }
    const std::optional<std::string> translated = translator.line(line);
    if (translator.error()) return *translator.error();
    if (translated) deck.text.append(*translated).push_back('\n');
  }
  if (!text.empty() && text.back() != '\n') deck.text.pop_back();
  return deck;
}

}  // namespace hefesto::schem
