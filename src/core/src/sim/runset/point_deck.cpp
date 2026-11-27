// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Point decks (point_deck.hpp).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/point_deck.hpp"

#include <algorithm>
#include <cctype>

#include "hefesto/sim/deck/spice_text.hpp"
#include "sim/runset/words.hpp"

namespace hefesto::sim {

namespace {

std::string first_word(const std::string& line) {
  std::size_t i = 0;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
    ++i;
  }
  std::size_t j = i;
  while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])) == 0) {
    ++j;
  }
  return ascii_lower(std::string_view{line}.substr(i, j - i));
}

/// true for the top-level cards of `lines` (not the title, not inside a
/// `.subckt` body or a `.control` block).
std::vector<bool> top_level(const std::vector<std::string>& lines) {
  std::vector<bool> out(lines.size(), false);
  int depth = 0;
  bool control = false;
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const std::string w = first_word(lines[i]);
    if (w == ".control") {
      control = true;
    }
    if (w == ".subckt") {
      ++depth;
    }
    out[i] = depth == 0 && !control && w != ".ends" && w != ".endc";
    if (w == ".endc") {
      control = false;
    }
    if (w == ".ends" && depth > 0) {
      --depth;
    }
  }
  return out;
}

bool is_model_line(const std::string& line) {
  const std::string w = first_word(line);
  return w == ".lib" || w == ".include" || w == ".inc";
}

// One `name=expression` of a `.param` line: the name and the expression's
// byte range.
struct Assignment {
  std::string name;
  std::size_t begin = 0;
  std::size_t end = 0;
};

std::size_t skip_space(const std::string& s, std::size_t i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])) != 0) {
    ++i;
  }
  return i;
}

// The end of an expression: the next whitespace outside quotes and brackets.
std::size_t expression_end(const std::string& s, std::size_t i) {
  int depth = 0;
  char quote = 0;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (quote != 0) {
      quote = c == quote ? 0 : quote;
    } else if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '(' || c == '{') {
      ++depth;
    } else if (c == ')' || c == '}') {
      depth = std::max(0, depth - 1);
    } else if (depth == 0 && std::isspace(static_cast<unsigned char>(c)) != 0) {
      break;
    }
  }
  return i;
}

std::vector<Assignment> assignments(const std::string& line) {
  std::vector<Assignment> out;
  std::size_t i = skip_space(line, 0);
  i += 6;  // `.param`
  while (true) {
    i = skip_space(line, i);
    const std::size_t name_begin = i;
    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) != 0 || line[i] == '_')) {
      ++i;
    }
    const std::size_t name_end = i;
    i = skip_space(line, i);
    if (name_end == name_begin || i >= line.size() || line[i] != '=') {
      return out;
    }
    i = skip_space(line, i + 1);
    const std::size_t end = expression_end(line, i);
    out.push_back(Assignment{ascii_lower(std::string_view{line}.substr(name_begin, name_end - name_begin)), i, end});
    i = end;
  }
}

diag::Status set_parameter(std::vector<std::string>& lines, const std::string& name, double value) {
  const std::vector<bool> top = top_level(lines);
  const std::string key = ascii_lower(name);
  std::optional<std::pair<std::size_t, Assignment>> found;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (!top[i] || first_word(lines[i]) != ".param") {
      continue;
    }
    for (const Assignment& a : assignments(lines[i])) {
      if (a.name != key) {
        continue;
      }
      if (found) {
        return diag::make("SIM-RUNSET-005", "Parameter " + name + " is defined more than once at top level");
      }
      found = std::pair{i, a};
    }
  }
  const std::string text = format_g12(value);
  if (!found) {
    lines.insert(lines.begin() + 1, ".param " + name + "=" + text);
    return diag::Done{};
  }
  const auto& [i, a] = *found;
  lines[i].replace(a.begin, a.end - a.begin, text);
  return diag::Done{};
}

std::string join(const std::vector<std::string>& tokens) {
  std::string out;
  for (const std::string& t : tokens) {
    out += (out.empty() ? "" : " ") + t;
  }
  return out;
}

void set_temperature(std::vector<std::string>& lines, double value) {
  const std::vector<bool> top = top_level(lines);
  const std::string text = format_g12(value);
  bool found = false;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string w = top[i] ? first_word(lines[i]) : std::string{};
    if (w == ".temp") {
      lines[i] = ".temp " + text;
      found = true;
    } else if (w == ".option" || w == ".options") {
      std::vector<std::string> tokens = runset_text::words(lines[i]);
      for (std::string& t : tokens) {
        if (ascii_lower(t).starts_with("temp=")) {
          t = t.substr(0, 5) + text;
          found = true;
        }
      }
      lines[i] = join(tokens);
    }
  }
  if (!found) {
    lines.insert(lines.begin() + 1, ".temp " + text);
  }
}

/// The DC value token of the top-level V or I line named `name`.
std::optional<std::pair<std::size_t, std::size_t>> dc_token(const std::vector<std::string>& lines,
                                                            const std::vector<bool>& top, const std::string& name) {
  const std::string key = ascii_lower(name);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::vector<std::string> tokens = top[i] ? runset_text::words(lines[i]) : std::vector<std::string>{};
    if (tokens.size() < 4 || ascii_lower(tokens[0]) != key) {
      continue;
    }
    const std::size_t at = ascii_lower(tokens[3]) == "dc" ? 4 : 3;
    if (at < tokens.size() && parse_spice_number(tokens[at])) {
      return std::pair{i, at};
    }
  }
  return std::nullopt;
}

diag::Status set_source(std::vector<std::string>& lines, const std::string& name, double value, bool scale) {
  const auto at = dc_token(lines, top_level(lines), name);
  if (!at) {
    return diag::make("SIM-RUNSET-004", "Sweep source:" + name + ": no top-level V or I line with a DC value");
  }
  std::vector<std::string> tokens = runset_text::words(lines[at->first]);
  const double base = parse_spice_number(tokens[at->second]).value_or(0.0);
  tokens[at->second] = format_g12(scale ? base * value : value);
  lines[at->first] = join(tokens);
  return diag::Done{};
}

void set_words(std::vector<std::string>& lines, const std::vector<std::pair<std::string, std::string>>& words,
               const std::vector<SectionVariable>& variables) {
  const std::vector<bool> top = top_level(lines);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    std::vector<std::string> tokens = top[i] ? runset_text::words(lines[i]) : std::vector<std::string>{};
    if (tokens.size() < 3 || ascii_lower(tokens[0]) != ".lib") {
      continue;
    }
    for (const SectionVariable& v : variables) {
      const bool owns = std::find(v.words.begin(), v.words.end(), tokens.back()) != v.words.end();
      const auto word = std::find_if(words.begin(), words.end(), [&v](const auto& w) { return w.first == v.name; });
      if (owns && word != words.end() && word->second != "-") {
        const std::size_t last = lines[i].rfind(tokens.back());
        lines[i].replace(last, tokens.back().size(), word->second);
        break;
      }
    }
  }
}

void insert_after_models(std::vector<std::string>& lines, const std::vector<std::string>& extra) {
  const std::size_t at = std::max<std::size_t>(after_last_model_line(lines), 1);
  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(at), extra.begin(), extra.end());
}

diag::Status apply_corner(std::vector<std::string>& lines, const PointChanges& c) {
  if (!c.corner) {
    return diag::Done{};
  }
  set_words(lines, c.corner->words, c.variables);
  std::vector<std::string> params;
  for (const auto& [name, expression] : c.corner->params) {
    params.push_back(".param " + name + "=" + expression);
  }
  insert_after_models(lines, params);
  return diag::Done{};
}

// Step 2: the `param:` levels.
diag::Status apply_parameters(std::vector<std::string>& lines, const PointChanges& c) {
  for (const auto& [name, value] : c.params) {
    if (auto status = set_parameter(lines, name, value); !status.ok()) {
      return status;
    }
  }
  return diag::Done{};
}

// Step 3: a `temp` level, else the corner's temperature.
diag::Status apply_temperature(std::vector<std::string>& lines, const PointChanges& c) {
  const std::optional<double> temp = c.temperature ? c.temperature : (c.corner ? c.corner->temperature : std::nullopt);
  if (temp) {
    set_temperature(lines, *temp);
  }
  return diag::Done{};
}

// Step 4: the supply scale (a `supply` level, else the corner's) on the
// `supply` stimuli, then the `source:` levels.
diag::Status apply_stimuli(std::vector<std::string>& lines, const PointChanges& c) {
  const std::optional<double> supply = c.supply ? c.supply : (c.corner ? c.corner->supply : std::nullopt);
  const std::vector<std::string> none;
  for (const std::string& source : supply && *supply != 1.0 ? c.supply_sources : none) {
    if (auto status = set_source(lines, source, *supply, true); !status.ok()) {
      return status;
    }
  }
  for (const auto& [name, value] : c.sources) {
    if (auto status = set_source(lines, name, value, false); !status.ok()) {
      return status;
    }
  }
  return diag::Done{};
}

diag::Status insert_mismatch_parameters(std::vector<std::string>& lines, const PointChanges& c,
                                        const std::vector<std::string>& mismatch) {
  if (mismatch.empty()) {
    return diag::Done{};
  }
  const std::string key = ascii_lower(c.dut_subckt);
  const auto it = std::find_if(lines.begin(), lines.end(), [&key](const std::string& l) {
    const std::vector<std::string> t = runset_text::words(l);
    return !key.empty() && t.size() > 1 && ascii_lower(t[0]) == ".subckt" && ascii_lower(t[1]) == key;
  });
  if (it == lines.end()) {
    const auto& p = *std::find_if(c.stat_parameters.begin(), c.stat_parameters.end(),
                                  [](const StatParameter& s) { return s.mismatch; });
    return diag::make("SIM-RUNSET-017", "Mismatch parameter " + p.name + " needs a layout or netlist DUT");
  }
  lines.insert(it + 1, mismatch.begin(), mismatch.end());
  return diag::Done{};
}

diag::Status apply_monte_carlo(std::vector<std::string>& lines, const PointChanges& c,
                               std::vector<diag::Diagnostic>& warnings) {
  if (!c.monte_carlo) {
    return diag::Done{};
  }
  auto stripped = strip_seed_settings(lines, warnings);
  if (!stripped.ok()) {
    return stripped.error();
  }
  lines = std::move(stripped.value());
  std::vector<std::string> top;
  std::vector<std::string> mismatch;
  if (!c.nominal) {
    set_words(lines, c.monte_carlo->words, c.variables);
    top = c.monte_carlo->params_after_models;
  }
  for (const StatParameter& p : c.stat_parameters) {
    auto line = stat_parameter_line(p, c.mode, c.nominal);
    if (!line.ok()) {
      return line.error();
    }
    (p.mismatch ? mismatch : top).push_back(std::move(line.value()));
  }
  insert_after_models(lines, top);
  return insert_mismatch_parameters(lines, c, mismatch);
}

}  // namespace

std::size_t after_last_model_line(const std::vector<std::string>& lines) {
  const std::vector<bool> top = top_level(lines);
  std::size_t at = 0;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (top[i] && is_model_line(lines[i])) {
      at = i + 1;
    }
  }
  return at;
}

diag::Outcome<std::vector<std::string>> point_deck(const std::vector<std::string>& base, const PointChanges& changes,
                                                  std::vector<diag::Diagnostic>& warnings) {
  if (base.empty()) {
    return diag::make("SIM-RUNSET-002", "the base deck is empty");
  }
  std::vector<std::string> lines = base;
  for (auto step : {apply_corner, apply_parameters, apply_temperature, apply_stimuli}) {
    if (auto status = step(lines, changes); !status.ok()) {
      return status.error();
    }
  }
  if (auto status = apply_monte_carlo(lines, changes, warnings); !status.ok()) {
    return status.error();
  }
  lines[0] += " [" + changes.job_id + "]";
  return lines;
}

}  // namespace hefesto::sim
