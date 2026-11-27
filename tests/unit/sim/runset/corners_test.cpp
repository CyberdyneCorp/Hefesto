// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// simulation-corners (M9 R3, task 15.2): bundled tables, user corners and
// sets, the corner dimension, pass and fail, the matrix, run all and re-run
// failed, the corner checks of `sim corners` (library API; the command is
// task 14.1) and corner jobs on the worker pool.
//
// Owner: SIMADV.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "hefesto/sim/runset/corners.hpp"
#include "hefesto/sim/runset/point_deck.hpp"
#include "hefesto/sim/runset/runset.hpp"
#include "hefesto/sim/runset/verdict.hpp"
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

std::vector<std::string> corner_deck(const sim::CornerTable& t, const sim::CornerLevel& level,
                                     const std::vector<std::string>& base) {
  sim::PointChanges c;
  c.job_id = "rs-p000000";
  c.variables = t.variables;
  auto applied = sim::corner_application(t, level);
  REQUIRE(applied.ok());
  c.corner = applied.value();
  c.supply_sources = {"Vdd"};
  std::vector<diag::Diagnostic> warnings;
  auto deck = sim::point_deck(base, c, warnings);
  REQUIRE(deck.ok());
  return deck.value();
}

bool contains(const std::vector<std::string>& lines, const std::string& line) {
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}

bool mentions(const std::vector<std::string>& lines, const std::string& text) {
  return std::any_of(lines.begin(), lines.end(), [&text](const std::string& l) { return l.find(text) != std::string::npos; });
}

sim::CornerPoint point(std::size_t corner, double tpd) {
  return sim::CornerPoint{corner, true, true, {{"tpd", tpd}}};
}

}  // namespace

// --- Corner definitions in the PDK descriptor ---

TEST_CASE("Missing section rejected", "[cap:simulation-corners][req:Corner definitions in the PDK descriptor]") {
  sim::CornerTable t = table("sky130A");
  t.variables[0].words.emplace_back("sf_lh");  // a user descriptor naming a section the library lacks
  t.corners.push_back(sim::CornerDefinition{"sf_lh", {{"proc", "sf_lh"}}, {}, sim::CornerKind::process, {}, {}, {}});
  const std::string library = ".lib tt\n.include tt.spice\n.endl tt\n.lib sf\n.endl sf\n";
  std::vector<std::string> sections = sim::library_sections(library);
  CHECK(sections == std::vector<std::string>{"tt", "sf"});
  // Every bundled word is present in the real file; here only sf_lh is missing.
  for (const sim::SectionVariable& v : t.variables) {
    for (const std::string& w : v.words) {
      if (w != "sf_lh") {
        sections.push_back(w);
      }
    }
  }
  const auto status = sim::check_corner_sections(t, {{"sky130.lib.spice", sections}});
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().text == "Corner sf_lh: section sf_lh not found in sky130.lib.spice");
}

TEST_CASE("Corner kinds: only process corners are in sets and the matrix",
          "[cap:simulation-corners][req:Corner definitions in the PDK descriptor]") {
  const sim::CornerTable t = table("sky130B");
  CHECK(t.corner("tt_mm")->kind == sim::CornerKind::mismatch);
  CHECK(t.corner("mc")->kind == sim::CornerKind::statistical);
  sim::CornerTable bad = t;
  bad.sets.push_back(sim::CornerSetDefinition{"mm", {"tt_mm"}, {27}, {1}, {}});
  CHECK(sim::expand_corner_set(bad, "mm", 1.8).error().code == "SIM-RUNSET-007");
}

// --- Bundled corner tables ---

TEST_CASE("sky130 R/C corner", "[cap:simulation-corners][req:Bundled corner tables]") {
  const sim::CornerTable t = table("sky130A");
  const sim::CornerLevel level{sim::corner_name("ss_hh", 125, 1.8 * 0.9), "ss_hh", 125, 0.9};
  CHECK(level.name == "ss_hh_125_1v62");
  const auto deck = corner_deck(t, level,
                                {"* tb", ".lib \"/store/sky130A/libs.tech/combined/sky130.lib.spice\" tt",
                                 "Vdd vdd 0 1.8", ".end"});
  CHECK(contains(deck, ".lib \"/store/sky130A/libs.tech/combined/sky130.lib.spice\" ss_hh"));
  CHECK(contains(deck, ".temp 125"));
  CHECK(contains(deck, "Vdd vdd 0 1.62"));
  CHECK(t.corner("sf_lh") == nullptr);  // there is no sf_lh section
}

TEST_CASE("sg13g2 slow corner", "[cap:simulation-corners][req:Bundled corner tables]") {
  const sim::CornerTable t = table("sg13g2");
  const auto deck = corner_deck(t, sim::CornerLevel{"ss_27_1v2", "ss", 27, 1.0},
                                {"* tb", ".lib /pdk/models/cornerMOSlv.lib mos_tt", ".lib /pdk/models/cornerRES.lib res_typ",
                                 "XM1 d g 0 0 sg13_lv_nmos w=1u l=0.13u", ".end"});
  CHECK(contains(deck, ".lib /pdk/models/cornerMOSlv.lib mos_ss"));
  CHECK(contains(deck, ".lib /pdk/models/cornerRES.lib res_typ"));
  CHECK_FALSE(mentions(deck, "cornerHBT.lib"));
  CHECK(t.variable("mos")->always_used);
  CHECK_FALSE(t.variable("hbt")->always_used);
}

TEST_CASE("gf180mcu corners map the optional variables", "[cap:simulation-corners][req:Bundled corner tables]") {
  const sim::CornerTable t = table("gf180mcuD");
  const auto words = [&t](const char* corner) { return t.words_of(*t.corner(corner)); };
  CHECK(words("ff")[1] == std::pair<std::string, std::string>{"res", "res_ff"});
  CHECK(words("fs")[1] == std::pair<std::string, std::string>{"res", "res_typical"});
  CHECK(words("typical")[2] == std::pair<std::string, std::string>{"cap", "mimcap_typical"});
  CHECK(t.set("pvt")->temperatures == std::vector<double>{-40, 25, 125});
  CHECK(sim::bundled_corner_table("tsmc28").error().code == "SIM-RUNSET-007");
}

// --- User-defined corners and corner sets ---

TEST_CASE("Explicit tuple list", "[cap:simulation-corners][req:User-defined corners and corner sets]") {
  sim::UserCorners user;
  user.sets.push_back(sim::CornerSetDefinition{"signoff", {}, {}, {}, {{"ss", 125, 0.9}, {"ff", -40, 1.1}, {"tt", 27, 1.0}}});
  std::vector<diag::Diagnostic> warnings;
  auto merged = sim::merge_user_corners(table("sky130A"), user, warnings);
  REQUIRE(merged.ok());
  auto levels = sim::expand_corner_set(merged.value(), "signoff", 1.8);
  REQUIRE(levels.ok());
  std::vector<std::string> names;
  for (const auto& l : levels.value()) {
    names.push_back(l.name);
  }
  CHECK(names == std::vector<std::string>{"ss_125_1v62", "ff_m40_1v98", "tt_27_1v8"});
  CHECK(warnings.empty());
}

TEST_CASE("A user corner shadows the PDK corner and duplicate tuples are refused",
          "[cap:simulation-corners][req:User-defined corners and corner sets]") {
  sim::UserCorners user;
  user.corners.push_back(sim::CornerDefinition{"ss", {{"proc", "ss_hh"}}, {}, sim::CornerKind::process, 100.0, {}, {}});
  user.sets.push_back(sim::CornerSetDefinition{"dup", {}, {}, {}, {{"tt", 27, 1.0}, {"tt", 27.2, 1.0}}});
  std::vector<diag::Diagnostic> warnings;
  auto merged = sim::merge_user_corners(table("sky130A"), user, warnings);
  REQUIRE(merged.ok());
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].text == "Testbench corner ss overrides the PDK corner");
  CHECK(sim::expand_corner_set(merged.value(), "dup", 1.8).error().text == "Duplicate corner tt_27_1v8 in corner set dup");
  // The user corner fixes its temperature.
  sim::CornerTable t = merged.value();
  t.sets.push_back(sim::CornerSetDefinition{"one", {}, {}, {}, {{"ss", -40, 1.0}}});
  CHECK(sim::expand_corner_set(t, "one", 1.8).value()[0].name == "ss_100_1v8");
}

// --- Corner dimension ---

TEST_CASE("Corner sweep order", "[cap:simulation-corners][req:Corner dimension]") {
  auto levels = sim::expand_corner_set(table("sky130A"), "pvt", 1.8);
  REQUIRE(levels.ok());
  REQUIRE(levels.value().size() == 45);
  CHECK(levels.value()[0].name == "tt_m40_1v62");
  CHECK(levels.value()[1].name == "tt_m40_1v8");
  const sim::Dimension d = sim::corner_dimension(levels.value());
  CHECK(d.name == "corner");
  CHECK(d.label(44) == "fs_125_1v98");
}

TEST_CASE("Corner names", "[cap:simulation-corners][req:Corner dimension]") {
  CHECK(sim::corner_name("tt", 27, 1.8) == "tt_27_1v8");
  CHECK(sim::corner_name("ss", -40, 1.8 * 0.9) == "ss_m40_1v62");
  CHECK(sim::corner_name("tt", -40, 1.62) == "tt_m40_1v62");
}

TEST_CASE("Monte Carlo at every corner: the corner dimension outside the Monte Carlo dimension",
          "[cap:simulation-corners][req:Corner dimension]") {
  sim::RunSetSpec spec;
  auto mc = sim::mc_dimension(sim::MonteCarloSpec{sim::McMode::mismatch, 3, 1, true, {}, false});
  REQUIRE(mc.ok());
  spec.dimensions = {sim::corner_dimension(sim::expand_corner_set(table("sky130A"), "pvt", 1.8).value()), mc.value()};
  auto points = sim::RunSet::generate(spec);
  REQUIRE(points.ok());
  CHECK(points.value().size() == 45 * 4);
  CHECK(points.value()[4].corner == "tt_m40_1v8");
  CHECK(points.value()[4].nominal);
}

// --- Pass and fail per corner ---

TEST_CASE("Corner-specific limit", "[cap:simulation-corners][req:Pass and fail per corner]") {
  sim::CornerSpec spec{"tpd", {sim::Limit::Kind::max, 120e-12, 0, {}}, {}, {{"ss_*", {sim::Limit::Kind::max, 180e-12, 0, {}}}}};
  const auto m = sim::corner_matrix({"ss_125_1v62", "tt_27_1v8"}, {point(0, 150e-12), point(1, 130e-12)}, {spec}, {"tpd"});
  CHECK(m.rows[0].verdict == sim::Verdict::pass);
  CHECK(m.rows[1].verdict == sim::Verdict::fail);
  CHECK(m.verdict == sim::Verdict::fail);
}

TEST_CASE("Corner verdicts: error, filters, no value and several points per corner",
          "[cap:simulation-corners][req:Pass and fail per corner]") {
  sim::CornerSpec spec{"tpd", {sim::Limit::Kind::max, 120e-12, 0, {}}, {"tt_*"}, {}};
  sim::CornerPoint failed{1, false, true, {}};
  sim::CornerPoint missing{0, true, true, {{"tpd", std::nullopt}}};
  const auto m = sim::corner_matrix({"tt_27_1v8", "ss_125_1v62", "ff_m40_1v98"},
                                    {missing, failed, point(2, 999e-12), point(2, 1e-12)}, {spec}, {"tpd"});
  CHECK(m.rows[0].verdict == sim::Verdict::fail);   // no value counts as a failure
  CHECK(m.rows[1].verdict == sim::Verdict::error);  // the point failed
  CHECK(m.rows[2].verdict == sim::Verdict::pass);   // the filter excludes ff: nothing applies
  CHECK(m.rows[2].cells[0].value == 999e-12);       // the worst of the corner's points
  CHECK(sim::glob_match("ss_?25_*", "ss_125_1v62"));
  CHECK_FALSE(sim::glob_match("ss_*", "tt_27_1v8"));
}

TEST_CASE("Specification limits and margins", "[cap:simulation-corners][req:Pass and fail per corner]") {
  const sim::Limit range{sim::Limit::Kind::range, 1.1, 1.3, {}};
  CHECK(sim::check_limit(range, 1.25) == sim::SpecResult::pass);
  CHECK(std::abs(*sim::margin(range, 1.25) - 0.5) < 1e-12);
  const sim::Limit at_zero{sim::Limit::Kind::min, 0, 0, {}};
  CHECK_FALSE(sim::margin(at_zero, 1.0).has_value());
  CHECK(sim::check_limit(at_zero, std::nullopt) == sim::SpecResult::no_value);
}

// --- Corner matrix ---

TEST_CASE("Worst corner in the summary", "[cap:simulation-corners][req:Corner matrix]") {
  const std::vector<std::string> corners{"tt_27_1v8", "ss_125_1v62", "ff_m40_1v98", "sf_27_1v8", "fs_27_1v8"};
  const std::vector<double> tpd{80e-12, 131e-12, 55e-12, 97e-12, 102e-12};
  std::vector<sim::CornerPoint> points;
  for (std::size_t i = 0; i < tpd.size(); ++i) {
    points.push_back(point(i, tpd[i]));
  }
  const auto m = sim::corner_matrix(corners, points, {}, {"tpd"});
  REQUIRE(m.summary.size() == 1);
  CHECK(sim::format_si(*m.summary[0].worst) == "131p");
  CHECK(m.summary[0].corner == "ss_125_1v62");
  const auto lists = sim::matrix_lists(m);
  CHECK(lists[1] == std::vector<std::string>{"ss_125_1v62", "pass", "131p"});
}

TEST_CASE("Matrix marks: marginal, fail, no value and pending, each with a symbol",
          "[cap:simulation-corners][req:Corner matrix]") {
  sim::CornerSpec spec{"tpd", {sim::Limit::Kind::max, 100e-12, 0, {}}, {}, {}};
  sim::CornerPoint pending{3, false, false, {}};
  const auto m = sim::corner_matrix({"a", "b", "c", "d"}, {point(0, 95e-12), point(1, 120e-12),
                                                           sim::CornerPoint{2, true, true, {}}, pending},
                                    {spec}, {"tpd", "power"});
  CHECK(m.columns == std::vector<std::string>{"tpd", "power"});
  CHECK(m.rows[0].cells[0].mark == sim::CellMark::marginal);  // margin 0.05 < 0.1
  CHECK(m.rows[1].cells[0].mark == sim::CellMark::fail);
  CHECK(m.rows[2].cells[0].mark == sim::CellMark::no_value);
  CHECK(m.rows[3].verdict == sim::Verdict::pending);
  CHECK(m.rows[3].cells[0].mark == sim::CellMark::pending);
  std::set<std::string_view> symbols;
  for (const auto mark : {sim::CellMark::pass, sim::CellMark::fail, sim::CellMark::marginal, sim::CellMark::no_value,
                          sim::CellMark::pending}) {
    symbols.insert(sim::mark_symbol(mark));
  }
  CHECK(symbols.size() == 5);
  CHECK(sim::format_si(1.8) == "1.8");
  CHECK(sim::format_si(2500) == "2.5k");
}

// --- Run all and re-run failed corners ---

TEST_CASE("Re-run failed after a fix", "[cap:simulation-corners][req:Run all and re-run failed corners]") {
  sim::CornerSpec spec{"tpd", {sim::Limit::Kind::max, 120e-12, 0, {}}, {}, {}};
  auto levels = sim::expand_corner_set(table("sky130A"), "pvt", 1.8);
  REQUIRE(levels.ok());
  std::vector<std::string> names;
  std::vector<sim::CornerPoint> points;
  for (std::size_t i = 0; i < levels.value().size(); ++i) {
    names.push_back(levels.value()[i].name);
    const bool slow = names.back() == "ss_125_1v62" || names.back() == "sf_125_1v62";
    points.push_back(point(i, slow ? 150e-12 : 90e-12));
  }
  const auto matrix = sim::corner_matrix(names, points, {spec}, {"tpd"});
  const std::vector<std::size_t> failed = sim::failed_corners(matrix);
  REQUIRE(failed.size() == 2);
  std::vector<sim::CornerLevel> rerun;
  for (const std::size_t i : failed) {
    rerun.push_back(levels.value()[i]);
  }
  sim::RunSet rs;
  rs.spec.id = "20261127-130000-tb-rs";
  rs.spec.dimensions = {sim::corner_dimension(rerun)};
  rs.context.testbench = "tb";
  rs.context.base = base_deck();  // "regenerate": a freshly prepared base deck
  rs.context.corners = table("sky130A");
  REQUIRE(rs.prepare().ok());
  CHECK(rs.points.size() == 2);
  const std::string json = sim::runset_json(rs, sim::RunSetInfo{"queued", sim::Retention::all, "sky130A", "1",
                                                                std::nullopt, "20261127-120000-tb-rs"});
  CHECK(json.find("\"rerunOf\": \"20261127-120000-tb-rs\"") != std::string::npos);
}

// --- Corner command options (library API; `sim corners` is task 14.1) ---

TEST_CASE("Define a corner from a script", "[cap:simulation-corners][req:Corner command options]") {
  const sim::CornerDefinition slowcold{"slowcold", {{"mos", "mos_ss"}}, {}, sim::CornerKind::process, -40.0, {}, {}};
  CHECK(sim::check_corner_words(table("sg13g2"), slowcold).ok());
  std::vector<diag::Diagnostic> warnings;
  auto merged = sim::merge_user_corners(table("sg13g2"), sim::UserCorners{{slowcold}, {}}, warnings);
  REQUIRE(merged.ok());
  REQUIRE(merged.value().corner("slowcold") != nullptr);
  CHECK(merged.value().words_of(*merged.value().corner("slowcold"))[0].second == "mos_ss");
}

TEST_CASE("Unknown section", "[cap:simulation-corners][req:Corner command options]") {
  const sim::CornerDefinition x{"x", {{"proc", "zz"}}, {}, sim::CornerKind::process, {}, {}, {}};
  const auto status = sim::check_corner_words(table("sky130A"), x);
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().text == "sim corners: proc has no section zz");
}

// --- Corner jobs on the worker pool ---

TEST_CASE("Large post-layout deck limits the pool", "[cap:simulation-corners][req:Corner jobs on the worker pool]") {
  // 16 GB, 10 cores and a 4 GiB result budget: B_job above 4 GiB gives
  // exactly one worker; the matrix equals the one-worker run's.
  std::vector<sim::CornerMatrix> matrices;
  for (const bool limited : {true, false}) {
    Host host;
    sim::JobManagerOptions o;
    o.make_engine = [&host]() -> diag::Outcome<std::unique_ptr<sim::Engine>> {
      return std::unique_ptr<sim::Engine>(std::make_unique<Engine>(host));
    };
    o.pool = limited ? sim::PoolInputs{10, 16 * kGiB, 4 * kGiB + (kGiB / 4), std::nullopt}
                     : sim::PoolInputs{10, 64 * kGiB, kGiB, 1};
    sim::JobManager jobs(std::move(o));
    CHECK(jobs.workers() == 1);
    auto levels = sim::expand_corner_set(table("sky130A"), "pvt", 1.8);
    REQUIRE(levels.ok());
    levels.value().resize(9);  // nine corners
    sim::RunSet rs;
    rs.spec.id = "20261127-140000-tb-rs";
    rs.spec.dimensions = {sim::corner_dimension(levels.value())};
    rs.context.testbench = "tb";
    rs.context.base = base_deck();
    rs.context.base.lines.insert(rs.context.base.lines.begin() + 1, ".lib \"/s/sky130.lib.spice\" tt");
    rs.context.corners = table("sky130A");
    rs.context.supply_sources = {"Vdd"};
    REQUIRE(rs.prepare().ok());
    sim::RunSetRunner runner(rs, jobs);
    REQUIRE(runner.start().ok());
    run_to_end(jobs, runner);
    CHECK(jobs.peak_active() == 1);
    std::vector<std::string> names;
    std::vector<sim::CornerPoint> points;
    for (std::size_t i = 0; i < rs.points.size(); ++i) {
      names.push_back(rs.points[i].corner);
      const sim::JobRecord* rec = jobs.record(rs.records[i].job);
      REQUIRE(rec != nullptr);
      points.push_back(sim::CornerPoint{i, rec->job.state == sim::JobState::completed, true,
                                        {{"v0", rec->vectors.at(1).real.at(0)}}});
    }
    matrices.push_back(sim::corner_matrix(names, points, {}, {"v0"}));
  }
  CHECK(sim::matrix_lists(matrices[0]) == sim::matrix_lists(matrices[1]));
}
