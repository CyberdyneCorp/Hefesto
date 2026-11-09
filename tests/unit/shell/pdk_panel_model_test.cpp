// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager panel's model: rows with install state (shape + text) and
// the `pdk` command lines its buttons run through the Dispatcher.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/shell/pdk_panel_model.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

const std::string kHash = "0fe599b2afb6708d281543108caf8310912f54af";

pdk::Catalog catalog() {
  pdk::Family sky130;
  sky130.name = "sky130";
  sky130.variants = {pdk::Variant{"sky130A", {"sky130_fd_sc_hd", "sky130_fd_pr"}, {"sky130_fd_sc_hd"}},
                     pdk::Variant{"sky130B", {"sky130_fd_sc_hd"}, {"sky130_fd_sc_hd"}}};
  pdk::Family gf;
  gf.name = "gf180mcu";
  gf.variants = {pdk::Variant{"gf180mcuD", {}, {}}};
  return pdk::Catalog({sky130, gf});
}

}  // namespace

TEST_CASE("PDK panel buttons run pdk commands", "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  CHECK(pdk_command(PdkAction::list, "sky130", "sky130A", "") == "pdk list");
  CHECK(pdk_command(PdkAction::remote, "sky130", "sky130A", "") == "pdk remote sky130");
  CHECK(pdk_command(PdkAction::install, "sky130", "sky130A", kHash) == "pdk install sky130A " + kHash);
  CHECK(pdk_command(PdkAction::install, "sky130", "sky130A", kHash, {true, true}) ==
        "pdk install sky130A " + kHash + " -accept-license -accept-unverified");
  CHECK(pdk_command(PdkAction::enable, "sky130", "sky130A", kHash) == "pdk enable sky130A " + kHash);
  CHECK(pdk_command(PdkAction::pin, "sky130", "sky130A", kHash) == "pdk pin sky130A " + kHash);
  CHECK(pdk_command(PdkAction::uninstall, "sky130", "sky130A", kHash) == "pdk uninstall sky130A " + kHash);
  CHECK_FALSE(pdk_command(PdkAction::install, "sky130", "sky130A", "").has_value());
  CHECK(pdk_action_label(PdkAction::pin) == "Pin to project");
}

TEST_CASE("PDK rows show install state as shape and text",
          "[cap:hefesto-design-system][req:Status is never color-only]") {
  pdk::InstalledVersion installed;
  installed.family = "sky130";
  installed.variant = "sky130A";
  installed.version.hash = kHash;
  installed.version.commit_date = "2025-01-02T00:00:00Z";
  installed.enabled = true;
  const std::vector<PdkRow> rows = pdk_rows(catalog(), {installed});
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].title() == "sky130A  (sky130)");
  CHECK(rows[0].default_libraries == 1);
  CHECK(rows[0].enabled);
  CHECK(rows[0].installed_hash == kHash);
  CHECK(rows[0].state.kind == StatusKind::success);
  CHECK(rows[0].state.label().rfind("● enabled ", 0) == 0);
  CHECK(rows[1].state.label() == "◆ not installed");
  CHECK_FALSE(rows[1].installed_hash.has_value());
  CHECK(rows[2].family == "gf180mcu");

  installed.enabled = false;
  const std::vector<PdkRow> disabled = pdk_rows(catalog(), {installed});
  CHECK(disabled[0].state.kind == StatusKind::info);
  CHECK(disabled[0].state.text.rfind("installed ", 0) == 0);
}
