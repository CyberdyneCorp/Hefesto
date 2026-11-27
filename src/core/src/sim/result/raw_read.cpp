// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Reading raw files ("Raw file format" reading rules, "Raw file import"):
// header lines in any order up to `Variables:`, unknown header lines
// ignored, `Binary:` and `Values:`, padded and unpadded, several plots.
// Nothing is allocated for a plot before its declared size is checked
// against the budget, which counts every Vector object and its name and
// type strings as well as the values (M9 R2 review: a header of many
// variables and 0 points passed the check). Owner: SIMRES. See raw.hpp.

#include <bit>
#include <cstring>
#include <limits>
#include <utility>

#include "hefesto/sim/deck/spice_text.hpp"
#include "hefesto/sim/result/raw.hpp"

namespace hefesto::sim {

namespace {

struct Header {
  std::string plotname;
  bool complex = false;
  std::optional<std::uint64_t> variables;
  std::optional<std::uint64_t> points;
};

std::optional<std::uint64_t> parse_count(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  std::uint64_t value = 0;
  std::size_t digits = 0;
  for (; digits < text.size() && text[digits] >= '0' && text[digits] <= '9'; ++digits) {
    if (value > (std::numeric_limits<std::uint64_t>::max() - 9) / 10) {
      return std::nullopt;
    }
    value = value * 10 + static_cast<std::uint64_t>(text[digits] - '0');
  }
  return digits == 0 ? std::nullopt : std::optional{value};
}

// The whitespace-separated words of `line` (names such as `v(out)` stay whole).
std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> tokens;
  for (std::string_view rest = line; !rest.empty();) {
    const std::size_t at = rest.find_first_not_of(" \t");
    if (at == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(at);
    const std::size_t end = rest.find_first_of(" \t");
    tokens.emplace_back(rest.substr(0, end));
    rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end);
  }
  return tokens;
}

class RawReader {
 public:
  RawReader(std::string_view data, std::string_view name, std::uint64_t budget)
      : data_(data), name_(name), budget_(budget) {}

  diag::Outcome<std::vector<Plot>> read() {
    std::vector<Plot> plots;
    skip_blank_lines();
    while (pos_ < data_.size()) {
      auto plot = read_plot();
      if (!plot.ok()) {
        return plot.error();
      }
      plots.push_back(std::move(plot.value()));
      skip_blank_lines();
    }
    if (plots.empty()) {
      return error("expected \"Title:\"");
    }
    return plots;
  }

 private:
  // --- lines ---

  std::optional<std::string_view> next_line() {
    if (pos_ >= data_.size()) {
      return std::nullopt;
    }
    const std::size_t end = data_.find('\n', pos_);
    std::string_view line = data_.substr(pos_, end == std::string_view::npos ? std::string_view::npos : end - pos_);
    pos_ = end == std::string_view::npos ? data_.size() : end + 1;
    ++line_;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    return line;
  }

  void skip_blank_lines() {
    while (pos_ < data_.size()) {
      const std::size_t end = data_.find('\n', pos_);
      const std::string_view line = data_.substr(pos_, end == std::string_view::npos ? std::string_view::npos : end - pos_);
      if (line.find_first_not_of(" \t\r") != std::string_view::npos) {
        return;
      }
      pos_ = end == std::string_view::npos ? data_.size() : end + 1;
      ++line_;
    }
  }

  diag::Diagnostic error(const std::string& what) const {
    return diag::make("SIM-RESULT-001", std::string{name_} + ":" + std::to_string(line_ + 1) + ": " + what);
  }

  // --- the header ---

  diag::Status read_header(Header& h) {
    for (;;) {
      const auto line = next_line();
      if (!line) {
        --line_;
        return error("expected \"Variables:\"");
      }
      if (line->starts_with("Variables:")) {
        return check_header(h);
      }
      apply_header_line(*line, h);
    }
  }

  static void apply_header_line(std::string_view line, Header& h) {
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) {
      return;  // not a header line: ignored
    }
    const std::string key = ascii_lower(line.substr(0, colon));
    std::string_view value = line.substr(colon + 1);
    while (!value.empty() && value.front() == ' ') {
      value.remove_prefix(1);
    }
    if (key == "plotname") {
      h.plotname = std::string{value};
    } else if (key == "flags") {
      h.complex = ascii_lower(value).find("complex") != std::string::npos;
    } else if (key == "no. variables") {
      h.variables = parse_count(value);
    } else if (key == "no. points") {
      h.points = parse_count(value);
    }
  }

  diag::Status check_header(const Header& h) {
    --line_;  // name the `Variables:` line
    diag::Status status = diag::Done{};
    if (!h.variables || *h.variables == 0) {
      status = error("expected \"No. Variables:\"");
    } else if (!h.points) {
      status = error("expected \"No. Points:\"");
    } else if (over_budget(*h.variables, *h.points, h.complex)) {
      status = budget_error(std::to_string(*h.variables) + " variables of " + std::to_string(*h.points) + " points");
    }
    ++line_;
    return status;
  }

  diag::Diagnostic budget_error(const std::string& what) const {
    return diag::make("SIM-RESULT-003", std::string{name_} + ": " + what + " exceed the result budget of " +
                                            std::to_string(budget_) + " bytes");
  }

  [[nodiscard]] std::uint64_t room() const { return budget_ - std::min(used_, budget_); }

  /// Charges each variable's Vector and its values; its name and type are
  /// charged as they are read (charge()).
  bool over_budget(std::uint64_t variables, std::uint64_t points, bool complex) {
    const std::uint64_t per_value = complex ? 16 : 8;
    if (points > room() / per_value) {
      return true;
    }
    const std::uint64_t per_variable = sizeof(Vector) + points * per_value;
    if (variables > room() / per_variable) {
      return true;
    }
    used_ += variables * per_variable;
    return false;
  }

  bool charge(std::uint64_t bytes) {
    if (bytes > room()) {
      return false;
    }
    used_ += bytes;
    return true;
  }

  // --- variables and data ---

  diag::Status read_variables(Plot& plot, std::uint64_t count) {
    plot.vectors.reserve(count);  // charged by over_budget
    for (std::uint64_t i = 0; i < count; ++i) {
      const auto line = next_line();
      const std::vector<std::string> tokens = line ? words(*line) : std::vector<std::string>{};
      if (tokens.size() < 3) {
        return error("expected variable " + std::to_string(i));
      }
      if (!charge(tokens[1].size() + tokens[2].size())) {
        return budget_error("the names of " + std::to_string(i + 1) + " variables");
      }
      Vector v;
      v.name = tokens[1];
      v.type = tokens[2];
      plot.vectors.push_back(std::move(v));
    }
    return diag::Done{};
  }

  diag::Status read_binary(Plot& plot, std::uint64_t points, bool complex) {
    const std::size_t per_value = complex ? 16 : 8;
    const std::size_t per_point = per_value * plot.vectors.size();
    const std::uint64_t available = (data_.size() - pos_) / per_point;
    if (available < points) {
      return diag::make("SIM-RESULT-002", std::string{name_} + ": binary data ends after point " +
                                              std::to_string(available) + " of " + std::to_string(points));
    }
    for (Vector& v : plot.vectors) {
      v.complex = complex;
      v.real.reserve(points);
      if (complex) {
        v.imag.reserve(points);
      }
    }
    for (std::uint64_t p = 0; p < points; ++p) {
      for (Vector& v : plot.vectors) {
        v.real.push_back(take_double());
        if (complex) {
          v.imag.push_back(take_double());
        }
      }
    }
    return diag::Done{};
  }

  double take_double() {
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < 8; ++i) {
      bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(data_[pos_ + i])) << (8U * i);
    }
    pos_ += 8;
    return std::bit_cast<double>(bits);
  }

  // One ASCII value: `re` or `re,im`.
  diag::Status ascii_value(std::string_view token, Vector& v, bool complex) {
    const std::size_t comma = token.find(',');
    const auto re = parse_spice_number(token.substr(0, comma));
    const auto im = comma == std::string_view::npos ? std::optional{0.0} : parse_spice_number(token.substr(comma + 1));
    if (!re || !im) {
      return error("expected a number");
    }
    v.real.push_back(*re);
    if (complex) {
      v.imag.push_back(*im);
    }
    return diag::Done{};
  }

  // Variable i of point p: ` <p>\t<value>` for the first, `\t<value>` for the others.
  diag::Status ascii_line(Plot& plot, std::size_t i, std::uint64_t p, std::uint64_t points, bool complex) {
    skip_blank_lines();
    const auto line = next_line();
    if (!line) {
      return error("values end after point " + std::to_string(p) + " of " + std::to_string(points));
    }
    const std::vector<std::string> tokens = words(*line);
    if (tokens.size() != (i == 0 ? 2U : 1U)) {
      return error(i == 0 ? "expected a point index and a value" : "expected a value");
    }
    return ascii_value(tokens.back(), plot.vectors[i], complex);
  }

  diag::Status read_ascii(Plot& plot, std::uint64_t points, bool complex) {
    for (Vector& v : plot.vectors) {
      v.complex = complex;
    }
    for (std::uint64_t p = 0; p < points; ++p) {
      for (std::size_t i = 0; i < plot.vectors.size(); ++i) {
        if (diag::Status s = ascii_line(plot, i, p, points, complex); !s.ok()) {
          return s;
        }
      }
    }
    return diag::Done{};
  }

  diag::Outcome<Plot> read_plot() {
    Header h;
    if (diag::Status s = read_header(h); !s.ok()) {
      return s.error();
    }
    Plot plot;
    plot.name = h.plotname;
    if (diag::Status s = read_variables(plot, *h.variables); !s.ok()) {
      return s.error();
    }
    const auto marker = next_line();
    diag::Status data = diag::Done{};
    if (marker && marker->starts_with("Binary:")) {
      data = read_binary(plot, *h.points, h.complex);
    } else if (marker && marker->starts_with("Values:")) {
      data = read_ascii(plot, *h.points, h.complex);
    } else {
      data = error("expected \"Binary:\" or \"Values:\"");
    }
    if (!data.ok()) {
      return data.error();
    }
    return plot;
  }

  std::string_view data_;
  std::string_view name_;
  std::uint64_t budget_;
  std::uint64_t used_ = 0;
  std::size_t pos_ = 0;
  std::size_t line_ = 0;  // lines consumed so far
};

}  // namespace

diag::Outcome<std::vector<Plot>> parse_raw(std::string_view data, std::string_view name, std::uint64_t budget) {
  RawReader reader{data, name, budget};
  return reader.read();
}

}  // namespace hefesto::sim
