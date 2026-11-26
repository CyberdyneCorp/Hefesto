// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Waveform measurements": y(x) piecewise linear between samples, binary64.
// Owner: SIMRES. See raw.hpp.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "hefesto/sim/result/raw.hpp"

namespace hefesto::sim {

namespace {

using Samples = std::vector<double>;

struct Wave {
  const Samples* x = nullptr;
  Samples y;
};

diag::Diagnostic bad(const std::string& text) { return diag::make("SIM-RESULT-006", text); }

std::string number(double v) {
  std::string s = std::to_string(v);
  s.erase(s.find_last_not_of('0') + 1);
  if (!s.empty() && s.back() == '.') {
    s.pop_back();
  }
  return s;
}

double part_of(const Vector& v, std::size_t i, std::string_view part) {
  const double re = v.real[i];
  const double im = i < v.imag.size() ? v.imag[i] : 0.0;
  if (part == "re") {
    return re;
  }
  if (part == "im") {
    return im;
  }
  if (part == "db") {
    return 20.0 * std::log10(std::hypot(re, im));
  }
  if (part == "ph") {
    return std::atan2(im, re) * 180.0 / std::numbers::pi;
  }
  return std::hypot(re, im);  // the magnitude
}

const Vector* find_vector(const Plot& plot, const std::string& name) {
  for (const Vector& v : plot.vectors) {
    if (v.name == name || display_name(v) == name) {
      return &v;
    }
  }
  return nullptr;
}

diag::Outcome<Wave> wave_of(const Plot& plot, const std::string& name, const std::optional<std::string>& part) {
  const Vector* v = find_vector(plot, name);
  if (v == nullptr) {
    return diag::make("SIM-RESULT-005", "no vector " + name + " in plot " + plot.name);
  }
  const Vector& scale = plot.vectors.front();
  if (v->real.size() != scale.real.size() || v->real.empty()) {
    return bad("vector " + name + " does not match the scale");
  }
  Wave w{&scale.real, {}};
  if (!v->complex && !part) {
    w.y = v->real;
    return w;
  }
  w.y.reserve(v->real.size());
  for (std::size_t i = 0; i < v->real.size(); ++i) {
    w.y.push_back(part_of(*v, i, part.value_or(v->complex ? "mag" : "re")));
  }
  return w;
}

diag::Outcome<double> value_at(const Wave& w, double x0) {
  const Samples& x = *w.x;
  if (x0 < x.front() || x0 > x.back()) {
    return bad("value_at " + number(x0) + " is outside [" + number(x.front()) + ", " + number(x.back()) + "]");
  }
  for (std::size_t i = 1; i < x.size(); ++i) {
    if (x0 <= x[i]) {
      const double dx = x[i] - x[i - 1];
      return dx == 0.0 ? w.y[i] : w.y[i - 1] + (w.y[i] - w.y[i - 1]) * (x0 - x[i - 1]) / dx;
    }
  }
  return w.y.back();
}

double extreme(const Wave& w, bool maximum) {
  const auto it = maximum ? std::max_element(w.y.begin(), w.y.end()) : std::min_element(w.y.begin(), w.y.end());
  return *it;
}

// The integral of y (or y squared) over [a, b], by segments.
double integral(const Wave& w, double a, double b, bool squared) {
  const Samples& x = *w.x;
  double sum = 0.0;
  for (std::size_t i = 1; i < x.size(); ++i) {
    const double lo = std::max(a, x[i - 1]);
    const double hi = std::min(b, x[i]);
    if (hi <= lo || x[i] == x[i - 1]) {
      continue;
    }
    const double slope = (w.y[i] - w.y[i - 1]) / (x[i] - x[i - 1]);
    const double y0 = w.y[i - 1] + slope * (lo - x[i - 1]);
    const double y1 = w.y[i - 1] + slope * (hi - x[i - 1]);
    sum += squared ? (hi - lo) * (y0 * y0 + y0 * y1 + y1 * y1) / 3.0 : (hi - lo) * (y0 + y1) / 2.0;
  }
  return sum;
}

diag::Outcome<double> average(const Wave& w, const std::vector<double>& args, bool rms) {
  const double a = args.size() >= 2 ? args[0] : w.x->front();
  const double b = args.size() >= 2 ? args[1] : w.x->back();
  if (!(b > a)) {
    return bad("the range must have b > a");
  }
  const double mean = integral(w, a, b, rms) / (b - a);
  return rms ? std::sqrt(mean) : mean;
}

const char* dir_name(int dir) { return dir > 0 ? "rise" : (dir < 0 ? "fall" : "either"); }

// The direction of a crossing at segment i (the sample equal to the level
// counts once, in the direction of the next differing sample).
int direction_after(const Samples& y, std::size_t i, double level) {
  for (std::size_t j = i; j < y.size(); ++j) {
    if (y[j] != level) {
      return y[j] > level ? 1 : -1;
    }
  }
  return 0;
}

struct Crossing {
  double at = 0.0;
  int dir = 0;
};

// The crossing that ends at sample i, if any.
std::optional<Crossing> crossing_at(const Wave& w, std::size_t i, double level) {
  const Samples& x = *w.x;
  const Samples& y = w.y;
  if (y[i] == level) {
    if (i > 0 && y[i - 1] == level) {
      return std::nullopt;  // counted at the first equal sample
    }
    const int d = direction_after(y, i + 1, level);
    if (d == 0 || (i > 0 && (y[i - 1] > level) == (d > 0))) {
      return std::nullopt;  // ends on the level, or touches it and returns
    }
    return Crossing{x[i], d};
  }
  if (i == 0 || y[i - 1] == level || (y[i - 1] < level) == (y[i] < level)) {
    return std::nullopt;
  }
  return Crossing{x[i - 1] + (level - y[i - 1]) * (x[i] - x[i - 1]) / (y[i] - y[i - 1]), y[i] > y[i - 1] ? 1 : -1};
}

diag::Outcome<double> cross(const Wave& w, double level, int k, int dir) {
  int seen = 0;
  for (std::size_t i = 0; i < w.y.size(); ++i) {
    const auto c = crossing_at(w, i, level);
    if (c && (dir == 0 || c->dir == dir) && ++seen == k) {
      return c->at;
    }
  }
  return diag::make("SIM-RESULT-007",
                    "no crossing of " + number(level) + " (" + dir_name(dir) + ", k=" + std::to_string(k) + ")");
}

diag::Outcome<double> cross_args(const Wave& w, const std::vector<double>& args, std::size_t from) {
  if (args.size() < from + 3 || !(args[from + 1] >= 1.0 && args[from + 1] <= 1e9)) {
    return bad("cross needs level, k >= 1 and dir");
  }
  const double dir = args[from + 2];
  return cross(w, args[from], static_cast<int>(args[from + 1]), dir > 0.0 ? 1 : (dir < 0.0 ? -1 : 0));
}

diag::Outcome<double> delay(const Plot& plot, const WaveformMeasure& m, const Wave& trig) {
  auto targ_wave = wave_of(plot, m.target.value_or(m.vector), m.part);
  if (!targ_wave.ok()) {
    return targ_wave.error();
  }
  auto t0 = cross_args(trig, m.arguments, 0);
  if (!t0.ok()) {
    return t0;
  }
  auto t1 = cross_args(targ_wave.value(), m.arguments, 3);
  if (!t1.ok()) {
    return t1;
  }
  return t1.value() - t0.value();
}

diag::Outcome<double> risetime(const Wave& w, const std::vector<double>& args) {
  if (args.size() < 2) {
    return bad("risetime needs lo and hi");
  }
  const double lo = args[0];
  const double hi = args[1];
  const double f_lo = args.size() >= 4 ? args[2] : 0.1;
  const double f_hi = args.size() >= 4 ? args[3] : 0.9;
  auto a = cross(w, lo + f_lo * (hi - lo), 1, 1);
  if (!a.ok()) {
    return a;
  }
  auto b = cross(w, lo + f_hi * (hi - lo), 1, 1);
  if (!b.ok()) {
    return b;
  }
  return b.value() - a.value();
}

}  // namespace

diag::Outcome<double> measure(const Plot& plot, const WaveformMeasure& m) {
  auto wave = wave_of(plot, m.vector, m.part);
  if (!wave.ok()) {
    return wave.error();
  }
  const Wave& w = wave.value();
  const std::string& f = m.function;
  if (f == "value_at") {
    return m.arguments.empty() ? diag::Outcome<double>{bad("value_at needs x0")} : value_at(w, m.arguments[0]);
  }
  if (f == "min" || f == "max") {
    return extreme(w, f == "max");
  }
  if (f == "avg" || f == "rms") {
    return average(w, m.arguments, f == "rms");
  }
  if (f == "cross") {
    return cross_args(w, m.arguments, 0);
  }
  if (f == "delay") {
    return delay(plot, m, w);
  }
  if (f == "risetime") {
    return risetime(w, m.arguments);
  }
  return diag::make("SIM-RESULT-008", "unknown measurement " + f);
}

}  // namespace hefesto::sim
