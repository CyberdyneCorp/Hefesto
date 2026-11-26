// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMRES. See progress.hpp.

#include "hefesto/sim/result/progress.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "hefesto/sim/deck/spice_text.hpp"

namespace hefesto::sim {

namespace {

constexpr std::array<std::string_view, 11> kAnalyses = {".op", ".dc",  ".ac",  ".tran",  ".noise", ".tf",
                                                        ".pz", ".sp",  ".pss", ".disto", ".sens"};

bool is_analysis(std::string_view card) {
  return std::find(kAnalyses.begin(), kAnalyses.end(), card) != kAnalyses.end();
}

// `.tran tstep tstop [tstart [tmax]] [uic]`: the range tstart..tstop.
void tran_range(const std::vector<std::string>& tokens, AnalysisSpan& span) {
  if (tokens.size() > 2) {
    span.stop = parse_spice_number(tokens[2]).value_or(0.0);
  }
  if (tokens.size() > 3) {
    span.start = parse_spice_number(tokens[3]).value_or(0.0);
  }
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

}  // namespace

std::vector<AnalysisSpan> analyses_of(const std::vector<std::string>& deck) {
  std::vector<AnalysisSpan> out;
  bool control = false;
  for (const std::string& line : deck) {
    const std::vector<std::string> tokens = spice_tokens(line);
    if (tokens.empty()) {
      continue;
    }
    const std::string card = ascii_lower(tokens.front());
    if (card == ".control" || card == ".endc") {
      control = card == ".control";
      continue;
    }
    if (control || !is_analysis(card)) {
      continue;
    }
    AnalysisSpan span{card.substr(1)};
    if (span.name == "tran") {
      tran_range(tokens, span);
    }
    out.push_back(std::move(span));
  }
  return out;
}

ProgressStatus parse_progress(std::string_view status, double point) {
  ProgressStatus out;
  std::string_view text = trim(status);
  if (point < 0.0) {
    // `<analysis>: <p>%` still in one string.
    const std::size_t colon = text.rfind(':');
    if (colon != std::string_view::npos && text.ends_with('%')) {
      if (const auto p = parse_spice_number(trim(text.substr(colon + 1)))) {
        point = *p;
        text = trim(text.substr(0, colon));
      }
    }
  }
  if (text == "--ready--") {
    out.kind = ProgressStatus::Kind::ready;
  } else if (text.starts_with("shooting:")) {
    out.kind = ProgressStatus::Kind::shooting;
    out.iteration = static_cast<std::uint64_t>(std::max(0.0, parse_spice_number(trim(text.substr(9))).value_or(0.0)));
  } else if (point >= 0.0) {
    out.kind = ProgressStatus::Kind::percent;
    out.analysis = ascii_lower(text);
    out.fraction = point / 100.0;
  } else if (!text.empty() && text.find_first_of(" :") == std::string_view::npos) {
    out.kind = ProgressStatus::Kind::start;
    out.analysis = ascii_lower(text);
  }
  return out;
}

ProgressTracker::ProgressTracker(std::vector<AnalysisSpan> analyses) : analyses_(std::move(analyses)) {}

void ProgressTracker::status(std::string_view status, double point) {
  const ProgressStatus parsed = parse_progress(status, point);
  switch (parsed.kind) {
    case ProgressStatus::Kind::percent:
      enter(parsed.analysis);
      advance(parsed.fraction);
      break;
    case ProgressStatus::Kind::start: enter(parsed.analysis); break;
    case ProgressStatus::Kind::shooting: iteration_ = parsed.iteration; break;
    case ProgressStatus::Kind::ready: advance(1.0); break;
    case ProgressStatus::Kind::other: break;
  }
}

void ProgressTracker::scale(double t) {
  if (current_ >= analyses_.size()) {
    return;
  }
  const AnalysisSpan& span = analyses_[current_];
  if (span.name == "tran" && span.stop > span.start) {
    advance((t - span.start) / (span.stop - span.start));
  }
}

double ProgressTracker::progress() const noexcept {
  if (analyses_.empty()) {
    return fraction_;
  }
  return std::min(1.0, (static_cast<double>(current_) + fraction_) / static_cast<double>(analyses_.size()));
}

// A later analysis of that name becomes the current one; the analyses
// before it count as completed.
void ProgressTracker::enter(std::string_view analysis) {
  for (std::size_t i = current_; i < analyses_.size(); ++i) {
    if (analyses_[i].name != analysis) {
      continue;
    }
    if (i != current_) {
      current_ = i;
      fraction_ = 0.0;
      iteration_.reset();
    }
    return;
  }
}

void ProgressTracker::advance(double fraction) {
  if (!std::isnan(fraction)) {
    fraction_ = std::max(fraction_, std::clamp(fraction, 0.0, 1.0));
  }
}

}  // namespace hefesto::sim
