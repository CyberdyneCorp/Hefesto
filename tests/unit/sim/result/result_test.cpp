// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Results (task 13.3; design D12, D18): display names, the raw format
// (Q-SIM-17), raw import and its cap, CSV export, waveform measurements and
// progress (Q-SIM-14).
//
// Owner: SIMRES.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/result/progress.hpp"
#include "hefesto/sim/result/raw.hpp"
#include "hefesto/sim/result/result_store.hpp"

namespace platform = hefesto::platform;
namespace sim = hefesto::sim;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

sim::Vector real(std::string name, std::string type, std::vector<double> values) {
  return sim::Vector{std::move(name), std::move(type), false, std::move(values), {}};
}

sim::Plot dc_plot() {
  return sim::Plot{"dc1", {real("v(in)", "voltage", {0, 1}), real("out", "voltage", {0, 0.5})}};
}

sim::Plot ac_plot() {
  sim::Vector f{"frequency", "frequency", true, {1e3, 1e4}, {0, 0}};
  sim::Vector out{"out", "voltage", true, {-1, 0.5}, {0, 0.5}};
  return sim::Plot{"ac1", {f, out}};
}

sim::RawWriteOptions fixed(bool binary) {
  sim::RawWriteOptions o;
  o.binary = binary;
  o.title = "hefesto testbench tb corner tt";
  o.date = "Thu Nov  5 14:30:00 2026";
  o.command = "hefesto-0.0.1 ngspice-47-hefesto1";
  return o;
}

}  // namespace

TEST_CASE("Branch current naming", "[cap:simulation-analyses-and-results][req:Final vector retrieval]") {
  CHECK(sim::display_name(real("vdd#branch", "current", {})) == "i(vdd)");
  CHECK(sim::display_name(real("out", "voltage", {})) == "v(out)");
  CHECK(sim::display_name(real("v(out)", "voltage", {})) == "v(out)");
  CHECK(sim::display_name(real("time", "time", {})) == "time");
}

TEST_CASE("ASCII raw of a two-point op-like plot", "[cap:simulation-analyses-and-results][req:Raw file format]") {
  const auto text = sim::render_raw({dc_plot()}, fixed(false));
  REQUIRE(text.ok());
  CHECK(text.value() ==
        "Title: hefesto testbench tb corner tt\n"
        "Date: Thu Nov  5 14:30:00 2026\n"
        "Command: hefesto-0.0.1 ngspice-47-hefesto1\n"
        "Plotname: dc1\n"
        "Flags: real\n"
        "No. Variables: 2\n"
        "No. Points: 2\n"
        "Variables:\n"
        "\t0\tv(in)\tvoltage\n"
        "\t1\tv(out)\tvoltage\n"
        "Values:\n"
        " 0\t0.000000000000000e+00\n"
        "\t0.000000000000000e+00\n"
        "\n"
        " 1\t1.000000000000000e+00\n"
        "\t5.000000000000000e-01\n"
        "\n");
}

TEST_CASE("Binary raw data is little-endian and round-trips, complex plots too",
          "[cap:simulation-analyses-and-results][req:Raw file format]") {
  const auto text = sim::render_raw({dc_plot(), ac_plot()}, fixed(true));
  REQUIRE(text.ok());
  const std::size_t data = text.value().find("Binary:\n") + 8;
  // The second value of the first point is v(out)[0] = 0; the first is v(in)[0] = 0;
  // the second point's first value 1.0 = 0x3FF0000000000000, little-endian.
  const unsigned char one[8] = {0, 0, 0, 0, 0, 0, 0xF0, 0x3F};
  CHECK(std::memcmp(text.value().data() + data + 16, one, 8) == 0);
  const auto plots = sim::parse_raw(text.value(), "result.raw");
  REQUIRE(plots.ok());
  REQUIRE(plots.value().size() == 2);
  CHECK(plots.value()[0].name == "dc1");
  CHECK(plots.value()[0].vectors[1].name == "v(out)");
  CHECK(plots.value()[0].vectors[1].real == std::vector<double>{0, 0.5});
  CHECK(plots.value()[1].vectors[1].complex);
  CHECK(plots.value()[1].vectors[1].imag == std::vector<double>{0, 0.5});
  // ASCII round trip, header lines in another order and an unknown one.
  std::string ascii = sim::render_raw({ac_plot()}, fixed(false)).value();
  ascii.replace(ascii.find("Plotname"), 0, "Option: something\n");
  const auto again = sim::parse_raw(ascii, "a.raw");
  REQUIRE(again.ok());
  CHECK(again.value()[0].vectors[1].real == std::vector<double>{-1, 0.5});
}

TEST_CASE("A malformed raw file names the line and the expected token",
          "[cap:simulation-analyses-and-results][req:Raw file import]") {
  const std::string text =
      "Title: t\nDate: d\nPlotname: p\nFlags: real\nNo. Variables: 1\nOption: x\nVariables:\n\t0\ttime\ttime\nBinary:\n";
  const auto plots = sim::parse_raw(text, "result.raw");
  REQUIRE_FALSE(plots.ok());
  CHECK(plots.error().code == "SIM-RESULT-001");
  CHECK(plots.error().text == "result.raw:7: expected \"No. Points:\"");
}

TEST_CASE("Truncated binary", "[cap:simulation-analyses-and-results][req:Raw file import]") {
  sim::Plot plot{"tran1", {real("time", "time", std::vector<double>(100, 1.0))}};
  std::string text = sim::render_raw({plot}, fixed(true)).value();
  text.resize(text.size() - 40 * 8);
  const auto plots = sim::parse_raw(text, "result.raw");
  REQUIRE_FALSE(plots.ok());
  CHECK(plots.error().code == "SIM-RESULT-002");
  CHECK(plots.error().text == "result.raw: binary data ends after point 60 of 100");
}

TEST_CASE("Raw import stays within the result budget before allocating",
          "[cap:simulation-analyses-and-results][req:Raw file import]") {
  const std::string text =
      "Title: t\nPlotname: p\nFlags: real\nNo. Variables: 1000\nNo. Points: 99999999999\nVariables:\n";
  const auto plots = sim::parse_raw(text, "huge.raw");
  REQUIRE_FALSE(plots.ok());
  CHECK(plots.error().code == "SIM-RESULT-003");
  sim::Plot small{"p", {real("time", "time", {0, 1, 2, 3})}};
  const std::string ok = sim::render_raw({small}, fixed(true)).value();
  CHECK(sim::parse_raw(ok, "r.raw", 32).ok());
  CHECK(sim::parse_raw(ok, "r.raw", 31).error().code == "SIM-RESULT-003");
}

TEST_CASE("ResultStore writes result.raw per job and imports it",
          "[cap:simulation-analyses-and-results][req:Result storage]") {
  platform::MemoryFileSystem files;
  REQUIRE(files.create_directories("/proj/simulations/tb").ok());
  const sim::ResultStore store{&files, "/proj/simulations/tb"};
  REQUIRE(store.write_raw("20261105-143000-tb-tt", dc_plot()).ok());
  CHECK(files.exists("/proj/simulations/tb/20261105-143000-tb-tt/result.raw"));
  const auto plot = store.read_raw("/proj/simulations/tb/20261105-143000-tb-tt/result.raw");
  REQUIRE(plot.ok());
  CHECK(plot.value().vectors[1].real == std::vector<double>{0, 0.5});
  REQUIRE(store.export_csv(dc_plot(), "/proj/out.csv").ok());
  CHECK(files.read("/proj/out.csv").value() == "v(in),v(out)\n0,0\n1,0.5\n");
  CHECK(sim::ResultStore{}.write_raw("j", dc_plot()).error().code == "SIM-RESULT-009");
}

TEST_CASE("Transient CSV", "[cap:simulation-analyses-and-results][req:CSV export]") {
  const sim::Plot plot{"tran1", {real("time", "time", {0, 1e-9}), real("out", "voltage", {0, 0.632120558828558})}};
  const auto csv = sim::render_csv(plot);
  REQUIRE(csv.ok());
  CHECK(csv.value() == "time,v(out)\n0,0\n1.0000000000000001e-09,0.632120558828558\n");
}

TEST_CASE("AC dB/phase", "[cap:simulation-analyses-and-results][req:CSV export]") {
  sim::CsvOptions options;
  options.dbphase = true;
  const auto csv = sim::render_csv(ac_plot(), options);
  REQUIRE(csv.ok());
  CHECK(csv.value().starts_with("frequency,db(v(out)),ph(v(out))\n1000,0,180\n"));
  const auto reim = sim::render_csv(ac_plot());
  CHECK(reim.value().starts_with("frequency,re(v(out)),im(v(out))\n1000,-1,0\n"));
  // Quoting, a subset, and a vector shorter than the scale.
  const sim::Plot odd{"p", {real("time", "time", {0, 1}), real("a,b", "notype", {5}), real("c", "notype", {1, 2})}};
  sim::CsvOptions subset;
  subset.vectors = {"a,b"};
  CHECK(sim::render_csv(odd, subset).value() == "time,\"a,b\"\n0,5\n1,\n");
}

TEST_CASE("Triangle wave", "[cap:simulation-analyses-and-results][req:Waveform measurements]") {
  const sim::Plot plot{"p", {real("x", "notype", {0, 1, 2}), real("y", "notype", {0, 2, 0})}};
  const sim::ResultStore store;
  CHECK(store.measure_waveform(plot, {"cross", "y", {1, 1, 1}, {}, {}}).value() == 0.5);
  CHECK(store.measure_waveform(plot, {"cross", "y", {1, 1, -1}, {}, {}}).value() == 1.5);
  CHECK(store.measure_waveform(plot, {"avg", "y", {}, {}, {}}).value() == 1.0);
  CHECK(store.measure_waveform(plot, {"max", "y", {}, {}, {}}).value() == 2.0);
  CHECK(store.measure_waveform(plot, {"min", "y", {}, {}, {}}).value() == 0.0);
  CHECK(store.measure_waveform(plot, {"value_at", "y", {0.25}, {}, {}}).value() == 0.5);
  CHECK_THAT(store.measure_waveform(plot, {"rms", "y", {}, {}, {}}).value(), WithinRel(std::sqrt(4.0 / 3.0), 1e-12));
  CHECK(store.measure_waveform(plot, {"delay", "y", {1, 1, 1, 1, 1, -1}, {}, {}}).value() == 1.0);
  const auto none = store.measure_waveform(plot, {"cross", "y", {3, 1, 1}, {}, {}});
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == "SIM-RESULT-007");
  CHECK(none.error().text == "no crossing of 3 (rise, k=1)");
  CHECK(store.measure_waveform(plot, {"value_at", "y", {5}, {}, {}}).error().code == "SIM-RESULT-006");
  CHECK(store.measure_waveform(plot, {"median", "y", {}, {}, {}}).error().code == "SIM-RESULT-008");
  CHECK(store.measure_waveform(plot, {"max", "z", {}, {}, {}}).error().code == "SIM-RESULT-005");
}

TEST_CASE("A sample equal to the level counts once, in the direction of the next differing sample",
          "[cap:simulation-analyses-and-results][req:Waveform measurements]") {
  const sim::Plot plot{"p", {real("x", "notype", {0, 1, 2, 3, 4, 5}), real("y", "notype", {0, 1, 1, 2, 1, 0})}};
  CHECK(sim::measure(plot, {"cross", "y", {1, 1, 1}, {}, {}}).value() == 1.0);
  CHECK(sim::measure(plot, {"cross", "y", {1, 1, -1}, {}, {}}).value() == 4.0);
  CHECK(sim::measure(plot, {"cross", "y", {1, 2, 0}, {}, {}}).value() == 4.0);
  // A touch is no crossing: y = 0 1 0 never crosses 1.
  const sim::Plot touch{"p", {real("x", "notype", {0, 1, 2}), real("y", "notype", {0, 1, 0})}};
  CHECK_FALSE(sim::measure(touch, {"cross", "y", {1, 1, 0}, {}, {}}).ok());
}

TEST_CASE("RC rise time", "[cap:simulation-analyses-and-results][req:Waveform measurements]") {
  std::vector<double> t;
  std::vector<double> y;
  for (int i = 0; i <= 10000; ++i) {
    t.push_back(i * 1e-12);
    y.push_back(1.0 - std::exp(-t.back() / 1e-9));
  }
  const sim::Plot plot{"tran1", {real("time", "time", t), real("out", "voltage", y)}};
  const auto rise = sim::measure(plot, {"risetime", "v(out)", {0, 1}, {}, {}});
  REQUIRE(rise.ok());
  CHECK_THAT(rise.value(), WithinAbs(1e-9 * std::log(9.0), 1e-12));
}

TEST_CASE("Complex measurements use the magnitude unless a part is given",
          "[cap:simulation-analyses-and-results][req:Waveform measurements]") {
  const sim::Plot plot = ac_plot();
  CHECK(sim::measure(plot, {"max", "v(out)", {}, {}, {}}).value() == 1.0);
  CHECK(sim::measure(plot, {"max", "v(out)", {}, std::string{"im"}, {}}).value() == 0.5);
  CHECK_THAT(sim::measure(plot, {"value_at", "v(out)", {1e3}, std::string{"ph"}, {}}).value(), WithinAbs(180, 1e-9));
}

TEST_CASE("Parse transient status", "[cap:simulation-analyses-and-results][req:Progress reporting]") {
  sim::ProgressTracker tracker{sim::analyses_of({"* t", ".tran 1n 10n", ".end"})};
  tracker.status("tran: 34.5%");
  CHECK_THAT(tracker.progress(), WithinAbs(0.345, 1e-12));
  tracker.status("tran", 20.0);  // never down within one analysis
  CHECK_THAT(tracker.progress(), WithinAbs(0.345, 1e-12));
  tracker.scale(5e-9);
  CHECK_THAT(tracker.progress(), WithinAbs(0.5, 1e-12));
  tracker.status("--ready--");
  CHECK(tracker.progress() == 1.0);
}

TEST_CASE("Two analyses", "[cap:simulation-analyses-and-results][req:Progress reporting]") {
  sim::ProgressTracker tracker{sim::analyses_of({"* t", ".op", ".tran 1n 10n 0", ".end"})};
  tracker.status("op");
  CHECK(tracker.progress() == 0.0);
  tracker.status("tran: 50.0%");
  CHECK(tracker.progress() == 0.75);
  tracker.status("shooting: 7");
  CHECK(tracker.iteration() == 7U);
  tracker.status("something else");
  CHECK(tracker.progress() == 0.75);
  CHECK(sim::analyses_of({"* t", ".control", "tran 1n 2n", ".endc", ".AC dec 10 1 1e9"}) ==
        std::vector<sim::AnalysisSpan>{{"ac", 0, 0}});
}
