// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// scripting-pdk-integration: the pinned PDKs' rc files and device scripts
// run unchanged (tasks.md 6.3): "PDK startup file contract", "PDK
// device-script contract", "Generator drawing conventions", "Command
// surface used by device generators"; and the command inventory (tasks.md
// 6.8): "PDK command and variable inventory". Each test skips when the
// reference data is not fetched. Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/inventory.hpp"
#include "hefesto/script/pdk/startup.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/pdk/widgets.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
namespace ref = hefesto::test;
using script::test::PdkSession;

namespace {

std::filesystem::path rc_of(const std::filesystem::path& variant) {
  return variant / "libs.tech/magic" / (variant.filename().string() + ".magicrc");
}

// Runs `rc` as the PDK startup file of `s`, with consent; `clean`: no
// startup diagnostic (every line ran).
void run_rc(PdkSession& s, const std::filesystem::path& rc, bool clean = true) {
  script::TestConsentHost host{script::ConsentDecision::run_once};
  script::StartupPlan plan;
  plan.files = {{rc, script::StartupKind::pdk, {}, false, script::pdk_identity_of(rc)}};
  plan.consent = &host;
  const auto run = script::run_startup_files(s.editor, plan);
  REQUIRE(run.ok());
  for (const auto& d : run.diagnostics) {
    UNSCOPED_INFO(d.code << " " << d.text);
  }
  if (clean) {
    CHECK(run.diagnostics.empty());
  }
}

bool path_search_has(PdkSession& s, const std::string& dir) {
  const std::string search = " " + s.eval("path search") + " ";
  return search.find(" " + dir + " ") != std::string::npos;
}

}  // namespace

TEST_CASE("sky130 power names and library paths", "[cap:scripting-pdk-integration][req:PDK startup file contract]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const std::string root = variant->parent_path().string();
  const auto env = script::test::pdk_env(root);
  PdkSession s{env};
  run_rc(s, rc_of(*variant));
  CHECK(s.eval("tech name") == "sky130A");
  CHECK(s.eval("list $VDD $GND $SUB") == "VPWR VGND VSUBS");
  CHECK(s.eval("snap") == "lambda");
  CHECK(s.eval("ext2spice scale") == "off");
  CHECK(path_search_has(s, root + "/sky130A/libs.ref/sky130_fd_sc_hd/mag"));
  CHECK(path_search_has(s, root + "/sky130A/libs.ref/sky130_sram_macros/mag"));
}

TEST_CASE("gf180mcu power names and library paths", "[cap:scripting-pdk-integration][req:PDK startup file contract]") {
  const auto variant = ref::pinned_variant(ref::kGf180mcuD);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const std::string root = variant->parent_path().string();
  const auto env = script::test::pdk_env(root);
  PdkSession s{env};
  run_rc(s, rc_of(*variant));
  CHECK(s.eval("tech name") == "gf180mcuD");
  CHECK(s.eval("list $VDD $GND $SUB") == "VDD VSS VSUBS");
  CHECK(s.eval("snap") == "lambda");
  CHECK(path_search_has(s, root + "/gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag"));
}

TEST_CASE("IHP locates its files relative to the rc file", "[cap:scripting-pdk-integration][req:PDK startup file contract]") {
  const auto variant = ref::pinned_variant(ref::kIhpSg13g2);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(std::nullopt);
  PdkSession s{env};
  const std::string before = s.eval("path search");
  run_rc(s, rc_of(*variant));
  CHECK(s.eval("tech name") == "ihp-sg13g2");
  CHECK(s.eval("set SUB") == "sub!");
  CHECK(s.eval("info exists PDK_ROOT") == "0");
  CHECK(s.eval("path search") == before);  // no addpath
  CHECK(s.eval("namespace exists sg13g2") == "1");  // the generator was sourced
  CHECK(s.eval("set sg13g2::script_path") == (*variant / "libs.tech/magic").string());
}

TEST_CASE("Older rc generation", "[cap:scripting-pdk-integration][req:PDK startup file contract]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  const std::filesystem::path old_rc = std::filesystem::path{HEFESTO_SOURCE_DIR} /
                                       ".cache/pdks/ciel/sky130/versions/0fe599b2afb6708d281543108caf8310912f54af/"
                                       "sky130A/libs.tech/magic/sky130A.magicrc";
  if (!variant || !std::filesystem::exists(old_rc)) {
    SKIP("PDK cache absent (the 0fe599b2 sky130A set is not fetched)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());  // the new-pin tech files
  PdkSession s{env};
  run_rc(s, old_rc);
  CHECK(s.eval("tech name") == "sky130A");
  CHECK(s.eval("set MAGTYPE") == "mag");
  CHECK(path_search_has(s, variant->parent_path().string() + "/sky130A/libs.ref/sky130_fd_sc_hd/mag"));
  CHECK(s.eval("info commands magic::query_my_projects") == "::magic::query_my_projects");
}

TEST_CASE("Technology guard quits", "[cap:scripting-pdk-integration][req:PDK startup file contract]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  script::test::TempDir dir;
  // A copy of the sky130A rc whose `tech load` fails: PDK_ROOT has no sky130A.
  const auto rc = dir.path() / "sky130A/libs.tech/magic/sky130A.magicrc";
  std::ifstream in{rc_of(*variant)};
  script::test::write_file(rc, std::string{std::istreambuf_iterator<char>{in}, {}});
  const auto env = script::test::pdk_env((dir.path() / "nowhere").string());
  PdkSession s{env};
  run_rc(s, rc, false);
  CHECK(s.eval("tech name") == "minimum");
  CHECK(s.script_session.quit_requested);
  // `quit` ends the file (BINDING stops the script).
  CHECK(s.eval("namespace exists sky130") == "0");
}

TEST_CASE("Menu registration survives multiple windows", "[cap:scripting-pdk-integration][req:PDK device-script contract]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());
  PdkSession s{env};
  // In wrapper mode `closewrapper` exists (ui-gui-managers); in NULL graphics it does not.
  s.eval("proc ::closewrapper {framename} {return closed}");
  run_rc(s, rc_of(*variant));
  CHECK(s.eval("set Opts(callback)") == "sky130::addtechmenu $framename; ");
  CHECK(s.eval("list $Opts(hidelocked) $Opts(hidespecial)") == "1 0");
  CHECK(s.eval("list $PDKNAME $PDKNAMESPACE") == "sky130A sky130");
  CHECK(s.eval("set PDKPATH") == variant->string());
  CHECK(s.eval("info commands closewrapperonly") == "closewrapperonly");  // closewrapper was wrapped
  CHECK(s.eval("info body ::closewrapper").find("closewrapperonly") != std::string::npos);
  CHECK(s.eval("namespace eval sky130 {namespace path}") == "::tcl::mathop ::tcl::mathfunc");
  CHECK(s.eval("dict get $sky130::ruleset poly_surround") == "0.08");
}

TEST_CASE("Callback order with a prior callback", "[cap:scripting-pdk-integration][req:PDK device-script contract]") {
  const auto variant = ref::pinned_variant(ref::kIhpSg13g2);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  PdkSession s;
  s.eval("set Opts(callback) {mycb $framename}");
  run_rc(s, rc_of(*variant));
  CHECK(s.eval("set Opts(callback)") == "sg13g2::addtechmenu $framename; mycb $framename");
}

TEST_CASE("Microns in a generator", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());
  PdkSession s{env};
  run_rc(s, rc_of(*variant));
  s.eval("snap internal; units internal; box 0 0 0 0");
  s.eval("box size 0.17um 0.17um");
  s.eval("paint mcon");
  CHECK(s.eval("box values") == "0 0 34 34");
  CHECK(s.eval("snap") == "internal");
  CHECK_FALSE(s.console_has("rror"));
}

TEST_CASE("Pin from a short label", "[cap:scripting-pdk-integration][req:Generator drawing conventions]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());
  PdkSession s{env};
  run_rc(s, rc_of(*variant));
  s.eval("snap internal; units internal; box 0 0 34 34; paint li");
  s.eval("label B c li");
  s.eval("select area label");
  s.eval("port make");
  CHECK(s.eval("port B index") == "1");
}

// "Snap restored after a draw" (sky130::cap_draw through magic::gencell)
// needs the gencell framework (PCELL, tasks.md 7.2); a direct drawing
// procedure exercises the same conventions without it.
TEST_CASE("a direct drawing procedure leaves the box stack as it found it",
          "[cap:scripting-pdk-integration][req:Generator drawing conventions]") {
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());
  PdkSession s{env};
  run_rc(s, rc_of(*variant));
  s.eval("snap internal; box 0 0 200 68");
  const auto draw = s.tcl().evaluate("sky130::subconn_draw");
  INFO((draw.diagnostics.empty() ? std::string{} : draw.diagnostics.front().text));
  CHECK(draw.ok());
  CHECK(s.eval("box values") == "0 0 200 68");  // pushbox ... popbox
  CHECK(s.eval("snap") == "internal");
  s.eval("select area psc");
  CHECK_FALSE(s.eval("what -list").empty());
}

TEST_CASE("Inventory coverage check", "[cap:scripting-pdk-integration][req:PDK command and variable inventory]") {
  struct Column {
    const ref::PinnedPdk* pdk;
    const char* family;
    bool pdk_root;
  };
  for (const Column& column : {Column{&ref::kSky130A, "sky130", true}, Column{&ref::kGf180mcuD, "gf180mcu", true},
                               Column{&ref::kIhpSg13g2, "sg13g2", false}}) {
    const auto variant = ref::pinned_variant(*column.pdk);
    if (!variant) {
      SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
    }
    INFO(column.family);
    const auto env = script::test::pdk_env(column.pdk_root ? std::optional{variant->parent_path().string()} : std::nullopt);
    PdkSession s{env};
    std::vector<std::string> names = s.registry.names();
    for (const auto& name : hefesto::pcell::framework_command_names()) {
      names.emplace_back(name);
    }
    script::CommandTrace trace;
    REQUIRE(trace.install(s.tcl(), names).ok());
    run_rc(s, rc_of(*variant));
    const auto* inventory = script::pdk_inventory(column.family);
    REQUIRE(inventory != nullptr);
    CHECK_FALSE(trace.names().empty());
    CHECK(trace.names().contains("tech"));
    CHECK(script::uncovered_commands(trace.names(), *inventory).empty());
    // Every inventory name exists in the session except the ones still
    // pending in other workstreams (listed in the PDKINT report).
    std::set<std::string> missing;
    for (const auto& name : script::missing_commands(s.tcl(), *inventory)) {
      missing.insert(name);
    }
    for (const auto& name : missing) {
      INFO("missing " << name);
      // addcommandentry: GUIMGR (ui-gui-managers command entry); gds: M5.
      CHECK((name == "addcommandentry" || name == "gds"));
    }
  }
}

namespace {

// The `magic::gencell <lib>::<device>` items of the toolkit menus the PDK's
// window callback registered.
std::vector<std::string> gencell_devices(const PdkSession& s) {
  std::vector<std::string> out;
  constexpr std::string_view kPrefix = "magic::gencell ";
  for (const auto& menu : s.editor.script.gencell.menus.menus) {
    for (const auto& item : menu.items) {
      if (item.command.starts_with(kPrefix)) {
        std::string device = item.command.substr(kPrefix.size());
        device.erase(device.find_last_not_of(' ') + 1);
        out.push_back(std::move(device));
      }
    }
  }
  return out;
}

}  // namespace

TEST_CASE("Inventory coverage check with every generator", "[cap:scripting-pdk-integration][req:PDK command and variable inventory]") {
  struct Column {
    const ref::PinnedPdk* pdk;
    const char* family;
    bool pdk_root;
    std::size_t devices;  // the table's "Generated-cell devices (magic::gencell menu entries)"
  };
  // gf180mcu: the table says 39, counting four entries gf180mcuD.tcl has
  // commented out (lines 100-106: the dss and asym FETs); 35 are active
  // (open item for the spec table).
  for (const Column& column : {Column{&ref::kSky130A, "sky130", true, 36}, Column{&ref::kGf180mcuD, "gf180mcu", true, 35},
                               Column{&ref::kIhpSg13g2, "sg13g2", false, 21}}) {
    const auto variant = ref::pinned_variant(*column.pdk);
    if (!variant) {
      SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
    }
    INFO(column.family);
    const auto env = script::test::pdk_env(column.pdk_root ? std::optional{variant->parent_path().string()} : std::nullopt);
    PdkSession s{env};
    s.eval("proc ::closewrapper {framename} {return closed}");  // wrapper mode
    std::vector<std::string> names = s.registry.names();
    for (const auto& name : hefesto::pcell::framework_command_names()) {
      names.emplace_back(name);
    }
    script::CommandTrace trace;
    REQUIRE(trace.install(s.tcl(), names).ok());
    hefesto::platform::ScriptedFilePicker picker;
    script::WidgetInterceptor interceptor{s.editor.script.menu_model, picker, s.editor.script.events};
    REQUIRE(interceptor.install(s.tcl(), ".layout1").ok());
    run_rc(s, rc_of(*variant), false);
    s.eval("apply {{framename} {global Opts; eval $Opts(callback)}} .layout1");
    const auto devices = gencell_devices(s);
    CHECK(devices.size() == column.devices);
    // Each generator with its defaults, non-interactively (toolkit.tcl's
    // "device, empty args" row of the gencell dispatch).
    std::size_t index = 0;
    for (const auto& device : devices) {
      const auto run = s.tcl().evaluate("magic::gencell " + device + " inv_trace_" + std::to_string(++index));
      UNSCOPED_INFO(device << ": " << (run.ok() ? std::string{"ok"} : run.diagnostics.front().text));
      if (device == "gf180mcu::nwell_3p3") {
        // The PDK's menu names a device it does not define (only
        // gf180mcu::nwell_*): Magic's gencell fails the same way
        // (toolkit.tcl:720-723).
        REQUIRE_FALSE(run.ok());
        CHECK(run.diagnostics.front().text == "No import routine for gf180mcu library cell nwell_3p3!");
        continue;
      }
      CHECK(run.ok());
    }
    const auto* inventory = script::pdk_inventory(column.family);
    REQUIRE(inventory != nullptr);
    const auto uncovered = script::uncovered_commands(trace.names(), *inventory);
    for (const auto& name : uncovered) {
      UNSCOPED_INFO("uncovered " << name);
    }
    CHECK(uncovered.empty());
  }
}

TEST_CASE("the inventory columns", "[cap:scripting-pdk-integration][req:PDK command and variable inventory]") {
  REQUIRE(script::pdk_inventories().size() == 3);
  const auto* sky130 = script::pdk_inventory("sky130");
  const auto* gf180 = script::pdk_inventory("gf180mcu");
  const auto* sg13g2 = script::pdk_inventory("ihp-sg13");
  REQUIRE((sky130 && gf180 && sg13g2));
  CHECK(sky130->covers("ext2spice"));
  CHECK_FALSE(gf180->covers("ext2spice"));
  CHECK(gf180->covers("save"));
  CHECK(sg13g2->covers("wire"));
  CHECK_FALSE(sg13g2->covers("snap"));
  CHECK(sg13g2->covers("closewrapper"));
  CHECK(script::uncovered_commands({"box", "frobnicate"}, *sky130) == std::vector<std::string>{"frobnicate"});
  CHECK(script::pdk_inventory("scmos") == nullptr);
}

TEST_CASE("ext2spice scale is a setting", "[cap:scripting-pdk-integration][req:Command surface used by device generators]") {
  PdkSession s;
  CHECK(s.eval("ext2spice scale") == "on");  // Magic's default
  s.eval("ext2spice scale off");
  CHECK(s.eval("ext2spice scale") == "off");
  s.eval("ext2spice scale yes");
  CHECK(s.eval("ext2spice scale") == "on");
  const auto bad = s.dispatcher->execute({"ext2spice", {"scale", "maybe"}, hefesto::cmd::Origin::script, std::nullopt});
  CHECK_FALSE(bad.ok());
  const auto later = s.dispatcher->execute({"ext2spice", {"hierarchy", "on"}, hefesto::cmd::Origin::script, std::nullopt});
  REQUIRE_FALSE(later.ok());
  CHECK(later.diagnostics.front().code == "UI-CLIENT-002");  // the netlist writer arrives in M7
}

TEST_CASE("Portable library reference", "[cap:scripting-pdk-integration][req:PDK-relative paths in saved files]") {
  // A session started with the sky130A rc file: PDKPATH (set by the device
  // script) is checked before PDK_ROOT, so the child's path is written as
  // $PDKPATH/libs.ref/... and read back through the script variable.
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (!variant) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  const auto env = script::test::pdk_env(variant->parent_path().string());
  PdkSession s{env};
  run_rc(s, rc_of(*variant), false);
  CHECK(s.eval("set PDKPATH") == variant->string());
  script::test::TempDir dir;
  const std::string parent = (dir.path() / "parent").string();
  s.eval("load " + parent);
  s.eval("box values 0 0 0 0");
  s.eval("getcell sky130_fd_sc_hd__inv_1");
  s.eval("save " + parent);
  std::ifstream in{parent + ".mag"};
  const std::string text{std::istreambuf_iterator<char>{in}, {}};
  INFO(text);
  CHECK(text.find("use sky130_fd_sc_hd__inv_1  sky130_fd_sc_hd__inv_1_0 $PDKPATH/libs.ref/sky130_fd_sc_hd/mag") !=
        std::string::npos);
  // Read back in a fresh cell list: $PDKPATH expands to the script variable.
  s.eval("cellname delete parent");
  s.eval("cellname delete sky130_fd_sc_hd__inv_1");
  s.eval("load " + parent);
  s.eval("select top cell; expand");
  // The printed form is the directory; the list form returns the whole
  // stored path with magic-compat ON (Q-CELL-45).
  const std::string libdir = (*variant / "libs.ref/sky130_fd_sc_hd/mag").string();
  CHECK(s.eval("cellname filepath sky130_fd_sc_hd__inv_1") == libdir);
  CHECK(s.eval("cellname list filepath sky130_fd_sc_hd__inv_1").starts_with(libdir + "/sky130_fd_sc_hd__inv_1"));
}
