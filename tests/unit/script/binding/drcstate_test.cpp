// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `magic::drcstate busy|idle` from the DRC engine (tasks.md 5.8;
// openspec/changes/m4-scripting/design.md D6; capability scripting-api
// "Generic Tcl helpers defined at startup": the procedure the background
// DRC calls, redefined by the GUI or a script).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "binding_fixture.hpp"
#include "hefesto/drc/engine/background.hpp"
#include "hefesto/drc/engine/host.hpp"

using hefesto::script::test::BindingFixture;
namespace drc = hefesto::drc;

namespace {

constexpr int kTimeoutMs = 20000;

void record_states(BindingFixture& f) {
  f.value("set ::states {}; proc magic::drcstate {option} {lappend ::states $option}");
}

}  // namespace

TEST_CASE("drcstate hears the DRC events of its project",
          "[cap:scripting-api][req:Generic Tcl helpers defined at startup]") {
  BindingFixture f;
  record_states(f);
  f.editor.drc.events.publish_state(drc::CheckerState::busy);
  f.editor.drc.events.publish_state(drc::CheckerState::idle);
  CHECK(f.value("set ::states") == "busy idle");
  // A failing redefinition is ignored, as in Magic.
  f.value("proc magic::drcstate {option} {error broken}");
  f.editor.drc.events.publish_state(drc::CheckerState::busy);
  // Each project has its own interpreter and its own checker.
  BindingFixture other;
  record_states(other);
  f.editor.drc.events.publish_state(drc::CheckerState::idle);
  CHECK(other.value("set ::states") == "");
}

TEST_CASE("drcstate follows drc catchup", "[cap:scripting-api][req:Generic Tcl helpers defined at startup]") {
  BindingFixture f;
  record_states(f);
  f.value("box 0 0 10 10; paint m1");
  f.value("drc catchup");
  CHECK(f.value("set ::states") == "busy idle");
}

TEST_CASE("drcstate follows the background worker", "[cap:scripting-api][req:Generic Tcl helpers defined at startup]") {
  BindingFixture f;
  record_states(f);
  {
    drc::BackgroundHost host{f.editor, {}};
    f.value("box 0 0 10 10; paint m1");
    REQUIRE(f.editor.drc.worker->run_until_idle(f.editor, kTimeoutMs));
  }
  CHECK(f.value("set ::states") == "busy idle");
}
