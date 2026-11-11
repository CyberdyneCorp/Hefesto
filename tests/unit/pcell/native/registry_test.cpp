// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability hefesto-scripting "Native PCell generators validated against
// Tcl": a native generator is used only for the exact PDK version it was
// validated against; any other version falls back to the Tcl generator.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/pcell/native/helpers.hpp"
#include "hefesto/pcell/native/registry.hpp"

namespace pcell = hefesto::pcell;

namespace {

struct Fake final : pcell::NativeGenerator {
  pcell::ParameterDict defaults() const override { return {}; }
  pcell::ParameterDict check(const pcell::ParameterDict& p, std::vector<hefesto::diag::Diagnostic>&) const override {
    return p;
  }
  hefesto::cmd::CommandResult draw(hefesto::editor::EditorState&, const pcell::ParameterDict&) const override {
    return hefesto::cmd::CommandResult::success();
  }
};

}  // namespace

TEST_CASE("native generators are found only for their exact PDK version",
          "[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]") {
  pcell::NativeRegistry registry;
  const hefesto::script::PdkIdentity pdk{"sky130", "sky130A", "5740c4a8"};
  registry.add(pdk, "sky130_fd_pr__nfet_01v8", std::make_unique<Fake>());
  CHECK(registry.find(pdk, "sky130_fd_pr__nfet_01v8") != nullptr);
  CHECK(registry.find({"sky130", "sky130A", "0fe599b2"}, "sky130_fd_pr__nfet_01v8") == nullptr);
  CHECK(registry.find(pdk, "sky130_fd_pr__pfet_01v8") == nullptr);
  // No native generator is built in yet (tasks.md 7.5/7.6): every device
  // runs its Tcl generator.
  CHECK(pcell::builtin_generators().size() == 0);
}
