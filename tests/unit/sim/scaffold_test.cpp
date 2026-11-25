// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M9 scaffold (openspec/changes/m9-lvs-simulation tasks.md 4.1, 4.2): the
// sim code area exists, EditorState carries a sim::Session, and every
// area-contract stub reports CORE-NOTIMPL-009 (sample_seed answers its
// documented nominal seed). The owners replace these checks with their
// requirement tests as the stubs go away.
//
// Owner: SCAFFOLD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/engine/pool.hpp"
#include "hefesto/sim/meas/meas.hpp"
#include "hefesto/sim/probe/overlay_feed.hpp"
#include "hefesto/sim/probe/probe.hpp"
#include "hefesto/sim/result/result_store.hpp"
#include "hefesto/sim/runset/runset.hpp"
#include "hefesto/sim/session.hpp"

namespace diag = hefesto::diag;
namespace sim = hefesto::sim;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-009";

template <typename T>
bool is_not_implemented(const diag::Outcome<T>& outcome) {
  return !outcome.ok() && outcome.error().code == kNotImplemented;
}

}  // namespace

TEST_CASE("the sim code area is declared", "[cap:hefesto-diagnostics][req:Structured diagnostic records]") {
  const auto area = diag::find_code_area("sim");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "SIM");
  CHECK(area->doc == "docs/diagnostics/codes/sim.md");
  CHECK(diag::codes::kM9NotImplemented == kNotImplemented);
}

TEST_CASE("EditorState carries an empty simulation session",
          "[cap:simulation-analyses-and-results][req:Result storage]") {
  const hefesto::editor::EditorState state;
  CHECK(state.sim.testbenches.empty());
  CHECK(state.sim.run_sets.empty());
  CHECK(state.sim.stored_jobs.empty());
  CHECK(state.sim.results.files == nullptr);
  CHECK(state.sim.engine == sim::EngineStatus::unknown);
  CHECK(state.sim.engine_reason.empty());
}

// make_worker_engine is implemented (SIMD, task 5.4): without a worker host
// simulation is unavailable (SIM-ENGINE-001), not "not implemented".
// tests/unit/sim/engine/worker_engine_test.cpp covers the engine itself.
TEST_CASE("make_worker_engine without a host reports SIM-ENGINE-001",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const sim::WorkerEngineOptions options;
  CHECK(options.program == "hefesto-simd");
  const auto engine = sim::make_worker_engine(nullptr, options);
  REQUIRE_FALSE(engine.ok());
  CHECK(engine.error().code == "SIM-ENGINE-001");
  CHECK_FALSE(is_not_implemented(engine));
}

TEST_CASE("scaffold job submission reports CORE-NOTIMPL-009",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  sim::JobManager jobs;
  CHECK(is_not_implemented(jobs.submit(sim::PreparedDeck{}, sim::JobOptions{"tb", "tt", {}})));
}

TEST_CASE("scaffold worker_pool_size reports CORE-NOTIMPL-009",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  const sim::PoolInputs inputs{8, std::uint64_t{16} << 30, std::uint64_t{1} << 30, std::nullopt};
  CHECK(is_not_implemented(sim::worker_pool_size(inputs)));
}

TEST_CASE("scaffold deck preparation reports CORE-NOTIMPL-009",
          "[cap:simulation-netlist-preparation][req:Circuit sources]") {
  sim::Testbench testbench;
  testbench.name = "tb";
  testbench.dut = sim::CircuitSource{sim::CircuitSource::Kind::spice, "inv.spice", sim::ParasiticMode::none};
  CHECK(is_not_implemented(sim::prepare(testbench, sim::DeckContext{})));
}

TEST_CASE("scaffold model descriptor reports CORE-NOTIMPL-009",
          "[cap:simulation-netlist-preparation][req:Simulation model descriptor in the PDK store]") {
  CHECK(is_not_implemented(sim::ModelDescriptor::load("sky130A")));
}

TEST_CASE("scaffold raw writer reports CORE-NOTIMPL-009", "[cap:simulation-analyses-and-results][req:Raw file format]") {
  const sim::ResultStore store;
  CHECK(is_not_implemented(store.write_raw("20261124-120000-tb-tt", sim::Plot{})));
}

TEST_CASE("scaffold raw import reports CORE-NOTIMPL-009", "[cap:simulation-analyses-and-results][req:Raw file import]") {
  const sim::ResultStore store;
  CHECK(is_not_implemented(store.read_raw("tb.raw")));
}

TEST_CASE("scaffold CSV export reports CORE-NOTIMPL-009", "[cap:simulation-analyses-and-results][req:CSV export]") {
  const sim::ResultStore store;
  CHECK(is_not_implemented(store.export_csv(sim::Plot{}, "tb.csv")));
}

TEST_CASE("scaffold waveform measurement reports CORE-NOTIMPL-009",
          "[cap:simulation-analyses-and-results][req:Waveform measurements]") {
  const sim::ResultStore store;
  CHECK(is_not_implemented(store.measure_waveform(sim::Plot{}, sim::WaveformMeasure{"max", "v(out)", {}, {}})));
}

TEST_CASE("scaffold node_name reports CORE-NOTIMPL-009", "[cap:irsim-interface][req:Hierarchical node names]") {
  CHECK(is_not_implemented(sim::node_name(sim::LayoutSelection{})));
}

TEST_CASE("scaffold resolve_probe reports CORE-NOTIMPL-009",
          "[cap:simulation-ui-and-crossprobe][req:Probing a layout net]") {
  const sim::NetNameMap map{{sim::NetNameEntry{"out", "out__c1", "xdut.out__c1", {}, {}}}};
  const auto on_node = [](const sim::LayoutPoint& /*point*/) { return true; };
  CHECK(is_not_implemented(sim::resolve_probe(map, sim::LayoutSelection{}, on_node)));
}

TEST_CASE("scaffold probe reports CORE-NOTIMPL-009", "[cap:simulation-ui-and-crossprobe][req:Probing device currents]") {
  sim::ProbeTarget target;
  target.kind = sim::ProbeTarget::Kind::device_current;
  target.name = "m1";
  CHECK(is_not_implemented(sim::probe(sim::Job{}, target)));
}

TEST_CASE("scaffold overlay feed reports CORE-NOTIMPL-009",
          "[cap:simulation-ui-and-crossprobe][req:Data feed to 3D overlays]") {
  const sim::OverlayFeed feed;
  CHECK(is_not_implemented(feed.voltages(sim::Job{}, 0.0)));
  CHECK(is_not_implemented(feed.currents(sim::Job{}, 0.0)));
}

TEST_CASE("scaffold run-set point generation reports CORE-NOTIMPL-009",
          "[cap:simulation-sweeps][req:Sweep point generation]") {
  sim::RunSetSpec spec;
  spec.sweeps.push_back(sim::SweepVariable{"w", {1.0, 2.0}});
  CHECK(is_not_implemented(sim::RunSet::generate(spec)));
}

TEST_CASE("scaffold run-set execution reports CORE-NOTIMPL-009", "[cap:simulation-sweeps][req:Run set execution]") {
  sim::RunSet run_set;
  sim::JobManager jobs;
  CHECK(is_not_implemented(sim::execute(run_set, jobs)));
}

TEST_CASE("scaffold sample_seed reports CORE-NOTIMPL-009", "[cap:simulation-monte-carlo][req:Sample seeds]") {
  // Regression: the stub answered 1 for every sample, so every Monte Carlo sample got the same seed.
  CHECK(is_not_implemented(sim::sample_seed(7, 0)));
  CHECK(is_not_implemented(sim::sample_seed(7, 3)));
}

TEST_CASE("scaffold measurement evaluation reports CORE-NOTIMPL-009",
          "[cap:simulation-measurements-and-statistics][req:Measurement definitions]") {
  CHECK(is_not_implemented(sim::evaluate_measurements(sim::ResultStore{}, sim::MeasSet{})));
}

TEST_CASE("scaffold statistics reports CORE-NOTIMPL-009",
          "[cap:simulation-measurements-and-statistics][req:Descriptive statistics]") {
  CHECK(is_not_implemented(sim::statistics(sim::MeasTable{}, sim::Specs{})));
}
