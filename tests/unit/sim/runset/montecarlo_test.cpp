// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// simulation-monte-carlo (M9 R3, task 15.3): modes, the dimension, sample
// seeds, the portable stream, reproducibility, parallel execution with a
// crashing sample, early stopping, user statistical parameters, the `sim mc`
// checks (library API; the command is task 14.1), sample independence from
// worker assignment and the sample limit. The engine's own side (patch 0003,
// patch 0005) is tested in third_party/ngspice/tests.
//
// Owner: SIMADV.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/sim/runset/montecarlo.hpp"
#include "hefesto/sim/runset/point_deck.hpp"
#include "hefesto/sim/runset/random.hpp"
#include "hefesto/sim/runset/runset.hpp"
#include "runset_fixture.hpp"

namespace sim = hefesto::sim;
namespace diag = hefesto::diag;
using namespace hefesto::test::simrunset;

namespace {

sim::CornerTable table(std::string_view pdk) {
  auto t = sim::bundled_corner_table(pdk);
  REQUIRE(t.ok());
  return t.value();
}

std::vector<std::string> sample_deck(const sim::CornerTable& t, sim::McMode mode, const std::string& corner,
                                     const std::vector<std::string>& base) {
  auto applied = sim::mc_application(t, mode, corner);
  REQUIRE(applied.ok());
  sim::PointChanges c;
  c.job_id = "rs-p000001";
  c.variables = t.variables;
  c.monte_carlo = applied.value();
  c.mode = mode;
  std::vector<diag::Diagnostic> warnings;
  auto deck = sim::point_deck(base, c, warnings);
  REQUIRE(deck.ok());
  return deck.value();
}

bool contains(const std::vector<std::string>& lines, const std::string& line) {
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}

sim::RunSet mc_run_set(const sim::MonteCarloSpec& spec, const std::string& id = "20261127-150000-tb-rs") {
  sim::RunSet rs;
  rs.spec.id = id;
  auto dim = sim::mc_dimension(spec);
  REQUIRE(dim.ok());
  rs.spec.dimensions = {dim.value()};
  rs.context.testbench = "tb";
  rs.context.corner = "tt";
  rs.context.base = base_deck();
  rs.context.base.lines.insert(rs.context.base.lines.begin() + 1, ".lib \"/s/sky130A/libs.tech/combined/sky130.lib.spice\" tt");
  rs.context.corners = table("sky130A");
  rs.context.monte_carlo = spec;
  REQUIRE(rs.prepare().ok());
  return rs;
}

}  // namespace

// --- Monte Carlo modes ---

TEST_CASE("sky130 mismatch at the slow corner", "[cap:simulation-monte-carlo][req:Monte Carlo modes]") {
  const auto deck = sample_deck(table("sky130A"), sim::McMode::mismatch, "ss",
                                {"* tb", ".lib \"/store/sky130A/libs.tech/combined/sky130.lib.spice\" ss", ".end"});
  CHECK(contains(deck, ".lib \"/store/sky130A/libs.tech/combined/sky130.lib.spice\" ss_mm"));
  CHECK(std::none_of(deck.begin(), deck.end(), [](const std::string& l) { return l.find("MC_PR_SWITCH") != std::string::npos; }));
  const auto both = sample_deck(table("sky130A"), sim::McMode::both, "tt",
                                {"* tb", ".lib \"/s/sky130.lib.spice\" tt", "R1 a 0 1", ".end"});
  CHECK(both == std::vector<std::string>{"* tb [rs-p000001]", ".lib \"/s/sky130.lib.spice\" mc", ".param MC_MM_SWITCH=1",
                                         "R1 a 0 1", ".end"});
}

TEST_CASE("gf180 process and mismatch", "[cap:simulation-monte-carlo][req:Monte Carlo modes]") {
  const auto deck = sample_deck(table("gf180mcuD"), sim::McMode::both, "typical",
                                {"* tb", ".include \"/store/gf180mcuD/libs.tech/ngspice/design.ngspice\"",
                                 ".lib \"/store/gf180mcuD/libs.tech/ngspice/sm141064.ngspice\" typical",
                                 "XM1 d g 0 0 nfet_03v3 w=1u l=0.28u", ".end"});
  CHECK(deck == std::vector<std::string>{"* tb [rs-p000001]",
                                         ".include \"/store/gf180mcuD/libs.tech/ngspice/design.ngspice\"",
                                         ".lib \"/store/gf180mcuD/libs.tech/ngspice/sm141064.ngspice\" statistical",
                                         ".param sw_stat_global=1", ".param sw_stat_mismatch=1",
                                         "XM1 d g 0 0 nfet_03v3 w=1u l=0.28u", ".end"});
}

TEST_CASE("sg13g2 both modes", "[cap:simulation-monte-carlo][req:Monte Carlo modes]") {
  const auto both = sim::mc_application(table("sg13g2"), sim::McMode::both, "tt");
  REQUIRE_FALSE(both.ok());
  CHECK(both.error().text == "sg13g2 does not define Monte Carlo mode both");
  const auto mm = sim::mc_application(table("sg13g2"), sim::McMode::mismatch, "ss");
  REQUIRE(mm.ok());
  CHECK(mm.value().words[0].second == "mos_ss_mismatch");
  CHECK(mm.value().words[2].second == "res_typ_mismatch");
  // Every PDK device instance needs mm_ok=1 to vary.
  const auto warnings = sim::mm_ok_warnings({"XM1 d g 0 0 sg13_lv_nmos w=1u l=0.13u mm_ok=1",
                                             "XM2 d g 0 0 sg13_lv_pmos w=1u l=0.13u", "XR1 a b rppd w=1u l=2u mm_ok=0",
                                             "Xother a b mycell"});
  REQUIRE(warnings.size() == 2);
  CHECK(warnings[0].text == "Monte Carlo mismatch: instance XM2 has mm_ok=0; it will not vary");
}

TEST_CASE("Process Monte Carlo at a non-typical corner is refused unless it has a statistical counterpart",
          "[cap:simulation-monte-carlo][req:Monte Carlo modes]") {
  const auto ss = sim::mc_application(table("sky130A"), sim::McMode::process, "ss");
  REQUIRE_FALSE(ss.ok());
  CHECK(ss.error().text == "Monte Carlo mode process is not defined at corner ss");
  CHECK(sim::mc_application(table("sky130A"), sim::McMode::process, "tt").ok());
  CHECK(sim::mc_application(table("gf180mcuC"), sim::McMode::both, "ff").error().code == "SIM-RUNSET-009");
}

// --- Monte Carlo dimension ---

TEST_CASE("Extending a run", "[cap:simulation-monte-carlo][req:Monte Carlo dimension]") {
  const auto small = sim::mc_levels(sim::MonteCarloSpec{sim::McMode::process, 100, 5, true, {}, false});
  const auto large = sim::mc_levels(sim::MonteCarloSpec{sim::McMode::process, 200, 5, true, {}, false});
  REQUIRE(small.size() == 101);
  REQUIRE(large.size() == 201);
  CHECK(small[0].label() == "nom");
  CHECK(small[0].seed(5) == sim::kNominalSeed);
  for (std::size_t level = 1; level < small.size(); ++level) {
    CHECK(small[level].index == level - 1);
    CHECK(small[level].seed(5) == large[level].seed(5));
  }
  const auto no_nominal = sim::mc_levels(sim::MonteCarloSpec{sim::McMode::process, 3, 5, false, {}, false});
  CHECK(no_nominal[0].label() == "0");
}

TEST_CASE("The nominal sample uses the corner's ordinary words and none of the mode's parameters",
          "[cap:simulation-monte-carlo][req:Monte Carlo dimension]") {
  sim::RunSet rs = mc_run_set(sim::MonteCarloSpec{sim::McMode::both, 2, 1, true, {}, false});
  std::vector<diag::Diagnostic> warnings;
  const auto nominal = rs.point_deck(0, warnings).value().lines;
  const auto sample = rs.point_deck(1, warnings).value().lines;
  CHECK(nominal[1] == ".lib \"/s/sky130A/libs.tech/combined/sky130.lib.spice\" tt");
  CHECK(sample[1] == ".lib \"/s/sky130A/libs.tech/combined/sky130.lib.spice\" mc");
  CHECK(sample[2] == ".param MC_MM_SWITCH=1");
  CHECK(nominal[2] != ".param MC_MM_SWITCH=1");
  CHECK(rs.points[0].seed == 1);
  CHECK(rs.points[1].seed == 875784623);
}

// --- Sample seeds ---

TEST_CASE("Seeds for base seed 1", "[cap:simulation-monte-carlo][req:Sample seeds]") {
  CHECK(sim::sample_seed(1, 0).value() == 875784623);
  CHECK(sim::sample_seed(1, 1).value() == 2120735756);
  CHECK(sim::sample_seed(1, 2).value() == 1645304719);
}

TEST_CASE("Seeds for base seed 42", "[cap:simulation-monte-carlo][req:Sample seeds]") {
  CHECK(sim::sample_seed(42, 0).value() == 1749773200);
  CHECK(sim::sample_seed(42, 1).value() == 112509509);
  CHECK(sim::sample_seed(0, 1).error().code == "SIM-RUNSET-010");
  CHECK(sim::sample_seed(2147483648U, 1).error().code == "SIM-RUNSET-010");
}

TEST_CASE("Seed option removed from a schematic deck", "[cap:simulation-monte-carlo][req:Sample seeds]") {
  std::vector<diag::Diagnostic> warnings;
  auto lines = sim::strip_seed_settings({"* tb", ".option seed=12", ".options reltol=1e-3 seed=3", ".control",
                                         "setseed 7", "set rndseed=9", "run", ".endc", ".end"},
                                        warnings);
  REQUIRE(lines.ok());
  CHECK(lines.value() == std::vector<std::string>{"* tb", ".options reltol=1e-3", ".control", "run", ".endc", ".end"});
  REQUIRE(warnings.size() == 4);
  CHECK(warnings[0].text == "Removed seed setting on line 2; Monte Carlo seeds are set by Hefesto");
  CHECK(warnings[3].text == "Removed seed setting on line 6; Monte Carlo seeds are set by Hefesto");
  for (const char* line : {".control", "set stat_param_as_func", ".option statlocal=1"}) {
    std::vector<diag::Diagnostic> w;
    auto refused = sim::strip_seed_settings({"* tb", line}, w);
    if (std::string_view{line} != ".control") {
      REQUIRE_FALSE(refused.ok());
      CHECK(refused.error().code == "SIM-RUNSET-011");
    }
  }
  std::vector<diag::Diagnostic> w;
  CHECK(sim::strip_seed_settings({"* tb", "set stat_param_as_func"}, w).error().text ==
        "stat_param_as_func is not supported in Monte Carlo runs");
}

// --- Portable random numbers ---

TEST_CASE("Generator state for seed 1", "[cap:simulation-monte-carlo][req:Portable random numbers]") {
  // Patch 0003 seeds ngspice's eight state words as 129 + (v_{k-1}(s) mod
  // 32639) from the same stream Hefesto uses for sample seeds.
  std::vector<std::uint64_t> states;
  for (std::uint64_t k = 0; k < 8; ++k) {
    states.push_back(129 + sim::random_value(1, k) % 32639);
  }
  CHECK(states == std::vector<std::uint64_t>{29963, 3443, 21757, 5511, 10925, 11052, 26345, 26017});
}

// --- Reproducibility ---

TEST_CASE("A sample's deck and seed depend only on the base deck, the base seed and the index",
          "[cap:simulation-monte-carlo][req:Reproducibility]") {
  sim::RunSet a = mc_run_set(sim::MonteCarloSpec{sim::McMode::mismatch, 5, 3, true, {}, false});
  sim::RunSet b = mc_run_set(sim::MonteCarloSpec{sim::McMode::mismatch, 9, 3, true, {}, false});
  std::vector<diag::Diagnostic> warnings;
  for (std::size_t i = 0; i < a.points.size(); ++i) {
    CHECK(a.point_deck(i, warnings).value().lines == b.point_deck(i, warnings).value().lines);
    CHECK(a.points[i].seed == b.points[i].seed);
  }
  // runset.json records the base deck's hash, so a changed deck is detected.
  CHECK(sim::runset_json(a, {}).find("\"deckSha256\": \"base-sha\"") != std::string::npos);
}

// --- Parallel and sequential execution ---

TEST_CASE("Worker crash during a run", "[cap:simulation-monte-carlo][req:Parallel and sequential execution]") {
  Host host;
  host.crash_jobs.insert(sim::point_job_id("20261127-150000-tb-rs", 234));  // sample 233 (level 234: nominal on)
  sim::JobManager jobs = manager(host, 4);
  sim::RunSet rs = mc_run_set(sim::MonteCarloSpec{sim::McMode::mismatch, 500, 1, true, {}, false});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  run_to_end(jobs, runner);
  CHECK(runner.state() == sim::RunSetState::completed);
  std::size_t completed = 0;
  for (std::size_t i = 0; i < rs.points.size(); ++i) {
    const sim::JobRecord* rec = jobs.record(rs.records[i].job);
    REQUIRE(rec != nullptr);
    if (i == 234) {
      CHECK(rec->job.state == sim::JobState::failed);
      CHECK(rec->diagnostics.front().text == "simulation engine terminated");
    } else {
      completed += rec->job.state == sim::JobState::completed ? 1 : 0;
    }
  }
  CHECK(completed == 500);  // 499 samples and the nominal sample
  CHECK(host.engines >= 2);  // the crashed worker was replaced
}

// --- Failed samples and early stopping ---

TEST_CASE("Early stop at a checkpoint", "[cap:simulation-monte-carlo][req:Failed samples and early stopping]") {
  std::vector<sim::SampleOutcome> samples(1000, sim::SampleOutcome{true, true, false});
  for (const std::size_t i : {3U, 40U, 120U, 200U, 260U}) {
    samples[i].pass = false;
  }
  CHECK(sim::early_stop(samples, 0.01, false) == std::optional<std::size_t>{500});
  CHECK(std::abs(*sim::wilson_half_width(445, 450) - 0.01049) < 1e-5);
  CHECK(std::abs(*sim::wilson_half_width(495, 500) - 0.00946) < 1e-5);
  // A checkpoint needs every sample below it final.
  samples[499].final = false;
  CHECK(sim::early_stop(samples, 0.01, false) == std::nullopt);
  CHECK_FALSE(sim::wilson_half_width(0, 0).has_value());
}

TEST_CASE("Failed samples are excluded from yield with failedSamples = exclude",
          "[cap:simulation-monte-carlo][req:Failed samples and early stopping]") {
  std::vector<sim::SampleOutcome> samples(100, sim::SampleOutcome{true, true, false});
  for (std::size_t i = 0; i < 10; ++i) {
    samples[i] = sim::SampleOutcome{true, false, true};  // failed runs
  }
  CHECK(sim::early_stop(samples, 0.05, false) == std::nullopt);              // 90/100 does not reach 0.05
  // Excluded: checkpoint 50 counts samples 10..49 only, 40 of 40 pass (half-width 0.0438).
  CHECK(sim::early_stop(samples, 0.05, true) == std::optional<std::size_t>{50});
}

TEST_CASE("A run set stops early and cancels the samples after the checkpoint",
          "[cap:simulation-monte-carlo][req:Failed samples and early stopping]") {
  Host host;
  sim::JobManager jobs = manager(host, 2);
  sim::RunSet rs = mc_run_set(sim::MonteCarloSpec{sim::McMode::mismatch, 400, 1, false, 0.08, false});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  run_to_end(jobs, runner);
  CHECK(runner.state() == sim::RunSetState::completed);
  REQUIRE(runner.stopped_early() == std::optional<std::size_t>{50});  // 50/50 passes: half-width 0.0357
  CHECK(runner.done() < 400);
  CHECK(sim::runset_json(rs, sim::RunSetInfo{"completed", sim::Retention::all, "sky130A", "1", 50, ""})
            .find("\"stoppedEarly\": 50") != std::string::npos);
}

// --- User statistical parameters ---

TEST_CASE("Resistor tolerance", "[cap:simulation-monte-carlo][req:User statistical parameters]") {
  const sim::StatParameter rload{"rload", sim::StatParameter::Distribution::gauss, {10e3, 0.05, 3}, false};
  CHECK(sim::stat_parameter_line(rload, sim::McMode::process, false).value() == ".param rload='gauss(10000,0.05,3)'");
  CHECK(sim::stat_parameter_line(rload, sim::McMode::process, true).value() == ".param rload=10000");
  CHECK(sim::stat_parameter_line(rload, sim::McMode::mismatch, false).value() == ".param rload=10000");
  const sim::StatParameter bad{"x", sim::StatParameter::Distribution::aunif, {1, 2, 3}, false};
  CHECK(sim::stat_parameter_line(bad, sim::McMode::process, false).error().code == "SIM-RUNSET-010");
}

TEST_CASE("Mismatch parameters go into the DUT subcircuit and need a netlist DUT",
          "[cap:simulation-monte-carlo][req:User statistical parameters]") {
  sim::PointChanges c;
  c.job_id = "p1";
  c.monte_carlo = sim::McApplication{};
  c.mode = sim::McMode::mismatch;
  c.stat_parameters = {{"dvt", sim::StatParameter::Distribution::agauss, {0, 0.01, 3}, true},
                       {"rl", sim::StatParameter::Distribution::unif, {1e3, 0.1}, false}};
  c.dut_subckt = "inv";
  std::vector<diag::Diagnostic> warnings;
  auto deck = sim::point_deck({"* tb", ".lib /m.lib tt", ".subckt inv a y", "R1 a y 1", ".ends", ".end"}, c, warnings);
  REQUIRE(deck.ok());
  CHECK(deck.value() == std::vector<std::string>{"* tb [p1]", ".lib /m.lib tt", ".param rl=1000", ".subckt inv a y",
                                                  ".param dvt='agauss(0,0.01,3)'", "R1 a y 1", ".ends", ".end"});
  c.dut_subckt.clear();  // a raw deck
  CHECK(sim::point_deck({"* tb", ".end"}, c, warnings).error().text == "Mismatch parameter dvt needs a layout or netlist DUT");
}

// --- Monte Carlo command options (library API) ---

TEST_CASE("Script query of a seed", "[cap:simulation-monte-carlo][req:Monte Carlo command options]") {
  const sim::MonteCarloSpec spec{sim::McMode::mismatch, 100, 1, true, {}, false};
  CHECK(sim::check_monte_carlo(spec).ok());
  CHECK(sim::sample_seed(spec.base_seed, 2).value() == 1645304719);
}

TEST_CASE("Invalid run count", "[cap:simulation-monte-carlo][req:Monte Carlo command options]") {
  const auto status = sim::check_monte_carlo(sim::MonteCarloSpec{sim::McMode::process, 0, 1, true, {}, false});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().text == "sim mc: runs must be between 1 and 100000");
  CHECK(sim::check_monte_carlo(sim::MonteCarloSpec{sim::McMode::process, 10, 0, true, {}, false}).error().code ==
        "SIM-RUNSET-010");
}

// --- Sample independence from worker assignment ---

TEST_CASE("Same sample on different workers", "[cap:simulation-monte-carlo][req:Sample independence from worker assignment]") {
  // A sample's result is identical on worker 1 of 1 and on any worker of N,
  // each worker having run different samples before.
  std::vector<std::vector<std::vector<sim::Vector>>> runs;
  for (const std::uint32_t workers : {1U, parallel_workers()}) {
    Host host;
    sim::JobManager jobs = manager(host, workers);
    sim::RunSet rs = mc_run_set(sim::MonteCarloSpec{sim::McMode::mismatch, 200, 42, false, {}, false});
    sim::RunSetRunner runner(rs, jobs);
    REQUIRE(runner.start().ok());
    run_to_end(jobs, runner);
    runs.push_back(vectors_by_point(rs, jobs));
  }
  CHECK(same_vectors({runs[0][137]}, {runs[1][137]}));
  CHECK(same_vectors(runs[0], runs[1]));
}

// --- Sample count limit ---

TEST_CASE("Too many samples", "[cap:simulation-monte-carlo][req:Sample count limit]") {
  const sim::MonteCarloSpec spec{sim::McMode::process, 200000, 1, true, {}, false};
  CHECK(sim::check_monte_carlo(spec).error().text == "sim mc: runs must be between 1 and 100000");
  CHECK(sim::mc_dimension(spec).error().code == "SIM-RUNSET-010");
  // The limit can be lowered.
  sim::RunSetLimits lowered;
  lowered.max_samples = 10;
  CHECK(sim::mc_dimension(sim::MonteCarloSpec{sim::McMode::process, 11, 1, true, {}, false}, lowered).error().text ==
        "sim mc: runs must be between 1 and 10");
  // 100 000 samples plus the nominal one pass the sample limit but not the point limit.
  sim::RunSetSpec rs;
  rs.dimensions = {sim::mc_dimension(sim::MonteCarloSpec{sim::McMode::process, 100000, 1, true, {}, false}).value()};
  CHECK(sim::RunSet::generate(rs).error().text == "Run set has 100001 points; the limit is 100000");
}
