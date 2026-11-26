// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Supported analyses". Owner: SIMRES. See analysis.hpp.

#include "hefesto/sim/result/analysis.hpp"

#include <algorithm>

#include "hefesto/sim/deck/spice_text.hpp"

namespace hefesto::sim {

namespace {

diag::Diagnostic bad(const char* analysis, const std::string& what) {
  return diag::make("SIM-RESULT-010", std::string{analysis} + ": " + what);
}

std::optional<diag::Diagnostic> check_source(const char* analysis, const std::string& name,
                                             const std::vector<std::string>& sources) {
  if (sources.empty()) {
    return std::nullopt;
  }
  const std::string lower = ascii_lower(name);
  const bool known = std::any_of(sources.begin(), sources.end(),
                                 [&](const std::string& s) { return ascii_lower(s) == lower; });
  return known ? std::nullopt : std::optional{bad(analysis, "source " + name + " does not exist")};
}

std::string sweep_text(const DcSweep& s) {
  return s.source + " " + format_g12(s.start) + " " + format_g12(s.stop) + " " + format_g12(s.step);
}

diag::Outcome<std::string> dc_card(const Analysis& a, const std::vector<std::string>& sources) {
  for (const DcSweep* s : {&a.sweep1, a.sweep2 ? &*a.sweep2 : nullptr}) {
    if (s == nullptr) {
      continue;
    }
    if (s->step == 0.0) {
      return bad("dc", "step must not be 0");
    }
    if (auto e = check_source("dc", s->source, sources)) {
      return *e;
    }
  }
  return ".dc " + sweep_text(a.sweep1) + (a.sweep2 ? " " + sweep_text(*a.sweep2) : std::string{});
}

// The frequency sweep of ac and noise.
std::optional<diag::Diagnostic> check_sweep(const char* analysis, const Analysis& a) {
  if (a.sweep != "dec" && a.sweep != "oct" && a.sweep != "lin") {
    return bad(analysis, "sweep must be dec, oct or lin");
  }
  if (a.points < 1) {
    return bad(analysis, "n must be >= 1");
  }
  if (a.fstart <= 0.0 && a.sweep != "lin") {
    return bad(analysis, "fstart must be > 0 for " + a.sweep + " sweep");
  }
  if (a.fstop < a.fstart) {
    return bad(analysis, "fstop must be >= fstart");
  }
  return std::nullopt;
}

std::string sweep_card(const Analysis& a) {
  return a.sweep + " " + std::to_string(a.points) + " " + format_g12(a.fstart) + " " + format_g12(a.fstop);
}

diag::Outcome<std::string> tran_card(const Analysis& a) {
  const double tstart = a.tstart.value_or(0.0);
  if (a.tstep <= 0.0) {
    return bad("tran", "tstep must be > 0");
  }
  if (a.tstop <= tstart) {
    return bad("tran", "tstop must be > tstart");
  }
  if (a.tmax && *a.tmax <= 0.0) {
    return bad("tran", "tmax must be > 0");
  }
  std::string card = ".tran " + format_g12(a.tstep) + " " + format_g12(a.tstop);
  if (a.tstart || a.tmax) {
    card += " " + format_g12(tstart);
  }
  if (a.tmax) {
    card += " " + format_g12(*a.tmax);
  }
  return a.uic ? card + " uic" : card;
}

diag::Outcome<std::string> noise_card(const Analysis& a, const std::vector<std::string>& sources) {
  if (auto e = check_sweep("noise", a)) {
    return *e;
  }
  if (auto e = check_source("noise", a.input, sources)) {
    return *e;
  }
  const std::string out = "v(" + a.output + (a.reference.empty() ? "" : "," + a.reference) + ")";
  return ".noise " + out + " " + a.input + " " + sweep_card(a);
}

}  // namespace

diag::Outcome<std::string> analysis_card(const Analysis& a, const std::vector<std::string>& sources) {
  switch (a.kind) {
    case Analysis::Kind::op: return std::string{".op"};
    case Analysis::Kind::dc: return dc_card(a, sources);
    case Analysis::Kind::ac:
      if (auto e = check_sweep("ac", a)) {
        return *e;
      }
      return ".ac " + sweep_card(a);
    case Analysis::Kind::tran: return tran_card(a);
    case Analysis::Kind::noise: return noise_card(a, sources);
    case Analysis::Kind::tf:
      if (auto e = check_source("tf", a.input, sources)) {
        return *e;
      }
      return ".tf " + a.output + " " + a.input;
  }
  return std::string{".op"};
}

}  // namespace hefesto::sim
