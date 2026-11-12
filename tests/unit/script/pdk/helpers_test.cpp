// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// scripting-pdk-integration "PDK helper procedures in the engine namespace"
// and scripting-api "Script-defined procedures in the engine namespace"
// (tasks.md 6.4; Q-SCRIPT-40, Q-SCRIPT-41): the helpers come from the
// pinned PDKs' own scripts, never predefined; a cross-PDK redefinition
// warns and takes effect. Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/script/pdk/helper_origins.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
namespace ref = hefesto::test;
using script::test::PdkSession;
using script::test::TempDir;
using script::test::write_file;

namespace {

class Redefinitions final : public script::ScriptObserver {
 public:
  void on_helper_redefined(const script::HelperRedefinition& r) override { seen.push_back(r); }
  std::vector<script::HelperRedefinition> seen;
};

// A session whose interpreter tracks helper origins through PDKINT's hook
// (no engine command bound, so the binding's own hook is absent), with
// `magic::cif scale output` answering `scale` (BINDING's `cif`, tasks.md 5.5).
struct HelperSession {
  PdkSession s;
  Redefinitions observer;
  hefesto::diag::CollectingSink sink;

  explicit HelperSession(const std::optional<std::string>& pdk_root = std::nullopt, double scale = 0.005)
      : s(script::test::pdk_env(pdk_root), false) {
    REQUIRE(script::install_helper_origin_hook(s.tcl(), s.editor.script.helper_origins, s.editor.script.events, &sink)
                .ok());
    s.editor.script.events.add(observer);
    s.tcl().define_command("::magic::cif", [scale](const hefesto::cmd::Invocation&) {
      return hefesto::cmd::CommandResult::success(scale);
    });
  }
  ~HelperSession() { s.editor.script.events.remove(observer); }
  HelperSession(const HelperSession&) = delete;
  HelperSession& operator=(const HelperSession&) = delete;

  // Sources `file` with a permissive `unknown`, so that only the helper
  // definitions matter (the device scripts' engine calls are not under test).
  void source_helpers(const std::filesystem::path& file) {
    s.eval("rename ::unknown ::pdk_test_unknown; proc ::unknown {args} {return {}}");
    const auto result = s.tcl().source(file);
    s.eval("rename ::unknown {}; rename ::pdk_test_unknown ::unknown");
    INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text));
    REQUIRE(result.ok());
  }
};

std::optional<std::filesystem::path> magic_dir(const ref::PinnedPdk& pdk) {
  const auto variant = ref::pinned_variant(pdk);
  return variant ? std::optional{*variant / "libs.tech/magic"} : std::nullopt;
}

}  // namespace

TEST_CASE("Helper name not reserved", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  PdkSession s;
  for (const char* name : {"u2l", "l2u", "i2u", "u2i", "float2spice", "spice2float", "3digitpastdecimal",
                           "searchcellondisk", "iscellnameunique", "query_mylib_ip", "query_my_projects", "magceil",
                           "magfloor"}) {
    INFO(name);
    CHECK(s.eval(std::string{"info commands magic::"} + name).empty());
  }
}

TEST_CASE("Micron conversion", "[cap:scripting-pdk-integration][req:PDK helper procedures in the engine namespace]") {
  const auto sky130 = magic_dir(ref::kSky130A);
  const auto sg13g2 = magic_dir(ref::kIhpSg13g2);
  if (!sky130 || !sg13g2) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  {
    HelperSession h{sky130->parent_path().parent_path().parent_path().string()};
    h.source_helpers(*sky130 / "sky130A.tcl");  // the template with common/pdk.tcl appended
    CHECK(h.s.eval("magic::u2i 0.17") == "34.0");
    CHECK(h.s.eval("magic::i2u 34") == "0.17");
    CHECK(h.s.eval("magic::3digitpastdecimal 0.12345") == "0.123");  // scripting-api "PDK helper callable"
  }
  {
    HelperSession h;
    h.s.eval("namespace eval sg13g2 {}");  // ihp-sg13g2.tcl creates it before sourcing its parts
    h.source_helpers(*sg13g2 / "ihp-sg13g2-util.tcl");
    CHECK(h.s.eval("magic::u2i 0.17") == "34");
    CHECK(h.s.eval("magic::i2u 34") == "0.170");
  }
}

TEST_CASE("SPICE suffix parsing", "[cap:scripting-pdk-integration][req:PDK helper procedures in the engine namespace]") {
  for (const auto& [pdk, file] : {std::pair{&ref::kSky130A, "sky130A.tcl"}, std::pair{&ref::kGf180mcuD, "gf180mcuD.tcl"},
                                  std::pair{&ref::kIhpSg13g2, "ihp-sg13g2-util.tcl"}}) {
    const auto dir = magic_dir(*pdk);
    if (!dir) {
      SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
    }
    INFO(file);
    HelperSession h;
    h.s.eval("namespace eval sg13g2 {}");
    h.source_helpers(*dir / file);
    h.s.eval("rename ::puts ::pdk_test_puts; proc ::puts {args} {lappend ::printed $args}");
    CHECK(h.s.eval("magic::spice2float 2k") == "2000.0");
    CHECK(h.s.eval("magic::spice2float 1.5") == "1.5");
    CHECK(h.s.eval("expr {[magic::spice2float 0.42u] == 0.42 * 1.0e-6}") == "1");
    CHECK(h.s.eval("info exists ::printed") == "0");
    // "n" maps to "* 1.0 e-9", which is not an expression (Q-SCRIPT-40, replicated).
    CHECK(h.s.eval("magic::spice2float 10n") == "0.0");
    CHECK(h.s.eval("set ::printed") == "{stderr {Value is not numeric!}}");
    h.s.eval("rename ::puts {}; rename ::pdk_test_puts ::puts");
  }
}

TEST_CASE("Cross-PDK helper redefinition", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  const auto sky130 = magic_dir(ref::kSky130A);
  const auto sg13g2 = magic_dir(ref::kIhpSg13g2);
  if (!sky130 || !sg13g2) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  HelperSession h;
  h.source_helpers(*sky130 / "sky130A.tcl");
  CHECK(h.observer.seen.empty());  // one PDK's own files never warn
  REQUIRE(h.s.editor.script.helper_origins.origin("::magic::i2u").has_value());
  CHECK(h.s.editor.script.helper_origins.origin("::magic::i2u")->variant == "sky130A");

  h.s.eval("namespace eval sg13g2 {}");
  h.source_helpers(*sg13g2 / "ihp-sg13g2-util.tcl");
  CHECK(h.s.eval("magic::i2u 34") == "0.170");  // the new definition takes effect
  const auto i2u = std::find_if(h.observer.seen.begin(), h.observer.seen.end(),
                                [](const auto& r) { return r.procedure == "::magic::i2u"; });
  REQUIRE(i2u != h.observer.seen.end());
  CHECK(i2u->first.variant == "sky130A");
  CHECK(i2u->second.variant == "ihp-sg13g2");
  CHECK(i2u->location.file == (*sg13g2 / "ihp-sg13g2-util.tcl").string());
  bool warned = false;
  for (const auto& d : h.sink.snapshot()) {
    if (d.code == "SCRIPT-PDK-003" && d.text.find("magic::i2u") != std::string::npos &&
        d.text.find("sky130A") != std::string::npos && d.text.find("ihp-sg13g2") != std::string::npos) {
      warned = true;
    }
  }
  CHECK(warned);
}

TEST_CASE("procedures outside magic:: or outside a PDK are not tracked",
          "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  HelperSession h;
  TempDir dir;
  write_file(dir.path() / "a/sky130A/libs.tech/magic/x.tcl", "proc magic::mine {} {return a}\nproc other {} {}\n");
  write_file(dir.path() / "b/gf180mcuD/libs.tech/magic/y.tcl",
             "namespace eval gf180mcu {proc ::magic::mine {} {return b}}\n");
  write_file(dir.path() / "user.tcl", "proc magic::mine {} {return user}\n");
  h.source_helpers(dir.path() / "a/sky130A/libs.tech/magic/x.tcl");
  CHECK_FALSE(h.s.editor.script.helper_origins.origin("::other").has_value());
  h.source_helpers(dir.path() / "b/gf180mcuD/libs.tech/magic/y.tcl");
  REQUIRE(h.observer.seen.size() == 1);
  CHECK(h.observer.seen.front().location.line == 1);
  h.source_helpers(dir.path() / "user.tcl");  // not a PDK file: no origin, no warning
  CHECK(h.observer.seen.size() == 1);
  CHECK(h.s.eval("magic::mine") == "user");
  h.s.eval("proc magic::typed {} {}");  // typed, not sourced
  CHECK_FALSE(h.s.editor.script.helper_origins.origin("::magic::typed").has_value());
}

TEST_CASE("the origin tracker reports a different PDK only", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  script::HelperOriginTracker tracker;
  const script::PdkIdentity a{"sky130", "sky130A", "1"};
  const script::PdkIdentity b{"ihp-sg13", "ihp-sg13g2", "2"};
  CHECK_FALSE(tracker.record("::magic::i2u", a, {"x.tcl", 3}).has_value());
  CHECK_FALSE(tracker.record("::magic::i2u", a, {"y.tcl", 4}).has_value());
  const auto again = tracker.record("::magic::i2u", b, {"z.tcl", 5});
  REQUIRE(again.has_value());
  CHECK(again->first == a);
  CHECK(again->second == b);
  CHECK(tracker.origin("::magic::i2u") == b);
  CHECK(script::redefinition_warning(*again).text ==
        "Warning: magic::i2u defined by the sky130A scripts (sky130/sky130A@1) is redefined by the ihp-sg13g2 "
        "scripts (ihp-sg13/ihp-sg13g2@2)");
}
