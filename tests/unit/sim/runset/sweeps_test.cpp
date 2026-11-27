// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// simulation-sweeps (M9 R3, task 15.1): run sets, sweep variables and
// levels, point decks, DOE, execution on the pool, storage, views and
// results in point order. Execution uses the deterministic fake engine of
// runset_fixture.hpp through the real JobManager.
//
// Owner: SIMADV.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/runset/point_deck.hpp"
#include "hefesto/sim/runset/random.hpp"
#include "hefesto/sim/runset/runset.hpp"
#include "runset_fixture.hpp"

namespace sim = hefesto::sim;
namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
using namespace hefesto::test::simrunset;
using Catch::Matchers::WithinULP;

namespace {

sim::Dimension list_dim(const std::string& variable, std::vector<double> values) {
  auto d = sim::sweep_dimension(sim::SweepDefinition{variable, sim::SweepDefinition::Mode::list, 0, 0, 0, 0,
                                                     std::move(values)});
  REQUIRE(d.ok());
  return d.value();
}

std::vector<std::string> labels(const std::vector<double>& levels) {
  std::vector<std::string> out;
  for (const double v : levels) {
    out.push_back(sim::format_level(v));
  }
  return out;
}

sim::RunSet sweep_run_set(std::vector<sim::Dimension> dims, const std::string& id = "20261127-120000-tb-rs") {
  sim::RunSet rs;
  rs.spec.id = id;
  rs.spec.dimensions = std::move(dims);
  rs.context.testbench = "tb";
  rs.context.corner = "tt";
  rs.context.base = base_deck();
  rs.context.supply_sources = {"Vdd"};
  REQUIRE(rs.prepare().ok());
  return rs;
}

std::vector<std::string> deck_of(const sim::RunSet& rs, std::size_t point) {
  std::vector<diag::Diagnostic> warnings;
  auto deck = rs.point_deck(point, warnings);
  REQUIRE(deck.ok());
  return deck.value().lines;
}

}  // namespace

// --- Run sets ---

TEST_CASE("Point index of a nested sweep", "[cap:simulation-sweeps][req:Run sets]") {
  sim::RunSetSpec spec;
  spec.dimensions = {list_dim("temp", {-40, 125}), list_dim("param:vdd", {1.62, 1.8, 1.98})};
  auto points = sim::RunSet::generate(spec);
  REQUIRE(points.ok());
  REQUIRE(points.value().size() == 6);
  CHECK(points.value()[4].values == std::vector<double>{125, 1.8});
  CHECK(points.value()[4].levels == std::vector<std::size_t>{1, 1});
  CHECK(sim::point_job_id("20261127-120000-tb-rs", 4) == "20261127-120000-tb-rs-p000004");
}

TEST_CASE("Too many points", "[cap:simulation-sweeps][req:Run sets]") {
  std::vector<double> hundred(100);
  for (std::size_t i = 0; i < hundred.size(); ++i) {
    hundred[i] = static_cast<double>(i);
  }
  sim::RunSetSpec spec;
  spec.dimensions = {list_dim("param:a", hundred), list_dim("param:b", hundred), list_dim("param:c", hundred)};
  auto points = sim::RunSet::generate(spec);
  REQUIRE_FALSE(points.ok());
  CHECK(points.error().code == "SIM-RUNSET-001");
  CHECK(points.error().text == "Run set has 1000000 points; the limit is 100000");
  // At the limit: 100 x 1000 points are generated.
  std::vector<double> thousand(1000);
  spec.dimensions = {list_dim("param:a", hundred), list_dim("param:b", thousand)};
  auto at_limit = sim::RunSet::generate(spec);
  REQUIRE(at_limit.ok());
  CHECK(at_limit.value().size() == 100'000);
}

TEST_CASE("The point limit can be lowered and not raised past twice the default",
          "[cap:simulation-sweeps][req:Run sets]") {
  sim::RunSetSpec spec;
  spec.dimensions = {list_dim("param:a", {1, 2, 3}), list_dim("param:b", {1, 2, 3, 4})};
  spec.limits.max_points = 11;
  CHECK(sim::RunSet::generate(spec).error().text == "Run set has 12 points; the limit is 11");
  spec.limits.max_points = 12;
  CHECK(sim::RunSet::generate(spec).ok());
  spec.limits.max_points = 2 * sim::kMaxRunSetPoints + 1;
  CHECK(sim::RunSet::generate(spec).error().code == "SIM-RUNSET-015");
  spec.limits.max_points = 0;
  CHECK(sim::RunSet::generate(spec).error().code == "SIM-RUNSET-015");
}

TEST_CASE("A run set of no points and a Monte Carlo dimension that is not innermost are refused",
          "[cap:simulation-sweeps][req:Run sets]") {
  sim::RunSetSpec spec;
  sim::Dimension empty;
  empty.kind = sim::Dimension::Kind::sweep;
  empty.name = "param:x";
  spec.dimensions = {empty};
  CHECK(sim::RunSet::generate(spec).error().text == "Run set has no points");
  auto mc = sim::mc_dimension(sim::MonteCarloSpec{});
  REQUIRE(mc.ok());
  spec.dimensions = {mc.value(), list_dim("param:a", {1})};
  CHECK(sim::RunSet::generate(spec).error().code == "SIM-RUNSET-004");
}

TEST_CASE("A run set id carries the time and the testbench, with a suffix on collision",
          "[cap:simulation-sweeps][req:Run sets]") {
  const auto when = std::chrono::system_clock::time_point{} + std::chrono::hours(24 * 365 * 50);
  const std::string first = sim::make_run_set_id(when, "inv_tb", [](std::string_view) { return false; });
  CHECK(first.size() == 15 + 10);
  CHECK(first.ends_with("-inv_tb-rs"));
  const std::string second = sim::make_run_set_id(when, "inv_tb", [&first](std::string_view id) { return id == first; });
  CHECK(second == first + "-2");
}

// --- Sweep variables ---

TEST_CASE("Sweep variables parse and refuse the corner and unknown kinds", "[cap:simulation-sweeps][req:Sweep variables]") {
  CHECK(sim::parse_sweep_variable("param:wn").value().text() == "param:wn");
  CHECK(sim::parse_sweep_variable("temp").value().kind == sim::SweepVariableRef::Kind::temperature);
  CHECK(sim::parse_sweep_variable("supply").value().kind == sim::SweepVariableRef::Kind::supply);
  CHECK(sim::parse_sweep_variable("source:Vin").value().name == "Vin");
  CHECK(sim::parse_sweep_variable("corner").error().code == "SIM-RUNSET-004");
  CHECK(sim::parse_sweep_variable("param:9w").error().code == "SIM-RUNSET-004");
  CHECK(sim::parse_sweep_variable("width").error().code == "SIM-RUNSET-004");
}

TEST_CASE("Pulse source rejected", "[cap:simulation-sweeps][req:Sweep variables]") {
  sim::Stimulus vin;
  vin.name = "Vin";
  vin.kind = sim::Stimulus::Kind::pulse;
  sim::Stimulus vdd;
  vdd.name = "Vdd";
  const auto var = sim::parse_sweep_variable("source:Vin").value();
  const auto status = sim::check_source_variable(var, {vin, vdd});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().text == "Sweep source:Vin: only dc stimuli can be swept; use a parameter");
  CHECK(sim::check_source_variable(sim::parse_sweep_variable("source:vdd").value(), {vin, vdd}).ok());
}

TEST_CASE("Two dimensions of one variable, and temp or supply with a corner set, are refused",
          "[cap:simulation-sweeps][req:Sweep variables]") {
  sim::RunSetSpec spec;
  spec.dimensions = {list_dim("param:WN", {1}), list_dim("param:wn", {2})};  // case-insensitive
  CHECK(sim::RunSet::generate(spec).error().code == "SIM-RUNSET-004");
  spec.dimensions = {sim::corner_dimension({sim::CornerLevel{"tt_27_1v8", "tt", 27, 1}}), list_dim("supply", {0.9})};
  CHECK(sim::RunSet::generate(spec).error().text == "Sweep supply conflicts with the corner supply");
  spec.dimensions = {sim::corner_dimension({sim::CornerLevel{"tt_27_1v8", "tt", 27, 1}}), list_dim("temp", {0})};
  CHECK(sim::RunSet::generate(spec).error().text == "Sweep temp conflicts with the corner temp");
}

TEST_CASE("Sweep a transistor width", "[cap:simulation-sweeps][req:Sweep variables]") {
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2, 4})});
  REQUIRE(rs.points.size() == 3);
  const std::vector<std::string> expected{".param wn=1 ln=0.15", ".param wn=2 ln=0.15", ".param wn=4 ln=0.15"};
  for (std::size_t i = 0; i < 3; ++i) {
    const auto deck = deck_of(rs, i);
    CHECK(std::find(deck.begin(), deck.end(), expected[i]) != deck.end());
  }
}

// --- Sweep point generation ---

TEST_CASE("Linear sweep with a step", "[cap:simulation-sweeps][req:Sweep point generation]") {
  auto levels = sim::sweep_levels({"supply", sim::SweepDefinition::Mode::lin_step, 0.9, 1.1, 0.05, 0, {}}, 100000);
  REQUIRE(levels.ok());
  CHECK(labels(levels.value()) == std::vector<std::string>{"0.9", "0.95", "1", "1.05", "1.1"});
}

TEST_CASE("Decade sweep", "[cap:simulation-sweeps][req:Sweep point generation]") {
  auto levels = sim::sweep_levels({"param:rl", sim::SweepDefinition::Mode::dec, 1e3, 1e6, 0, 2, {}}, 100000);
  REQUIRE(levels.ok());
  REQUIRE(levels.value().size() == 7);
  CHECK(sim::format_level(levels.value()[1]) == "3162.27766017");
  CHECK(sim::format_level(levels.value()[6]) == "1000000");
  // Downwards, and octaves.
  auto down = sim::sweep_levels({"param:rl", sim::SweepDefinition::Mode::oct, 8, 1, 0, 1, {}}, 100000);
  REQUIRE(down.ok());
  CHECK(labels(down.value()) == std::vector<std::string>{"8", "4", "2", "1"});
}

TEST_CASE("Invalid logarithmic start", "[cap:simulation-sweeps][req:Sweep point generation]") {
  auto levels = sim::sweep_levels({"param:wn", sim::SweepDefinition::Mode::dec, 0, 10, 0, 2, {}}, 100000);
  REQUIRE_FALSE(levels.ok());
  CHECK(levels.error().text == "Sweep param:wn: dec start must be > 0");
}

TEST_CASE("Linear points, lists, and level counts above the limit before allocation",
          "[cap:simulation-sweeps][req:Sweep point generation]") {
  auto points = sim::sweep_levels({"param:x", sim::SweepDefinition::Mode::lin_points, 0, 1, 0, 3, {}}, 100000);
  CHECK(labels(points.value()) == std::vector<std::string>{"0", "0.5", "1"});
  CHECK(sim::sweep_levels({"param:x", sim::SweepDefinition::Mode::lin_points, 0, 1, 0, 1, {}}, 10).error().text ==
        "Sweep param:x: lin points must be >= 2");
  // Crafted input: a step of 1e-300 asks for ~1e300 levels; refused, never allocated.
  auto huge = sim::sweep_levels({"param:x", sim::SweepDefinition::Mode::lin_step, 0, 1, 1e-300, 0, {}}, 100000);
  REQUIRE_FALSE(huge.ok());
  CHECK(huge.error().code == "SIM-RUNSET-001");
  CHECK(sim::sweep_levels({"param:x", sim::SweepDefinition::Mode::lin_step, 1, 0, 0.5, 0, {}}, 10).error().code ==
        "SIM-RUNSET-003");
  CHECK(sim::sweep_levels({"param:x", sim::SweepDefinition::Mode::list, 0, 0, 0, 0, {2, 2}}, 10).value().size() == 2);
}

// --- Point decks ---

TEST_CASE("Parameter replaced in place", "[cap:simulation-sweeps][req:Point decks]") {
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {2})});
  const std::vector<std::string> deck = deck_of(rs, 0);
  std::vector<std::string> expected = base_deck().lines;
  expected[0] = "* inverter tb [20261127-120000-tb-rs-p000000]";
  expected[1] = ".param wn=2 ln=0.15";
  CHECK(deck == expected);  // the .param inside .subckt inv is not changed
}

TEST_CASE("Parameter defined twice", "[cap:simulation-sweeps][req:Point decks]") {
  sim::PointChanges c;
  c.job_id = "rs-p000000";
  c.params = {{"vdd", 1.62}};
  std::vector<diag::Diagnostic> warnings;
  auto deck = sim::point_deck({"* t", "R1 a 0 1", ".param vdd=1.8", "V1 a 0 1", "R2 a 0 1", "R3 a 0 1", "R4 a 0 1",
                               "R5 a 0 1", ".param vdd=1.62", ".end"},
                              c, warnings);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().text == "Parameter vdd is defined more than once at top level");
}

TEST_CASE("Point decks insert missing definitions and change temperature, supply and sources",
          "[cap:simulation-sweeps][req:Point decks]") {
  sim::PointChanges c;
  c.job_id = "rs-p000003";
  c.params = {{"cl", 1e-15}};
  c.temperature = 125;
  c.supply = 0.9;
  c.supply_sources = {"Vdd"};
  c.sources = {{"vin", 0.45}};
  std::vector<diag::Diagnostic> warnings;
  auto deck = sim::point_deck({"* t", ".options reltol=1e-4 temp=27", "Vdd vdd 0 DC 1.8", "Vin in 0 0.9",
                               ".control", "set temp=1", ".endc", ".end"},
                              c, warnings);
  REQUIRE(deck.ok());
  CHECK(deck.value() == std::vector<std::string>{"* t [rs-p000003]", ".param cl=1e-15", ".options reltol=1e-4 temp=125",
                                                  "Vdd vdd 0 DC 1.62", "Vin in 0 0.45", ".control", "set temp=1",
                                                  ".endc", ".end"});
  // Without a .temp card or temp= option, `.temp <value>` follows the title.
  sim::PointChanges t;
  t.job_id = "x";
  t.temperature = -40;
  auto inserted = sim::point_deck({"* t", "R1 a 0 1", ".end"}, t, warnings);
  CHECK(inserted.value() == std::vector<std::string>{"* t [x]", ".temp -40", "R1 a 0 1", ".end"});
  // A source: level needs a top-level V or I line with a DC value.
  sim::PointChanges s;
  s.job_id = "x";
  s.sources = {{"vclk", 1}};
  CHECK(sim::point_deck({"* t", "Vclk c 0 pulse(0 1 0 1n 1n 5n 10n)", ".end"}, s, warnings).error().code ==
        "SIM-RUNSET-004");
}

TEST_CASE("Points with equal level values get decks that differ only in the title",
          "[cap:simulation-sweeps][req:Point decks]") {
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {3, 3})});
  std::vector<std::string> a = deck_of(rs, 0);
  std::vector<std::string> b = deck_of(rs, 1);
  CHECK(a[0] != b[0]);
  a.erase(a.begin());
  b.erase(b.begin());
  CHECK(a == b);
}

// --- Design of experiments ---

TEST_CASE("One-at-a-time rows", "[cap:simulation-sweeps][req:Design of experiments]") {
  sim::DoeBlock block{sim::DoeBlock::Method::oat, {{"param:wn", 1, 3, false, 2}, {"temp", -40, 120, false, 2}}, 0, 1};
  auto rows = sim::doe_rows(block, 100000);
  REQUIRE(rows.ok());
  CHECK(rows.value() == std::vector<std::vector<double>>{{2, 40}, {1, 40}, {3, 40}, {2, -40}, {2, 120}});
}

TEST_CASE("Latin hypercube strata", "[cap:simulation-sweeps][req:Design of experiments]") {
  sim::DoeBlock block{sim::DoeBlock::Method::lhs, {{"param:a", 0, 1, false, 2}, {"param:b", 0, 1, false, 2}}, 10, 7};
  auto first = sim::doe_rows(block, 100000);
  auto second = sim::doe_rows(block, 100000);
  REQUIRE(first.ok());
  CHECK(first.value() == second.value());  // integer arithmetic and one draw per swap: identical on every platform
  for (std::size_t j = 0; j < 2; ++j) {
    std::set<int> strata;
    for (const auto& row : first.value()) {
      strata.insert(static_cast<int>(row[j] * 10));
    }
    CHECK(strata.size() == 10);  // one row in each [k/10, (k+1)/10)
  }
}

TEST_CASE("Full factorial, random, log factors and the DOE seed", "[cap:simulation-sweeps][req:Design of experiments]") {
  sim::DoeBlock full{sim::DoeBlock::Method::full, {{"param:a", 0, 1, false, 2}, {"param:b", 1, 100, true, 3}}, 0, 1};
  CHECK(sim::doe_rows(full, 100).value() ==
        std::vector<std::vector<double>>{{0, 1}, {0, 10}, {0, 100}, {1, 1}, {1, 10}, {1, 100}});
  sim::DoeBlock random{sim::DoeBlock::Method::random, {{"param:a", 0, 1, false, 2}}, 3, 1};
  const auto r = sim::doe_rows(random, 100).value();
  CHECK(r[0][0] == sim::random_unit(sim::random_value(1, 0)));
  CHECK(r[2][0] == sim::random_unit(sim::random_value(1, 2)));
  random.seed = 2;
  CHECK(sim::doe_rows(random, 100).value() != r);
  sim::DoeBlock bad{sim::DoeBlock::Method::full, {{"param:a", 0, 0, false, 2}}, 0, 1};
  CHECK(sim::doe_rows(bad, 100).error().code == "SIM-RUNSET-006");
  sim::DoeBlock big{sim::DoeBlock::Method::full, {{"param:a", 0, 1, false, 1000}, {"param:b", 0, 1, false, 1000}}, 0, 1};
  CHECK(sim::doe_rows(big, 100000).error().text == "Run set has 1000000 points; the limit is 100000");
}

// --- Run set execution ---

TEST_CASE("Same results with one or four workers", "[cap:simulation-sweeps][req:Run set execution]") {
  // Bit-identical stored vectors with 1 and min(4, physical_cores - 1) workers.
  std::vector<std::vector<std::vector<sim::Vector>>> runs;
  for (const std::uint32_t workers : {1U, parallel_workers()}) {
    Host host;
    sim::JobManager jobs = manager(host, workers);
    REQUIRE(jobs.workers() == workers);
    sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2, 3, 4}), list_dim("temp", {-40, 27, 125})});
    sim::RunSetRunner runner(rs, jobs);
    REQUIRE(runner.start().ok());
    run_to_end(jobs, runner);
    CHECK(runner.state() == sim::RunSetState::completed);
    CHECK(runner.status_line() == "completed 12/12 1.0");
    CHECK(jobs.peak_active() <= workers);
    runs.push_back(vectors_by_point(rs, jobs));
  }
  CHECK(same_vectors(runs[0], runs[1]));
}

TEST_CASE("A cancelled run set resumes exactly the points without a result and equals an uninterrupted run",
          "[cap:simulation-sweeps][req:Run set execution]") {
  Host host;
  sim::JobManager jobs = manager(host, 1);
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2, 3, 4, 5, 6, 7, 8})});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  while (runner.done() < 3) {
    jobs.poll_for(std::chrono::milliseconds(2));
    runner.update();
  }
  REQUIRE(runner.cancel().ok());
  CHECK(runner.state() == sim::RunSetState::cancelled);
  std::vector<sim::JobId> completed;
  for (const sim::PointRecord& r : rs.records) {
    if (r.completed) {
      completed.push_back(r.job);
    }
  }
  REQUIRE(completed.size() >= 3);
  const std::uint64_t engines_before = host.engines;
  REQUIRE(runner.resume().ok());
  run_to_end(jobs, runner);
  CHECK(runner.state() == sim::RunSetState::completed);
  CHECK(host.engines == engines_before);  // the healthy worker is reused

  Host fresh_host;
  sim::JobManager fresh = manager(fresh_host, 1);
  sim::RunSet again = sweep_run_set({list_dim("param:wn", {1, 2, 3, 4, 5, 6, 7, 8})});
  sim::RunSetRunner uninterrupted(again, fresh);
  REQUIRE(uninterrupted.start().ok());
  run_to_end(fresh, uninterrupted);
  CHECK(same_vectors(vectors_by_point(rs, jobs), vectors_by_point(again, fresh)));
}

TEST_CASE("Pausing a run set stops new points from starting", "[cap:simulation-sweeps][req:Run set execution]") {
  Host host;
  sim::JobManager jobs = manager(host, 1);
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2, 3, 4})});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  REQUIRE(runner.pause().ok());
  run_to_end(jobs, runner);
  CHECK(runner.state() == sim::RunSetState::paused);
  CHECK(runner.done() == 1);  // the point submitted before the pause
  REQUIRE(runner.resume().ok());
  run_to_end(jobs, runner);
  CHECK(runner.state() == sim::RunSetState::completed);
  CHECK(runner.done() == 4);
}

TEST_CASE("A base deck whose points cannot be made fails the run set before any job",
          "[cap:simulation-sweeps][req:Run set execution]") {
  Host host;
  sim::JobManager jobs = manager(host, 1);
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2})});
  rs.context.base.lines.insert(rs.context.base.lines.begin() + 2, ".param wn=7");
  sim::RunSetRunner runner(rs, jobs);
  const auto status = runner.start();
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == "SIM-RUNSET-005");
  CHECK(runner.state() == sim::RunSetState::failed);
  CHECK(host.engines == 0);
}

TEST_CASE("execute submits every point without a completed result in index order",
          "[cap:simulation-sweeps][req:Run set execution]") {
  Host host;
  sim::JobManager jobs = manager(host, 2);
  sim::RunSet rs = sweep_run_set({list_dim("param:wn", {1, 2, 3})});
  REQUIRE(sim::execute(rs, jobs).ok());
  for (const sim::JobId& id : rs.jobs) {
    CHECK(jobs.state(id) == sim::JobState::queued);
  }
}

// --- Run set storage ---

TEST_CASE("Large run set keeps only failing waveforms", "[cap:simulation-sweeps][req:Run set storage]") {
  CHECK(sim::default_retention(100) == sim::Retention::all);
  CHECK(sim::default_retention(500) == sim::Retention::failing);
  std::vector<std::size_t> kept;
  for (std::size_t i = 0; i < 500; ++i) {
    sim::RunPoint p;
    p.index = i;
    if (sim::keeps_raw(sim::default_retention(500), p, false, i == 12 || i == 310)) {
      kept.push_back(i);
    }
  }
  CHECK(kept == std::vector<std::size_t>{0, 12, 310});
  sim::RunPoint nominal;
  nominal.index = 7;
  nominal.nominal = true;
  CHECK(sim::keeps_raw(sim::Retention::none, nominal, false, false));
}

TEST_CASE("runset.json, base.spice and points.csv are written in point order",
          "[cap:simulation-sweeps][req:Run set storage]") {
  Host host;
  sim::JobManager jobs = manager(host, 2);
  sim::RunSet rs = sweep_run_set({list_dim("temp", {-40, 125}), list_dim("param:vdd", {1.62, 1.8, 1.98})});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  run_to_end(jobs, runner);
  const auto rows = sim::point_results(rs, jobs, [](const sim::JobRecord& r) {
    return std::vector<std::optional<double>>{r.vectors.at(1).real.at(0)};
  });
  platform::MemoryFileSystem fs;
  const platform::Path folder{"/p/simulations/tb/20261127-120000-tb-rs"};
  sim::RunSetInfo info{"completed", sim::Retention::all, "sky130A", "1", std::nullopt, ""};
  REQUIRE(sim::store_run_set(fs, folder, rs, info, {"tpd"}, rows).ok());
  const std::string csv = fs.read(folder / "points.csv").value();
  CHECK(csv.starts_with("index,job,state,temp,param:vdd,tpd\n0,20261127-120000-tb-rs-p000000,completed,-40,1.62,"));
  CHECK(std::count(csv.begin(), csv.end(), '\n') == 7);
  const std::string json = fs.read(folder / "runset.json").value();
  for (const char* key : {"\"id\"", "\"testbench\"", "\"state\"", "\"dimensions\"", "\"total\": 6", "\"keep\": \"all\"",
                          "\"deckSha256\": \"base-sha\"", "\"descriptorVersion\""}) {
    CHECK(json.find(key) != std::string::npos);
  }
  CHECK(fs.read(folder / "base.spice").value().starts_with("* inverter tb\n.param wn=1 ln=0.15\n"));
}

// --- Sweep result views ---

TEST_CASE("Measurement versus temperature", "[cap:simulation-sweeps][req:Sweep result views]") {
  sim::RunSet rs = sweep_run_set({list_dim("temp", {-40, 0, 27, 85, 125}), list_dim("supply", {0.9, 1, 1.1})});
  std::vector<std::optional<double>> tpd(rs.points.size());
  for (std::size_t i = 0; i < tpd.size(); ++i) {
    tpd[i] = static_cast<double>(i);
  }
  const auto curves = sim::measurement_curves(rs, 0, tpd);
  REQUIRE(curves.size() == 3);
  CHECK(curves[0].label == "supply=0.9");
  CHECK(curves[1].label == "supply=1");
  CHECK(curves[2].label == "supply=1.1");
  CHECK(curves[0].x == std::vector<double>{-40, 0, 27, 85, 125});
  CHECK(curves[1].y.size() == 5);
  CHECK(sim::family_legend("v(out)", rs, 4) == "v(out) [temp=0, supply=1]");
  CHECK_FALSE(sim::overlay_legend("v(out)", 200).has_value());
  CHECK(sim::overlay_legend("v(out)", 201) == std::optional<std::string>{"v(out) (201 points)"});
}

// --- Sweep command options (library API; the `sim` command is task 14.1) ---

TEST_CASE("Nested sweep from a script", "[cap:simulation-sweeps][req:Sweep command options]") {
  // sim sweep add temp list -40 27 125; sim sweep add supply lin 0.9 1.1 -points 3; sim runset run -wait
  auto temp = sim::sweep_dimension({"temp", sim::SweepDefinition::Mode::list, 0, 0, 0, 0, {-40, 27, 125}});
  auto supply = sim::sweep_dimension({"supply", sim::SweepDefinition::Mode::lin_points, 0.9, 1.1, 0, 3, {}});
  REQUIRE(temp.ok());
  REQUIRE(supply.ok());
  Host host;
  sim::JobManager jobs = manager(host, 2);
  sim::RunSet rs = sweep_run_set({temp.value(), supply.value()});
  sim::RunSetRunner runner(rs, jobs);
  REQUIRE(runner.start().ok());
  run_to_end(jobs, runner);
  CHECK(runner.status_line() == "completed 9/9 1.0");
}

TEST_CASE("Unknown factor syntax", "[cap:simulation-sweeps][req:Sweep command options]") {
  const auto bad = sim::parse_doe_factor("param:wn:1");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text == "Usage: sim doe <full|oat|lhs|random> [-points <n>] [-seed <s>] <factor…>");
  const auto good = sim::parse_doe_factor("param:wn:1:3:log:5");
  REQUIRE(good.ok());
  CHECK(good.value().variable == "param:wn");
  CHECK(good.value().log);
  CHECK(good.value().levels == 5);
}

// --- Measured waveform retention ---

TEST_CASE("Only measured vectors kept", "[cap:simulation-sweeps][req:Measured waveform retention]") {
  std::vector<sim::Vector> vectors{{"time", "time", false, {0, 1}, {}}};
  for (int i = 0; i < 40; ++i) {
    vectors.push_back(sim::Vector{"v(n" + std::to_string(i) + ")", "voltage", false, {1, 2}, {}});
  }
  vectors.push_back(sim::Vector{"v(out)", "voltage", false, {3, 4}, {}});
  const auto kept = sim::retained_vectors(sim::Retention::measured, vectors, {"v(out)"});
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].name == "time");
  CHECK(kept[1].name == "v(out)");
  CHECK(kept[1].real == std::vector<double>{3, 4});  // full resolution
  CHECK(sim::retained_vectors(sim::Retention::all, vectors, {"v(out)"}).size() == vectors.size());
}

// --- Results in point order ---

TEST_CASE("Out-of-order completion", "[cap:simulation-sweeps][req:Results in point order]") {
  std::vector<std::string> csvs;
  for (const std::uint32_t workers : {1U, parallel_workers()}) {
    Host host;
    host.slow_jobs.insert(sim::point_job_id("20261127-120000-tb-rs", 7));
    sim::JobManager jobs = manager(host, workers);
    std::vector<double> values(100);
    for (std::size_t i = 0; i < values.size(); ++i) {
      values[i] = 1.0 + static_cast<double>(i) / 100.0;
    }
    sim::RunSet rs = sweep_run_set({list_dim("param:wn", values)});
    sim::RunSetRunner runner(rs, jobs);
    REQUIRE(runner.start().ok());
    run_to_end(jobs, runner);
    const auto rows = sim::point_results(rs, jobs, [](const sim::JobRecord& r) {
      return std::vector<std::optional<double>>{r.vectors.at(1).real.at(0)};
    });
    for (std::size_t i = 0; i < rows.size(); ++i) {
      CHECK(rows[i].index == i);
    }
    csvs.push_back(sim::points_csv(rs, {"m"}, rows));
  }
  CHECK(csvs[0] == csvs[1]);
}
