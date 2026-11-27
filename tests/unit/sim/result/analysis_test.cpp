// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Analysis cards and their validation (task 13.3; capability
// simulation-analyses-and-results "Supported analyses").
//
// Owner: SIMRES.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/sim/result/analysis.hpp"

namespace sim = hefesto::sim;
using Kind = sim::Analysis::Kind;

namespace {

std::string card(const sim::Analysis& a, const std::vector<std::string>& sources = {}) {
  const auto c = sim::analysis_card(a, sources);
  return c.ok() ? c.value() : "error: " + c.error().text;
}

}  // namespace

TEST_CASE("Transient card", "[cap:simulation-analyses-and-results][req:Supported analyses]") {
  sim::Analysis tran;
  tran.kind = Kind::tran;
  tran.tstep = 10e-12;
  tran.tstop = 20e-9;
  CHECK(card(tran) == ".tran 1e-11 2e-08");
  tran.tmax = 1e-10;
  CHECK(card(tran) == ".tran 1e-11 2e-08 0 1e-10");
  tran.uic = true;
  CHECK(card(tran) == ".tran 1e-11 2e-08 0 1e-10 uic");
  tran.tstep = 0;
  CHECK(card(tran) == "error: tran: tstep must be > 0");
  tran.tstep = 1e-12;
  tran.tstart = 30e-9;
  CHECK(card(tran) == "error: tran: tstop must be > tstart");
}

TEST_CASE("Invalid AC sweep", "[cap:simulation-analyses-and-results][req:Supported analyses]") {
  sim::Analysis ac;
  ac.kind = Kind::ac;
  ac.sweep = "dec";
  ac.points = 10;
  ac.fstart = 0;
  ac.fstop = 1e9;
  const auto c = sim::analysis_card(ac);
  REQUIRE_FALSE(c.ok());
  CHECK(c.error().code == "SIM-RESULT-010");
  CHECK(c.error().text == "ac: fstart must be > 0 for dec sweep");
  ac.fstart = 1;
  CHECK(card(ac) == ".ac dec 10 1 1000000000");
  ac.points = 0;
  CHECK(card(ac) == "error: ac: n must be >= 1");
}

TEST_CASE("dc, noise, tf and op cards check their sources",
          "[cap:simulation-analyses-and-results][req:Supported analyses]") {
  const std::vector<std::string> sources = {"Vin", "Vdd"};
  sim::Analysis dc;
  dc.kind = Kind::dc;
  dc.sweep1 = {"vin", 0, 1.8, 0.01};
  CHECK(card(dc, sources) == ".dc vin 0 1.8 0.01");
  dc.sweep2 = sim::DcSweep{"Vdd", 1.6, 2.0, 0.2};
  CHECK(card(dc, sources) == ".dc vin 0 1.8 0.01 Vdd 1.6 2 0.2");
  dc.sweep1.step = 0;
  CHECK(card(dc, sources) == "error: dc: step must not be 0");
  dc.sweep1 = {"Vx", 0, 1, 0.1};
  CHECK(card(dc, sources) == "error: dc: source Vx does not exist");

  sim::Analysis noise;
  noise.kind = Kind::noise;
  noise.output = "out";
  noise.input = "Vin";
  noise.sweep = "oct";
  noise.points = 5;
  noise.fstart = 10;
  noise.fstop = 1e6;
  CHECK(card(noise, sources) == ".noise v(out) Vin oct 5 10 1000000");
  noise.reference = "ref";
  CHECK(card(noise, sources) == ".noise v(out,ref) Vin oct 5 10 1000000");

  sim::Analysis tf;
  tf.kind = Kind::tf;
  tf.output = "v(out)";
  tf.input = "Vnope";
  CHECK(card(tf, sources) == "error: tf: source Vnope does not exist");
  CHECK(card(sim::Analysis{}) == ".op");
}
