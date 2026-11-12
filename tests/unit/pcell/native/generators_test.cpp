// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The native generators against the PDKs' Tcl generators in the same build
// (design D10, D-29; capability hefesto-scripting "Native PCell generators
// validated against Tcl", pcell-toolkit "Native generator equivalence"):
// the same `_defaults`, the same `_check` corrections and messages, and the
// same generated cell name and saved .mag (except `timestamp`) through
// `magic::gencell`. The full parameter sweeps run in
// scripts/pcell_equivalence.py (the `pcell-equivalence` CI job). Each test
// skips when its pinned PDK data set is not fetched.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../script/pdk/pdk_fixture.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/pcell/native/registry.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/startup.hpp"
#include "hefesto/script/pdk/trust.hpp"

namespace fs = std::filesystem;
namespace pcell = hefesto::pcell;
namespace script = hefesto::script;
namespace ref = hefesto::test;
using script::test::PdkSession;

namespace {

struct Pdk {
  const ref::PinnedPdk* pinned;
  const char* library;  // the Tcl namespace of the device procedures
};

const Pdk kSky130{&ref::kSky130A, "sky130"};
const Pdk kGf180{&ref::kGf180mcuD, "gf180mcu"};
const Pdk kSg13g2{&ref::kIhpSg13g2, "sg13g2"};

// A session that ran the PDK's rc file (with consent), or nothing when the
// data set is not fetched.
std::unique_ptr<PdkSession> pdk_session(const Pdk& pdk, bool native) {
  const auto variant = ref::pinned_variant(*pdk.pinned);
  if (!variant) {
    return nullptr;
  }
  auto s = std::make_unique<PdkSession>(script::test::pdk_env(variant->parent_path().string()));
  const fs::path rc = *variant / "libs.tech/magic" / (variant->filename().string() + ".magicrc");
  script::TestConsentHost host{script::ConsentDecision::run_once};
  script::StartupPlan plan;
  plan.files = {{rc, script::StartupKind::pdk, {}, false, script::pdk_identity_of(rc)}};
  plan.consent = &host;
  REQUIRE(script::run_startup_files(s->editor, plan).ok());
  if (!native) {
    s->eval("set ::hefesto::native_generators 0");
  }
  return s;
}

script::PdkIdentity identity_of(const Pdk& pdk) {
  const auto variant = ref::pinned_variant(*pdk.pinned);
  return *script::pdk_identity_of(*variant / "libs.tech/magic" / (variant->filename().string() + ".tcl"));
}

// The PDK's shared drawing and checking procedures fail when called, so a
// test that expects the native generator notices a fallback to Tcl.
void guard_tcl_generators(PdkSession& s, const Pdk& pdk) {
  for (const char* proc : {"mos_draw", "mos_check", "fixed_draw", "fixed_check"}) {
    s.eval(std::string{"proc "} + pdk.library + "::" + proc + " args {error {Tcl generator called}}");
  }
}

std::string masked_file(const fs::path& path) {
  std::ifstream in{path, std::ios::binary};
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("timestamp ", 0) != 0) {
      out << line << '\n';
    }
  }
  return out.str();
}

struct Generated {
  std::string cell;
  std::string top;    // the parent cell, timestamp lines masked
  std::string mag;    // the generated cell, timestamp lines masked ("" for a fixed device)
};

// `magic::gencell <lib>::<device> inst <args>` in a new cell `top`, saved.
Generated generate(PdkSession& s, const Pdk& pdk, const std::string& device, const std::string& args) {
  script::test::TempDir dir;
  s.eval("load top -silent");
  s.eval("box 0 0 0 0");
  s.eval(std::string{"magic::gencell "} + pdk.library + "::" + device + " inst " + args);
  Generated g;
  g.cell = s.eval("instance list celldef inst");
  s.eval("save " + (dir.path() / "top").string());
  g.top = masked_file(dir.path() / "top.mag");
  if (!g.cell.empty() && g.cell.rfind(device, 0) == 0) {  // a generated cell, not a library cell
    s.eval("load " + g.cell + " -silent");
    s.eval("save " + (dir.path() / g.cell).string());
    g.mag = masked_file(dir.path() / (g.cell + ".mag"));
  }
  return g;
}

void check_same_result(const Pdk& pdk, const std::string& device, const std::string& args, bool placed = true) {
  auto native = pdk_session(pdk, true);
  auto tcl = pdk_session(pdk, false);
  if (!native || !tcl) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  guard_tcl_generators(*native, pdk);
  INFO(device << " " << args);
  const Generated a = generate(*native, pdk, device, args);
  const Generated b = generate(*tcl, pdk, device, args);
  CHECK_FALSE(native->console_has("Tcl generator called"));
  CHECK(a.cell.empty() != placed);
  CHECK(a.cell == b.cell);
  CHECK(a.mag == b.mag);
  CHECK(a.top == b.top);
}

// A NativeHost over the session's interpreter: engine commands through
// `::magic::`, `puts` and variables recorded.
class SessionHost final : public pcell::NativeHost {
 public:
  explicit SessionHost(PdkSession& s) : s_(s) {}
  std::string run(const std::vector<std::string>& words) override {
    std::vector<std::string> w = words;
    w.front() = "::magic::" + w.front();
    const auto r = s_.tcl().evaluate(pcell::merge_tcl_list(w));
    ++runs;
    return r.ok() ? pcell::tcl_string(r.value) : std::string{};
  }
  void puts(bool to_stderr, std::string_view text) override { printed += (to_stderr ? "E:" : "O:") + std::string{text}; }
  void set_variable(std::string_view name, std::string_view value) override {
    variables += std::string{name} + "=" + std::string{value} + ";";
  }
  std::string printed;
  std::string variables;
  int runs = 0;

 private:
  PdkSession& s_;
};

// The Tcl `_check` with its `puts` captured: {code result printed}.
std::vector<std::string> tcl_check(PdkSession& s, const Pdk& pdk, const std::string& device,
                                   const std::string& parameters) {
  s.eval(R"(proc ::capture_check {script} {
    rename ::puts ::__capture_real_puts
    proc ::puts {args} {
      set stream [expr {[lindex $args 0] eq "-nonewline" ? [lindex $args end-1] : [lindex $args 0]}]
      append ::__captured [expr {$stream eq "stderr" ? "E:" : "O:"}] [lindex $args end]
      if {[lindex $args 0] ne "-nonewline"} {append ::__captured "\n"}
    }
    set ::__captured ""
    set code [catch {uplevel #0 $script} result]
    rename ::puts {}
    rename ::__capture_real_puts ::puts
    list $code $result $::__captured
  })");
  const std::string call = std::string{pdk.library} + "::" + device + "_check " +
                           pcell::merge_tcl_list(std::vector<std::string>{parameters});
  auto split = pcell::split_tcl_list(s.eval("::capture_check " + pcell::merge_tcl_list(std::vector<std::string>{call})));
  REQUIRE(split.ok());
  return split.value();
}

void check_checks(const Pdk& pdk, const std::string& device, const std::vector<std::string>& overrides) {
  auto s = pdk_session(pdk, true);
  if (!s) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  const pcell::NativeGenerator* g =
      pcell::builtin_generators().find(identity_of(pdk), std::string{pdk.library} + "::" + device);
  REQUIRE(g != nullptr);
  for (const auto& extra : overrides) {
    auto args = pcell::ParameterDict::from_tcl(extra);
    REQUIRE(args.ok());
    const pcell::ParameterDict parameters = g->defaults().merged(args.value());
    INFO(device << " " << parameters.to_tcl());
    SessionHost host{*s};
    const auto native = g->check(host, parameters);
    const auto tcl = tcl_check(*s, pdk, device, parameters.to_tcl());
    REQUIRE(tcl.size() == 3);
    if (!native) {
      CHECK(host.printed.empty());  // refused before printing: Tcl runs instead
      continue;
    }
    CHECK(host.printed == tcl[2]);
    if (native->ok()) {
      CHECK(tcl[0] == "0");
      CHECK(native->value().to_tcl() == tcl[1]);
    } else {
      CHECK(tcl[0] == "1");
    }
  }
}

}  // namespace

TEST_CASE("Same result native and Tcl", "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  // The spec scenario: an nfet with w=1.0, l=0.15, nf=2.
  check_same_result(kSky130, "sky130_fd_pr__nfet_01v8", "w 1.0 l 0.15 nf 2");
}

TEST_CASE("native MOSFETs of the three PDKs equal their Tcl generators",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  check_same_result(kSky130, "sky130_fd_pr__pfet_g5v0d16v0", "w 6.0 nf 2 m 2");
  check_same_result(kSky130, "sky130_fd_pr__cap_var_lvt", "nf 3");
  check_same_result(kSky130, "sky130_fd_pr__nfet_01v8_lvt", "nf 4 l 0.15");
  check_same_result(kGf180, "nfet_03v3", "w 1.0 nf 2 m 2 viasrc 100");
  check_same_result(kGf180, "pfet_10v0_asym", "nf 2");
  check_same_result(kSg13g2, "sg13_lv_nmos", "w 1.0 ng 2 m 2");
  check_same_result(kSg13g2, "sg13_hv_rfpmos", "ng 3 ccols 2");
}

TEST_CASE("native fixed devices place the same library cell and array",
          "[cap:pcell-toolkit][req:Native generator equivalence]") {
  check_same_result(kSky130, "sky130_fd_pr__npn_05v5_W1p00L1p00", "nx 2 ny 3 deltax 0.5");
  check_same_result(kSky130, "sky130_fd_pr__cap_vpp_08p6x07p8_m1m2_shieldl1", "");
  check_same_result(kGf180, "pnp_05p00x05p00", "nx 3 deltay 1.25");
  // gf180mcuD-bjt.tcl names a cell gf180mcu_fd_pr does not have: both fail alike.
  check_same_result(kGf180, "npn_00p54x08p00", "", false);
  // sg13g2 builds the placed cell from the PDK's layout script on first use.
  check_same_result(kSg13g2, "diodevdd_2kv", "");
  check_same_result(kSg13g2, "scr1", "nx 2 ny 3 deltax 0.5");
  check_same_result(kSg13g2, "schottky", "nx 2");
}

namespace {

// A new cell top holding sub, with the box at the origin; `edit_sub` makes sub the edit cell.
void place_in_sub_setup(PdkSession& s, bool edit_sub) {
  s.eval("load sub -silent");
  s.eval("load top -silent");
  s.eval("box 0 0 0 0");
  s.eval("getcell sub");
  if (edit_sub) {
    s.eval("select cell sub_0");
    s.eval("edit");
    s.eval("select clear");
  }
  s.eval("box 0 0 0 0");
}

}  // namespace

TEST_CASE("sg13g2 fixed devices follow Magic with magic-compat ON and keep the edit cell with it OFF",
          "[cap:scripting-pdk-integration][req:Generator drawing conventions]") {
  // Q-SCRIPT-45 (resolution: compat). The first device of a kind builds its cell
  // with `load <cell>` ... `load [cellname list window]`, which in Magic makes the
  // window's cell the edit cell (SCRIPT-PDK-14). OFF puts the edit context back.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    auto s = pdk_session(kSg13g2, true);
    if (!s) {
      SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
    }
    s->project.compat.magic_compat = compat;
    guard_tcl_generators(*s, kSg13g2);
    place_in_sub_setup(*s, true);
    REQUIRE(s->editor.edit_cell != nullptr);
    REQUIRE(s->editor.edit_cell->name() == "sub");
    s->eval("magic::gencell sg13g2::diodevdd_2kv D1");
    CHECK_FALSE(s->console_has("Tcl generator called"));
    CHECK(s->eval("cellname list window") == "top");
    CHECK(s->editor.edit_cell->name() == (compat ? "top" : "sub"));
    CHECK(s->eval("cellname list children sub") == (compat ? "" : "diodevdd_2kv"));
    CHECK(s->eval("lsort [cellname list children top]") == (compat ? "diodevdd_2kv sub" : "sub"));
    if (compat) {
      // The cell exists now: the next device goes to the edit cell, top.
      s->eval("box 5000 0 5000 0");
      s->eval("magic::gencell sg13g2::diodevdd_2kv D2");
      CHECK(s->eval("instance list celldef D2") == "diodevdd_2kv");
    }
  }
}

TEST_CASE("an sg13g2 fixed device without a box is not placed with magic-compat ON, as in Magic",
          "[cap:scripting-pdk-integration][req:Generator drawing conventions]") {
  // Regression (golden SCRIPT-PDK-14): in a new cell without a box, `box values {*}$curbox`
  // restores nothing, the box stays in the generated cell, getcell fails and `select cell {}`
  // selects the top cell.
  auto s = pdk_session(kSg13g2, true);
  if (!s) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  guard_tcl_generators(*s, kSg13g2);
  s->eval("load top");
  hefesto::diag::CollectingSink printed;
  s->dispatcher->add_sink(printed);
  CHECK(s->eval("magic::gencell sg13g2::diodevdd_2kv D1") == "0");
  CHECK_FALSE(s->console_has("Tcl generator called"));
  const auto emitted = printed.snapshot();
  CHECK(std::any_of(emitted.begin(), emitted.end(), [](const hefesto::diag::Diagnostic& d) {
    return d.text == "The box is in cell \"diodevdd_2kv\", not in the edit cell \"top\"";
  }));
  CHECK(s->eval("cellname list children top").empty());
  CHECK(s->eval("cellname list self") == "top");
  CHECK(s->eval("what -list") == "{} {} {{{Topmost cell in the window} top}}");
}

TEST_CASE("native _defaults equal the PDK procedures", "[cap:pcell-toolkit][req:Native generator equivalence]") {
  for (const Pdk* pdk : {&kSky130, &kGf180, &kSg13g2}) {
    auto s = pdk_session(*pdk, true);
    if (!s) {
      continue;
    }
    const script::PdkIdentity id = identity_of(*pdk);
    std::size_t compared = 0;
    for (const auto& [key_pdk, device] : pcell::builtin_generators().keys()) {
      if (key_pdk != id) {
        continue;
      }
      INFO(device);
      const auto* g = pcell::builtin_generators().find(id, device);
      CHECK(g->defaults().to_tcl() == s->eval("format %s [" + device + "_defaults]"));
      ++compared;
    }
    CHECK(compared > 0);
  }
}

TEST_CASE("native _check corrections and messages equal the PDK procedures",
          "[cap:pcell-toolkit][req:Native generator equivalence]") {
  const std::vector<std::string> mos = {"",          "w 0.1",        "l 0.01",          "nf 0 m 0",
                                        "nf 1.5",    "m 2.5",        "diffcov 10",       "polycov 150",
                                        "viasrc 150", "viagb -150",  "w 15 l 0.5 m 4",  "l 9 nf 6",
                                        "diffcov abc", "w 10 l 10 m 3 nf 3"};
  check_checks(kSky130, "sky130_fd_pr__nfet_01v8", mos);
  check_checks(kSky130, "sky130_fd_pr__cap_var", mos);
  check_checks(kGf180, "nfet_03v3", {"", "w 0.1", "l 0.01", "nf 0 m 0", "w 50 m 3", "l 40 nf 3", "lmax 0.5"});
  check_checks(kSg13g2, "sg13_lv_pmos", {"", "w 0.1", "ng 0", "ng 1.5", "viagate 150", "w 15 l 0.5 m 4"});
  const std::vector<std::string> fixed = {"", "nx 0", "ny 0", "nx 1.5", "deltax -1", "deltay 2u", "deltax abc"};
  check_checks(kSky130, "sky130_fd_pr__npn_05v5_W1p00L1p00", fixed);
  check_checks(kGf180, "pnp_10p00x00p42", fixed);
  check_checks(kSg13g2, "diodevdd_4kv", fixed);
  check_checks(kSg13g2, "schottky", fixed);
}

TEST_CASE("a parameter outside Tcl's numbers runs the Tcl generator",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  auto s = pdk_session(kSky130, true);
  if (!s) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  const auto* g = pcell::builtin_generators().find(identity_of(kSky130), "sky130::sky130_fd_pr__nfet_01v8");
  REQUIRE(g != nullptr);
  auto parameters = g->defaults();
  parameters.set("w", "1.0u");  // the Tcl mos_device fails on it; native reproduces only numbers
  SessionHost host{*s};
  CHECK_FALSE(g->draw(host, parameters).has_value());
  CHECK(host.runs == 0);  // refused before any engine command
  // A _check whose Tcl loop never ends (both extents over 30 um) is not reproduced.
  parameters = g->defaults();
  parameters.set("w", "40");
  parameters.set("l", "40");
  CHECK_FALSE(g->check(host, parameters).has_value());
  CHECK(host.printed.empty());
}

TEST_CASE("another PDK version runs the Tcl generator and says so",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  auto s = pdk_session(kSky130, true);
  if (!s) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  // The helpers now come from another sky130A version (as if that version's
  // rc file had run): no native generator is registered for it.
  script::PdkIdentity other = identity_of(kSky130);
  other.version = "0fe599b2afb6708d281543108caf8310912f54af";
  static_cast<void>(s->editor.script.helper_origins.record("::magic::spice2float", other, {}));
  s->eval("proc sky130::mos_draw args {set ::tcl_draw_ran 1; error {stop}}");
  s->eval("load top -silent");
  hefesto::diag::CollectingSink printed;
  s->dispatcher->add_sink(printed);
  static_cast<void>(s->tcl().evaluate("magic::gencell sky130::sky130_fd_pr__nfet_01v8 inst w 1.0"));
  CHECK(s->eval("info exists ::tcl_draw_ran") == "1");
  // PCELL-NATIVE-001 was reported once for the device.
  CHECK(s->editor.script.gencell.native_fallbacks.contains("sky130::sky130_fd_pr__nfet_01v8"));
  // Regression (golden DESKTOP-ALPHA-*, SCRIPT-PDK-07/09): the report is Hefesto-only, so the
  // `--diagnostics=magic` console of `magic -dnull` does not print it.
  const auto emitted = printed.snapshot();
  const auto fallback = std::find_if(emitted.begin(), emitted.end(),
                                     [](const hefesto::diag::Diagnostic& d) { return d.code == "PCELL-NATIVE-001"; });
  REQUIRE(fallback != emitted.end());
  CHECK(fallback->native_only);
  CHECK_FALSE(hefesto::diag::shown_in(*fallback, hefesto::diag::Format::magic));
}

TEST_CASE("::hefesto::native_generators 0 runs the Tcl generators",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  auto s = pdk_session(kGf180, false);
  if (!s) {
    SKIP("PDK data set not fetched (scripts/fetch_pdk.py)");
  }
  s->eval("proc gf180mcu::mos_draw args {set ::tcl_draw_ran 1; error {stop}}");
  s->eval("load top -silent");
  static_cast<void>(s->tcl().evaluate("magic::gencell gf180mcu::nfet_03v3 inst"));
  CHECK(s->eval("info exists ::tcl_draw_ran") == "1");
}
