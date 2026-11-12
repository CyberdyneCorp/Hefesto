// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared script types (openspec/changes/m4-scripting/design.md D4). The
// [m4-scaffold] tests are replaced by [cap:][req:] tests as the owners land.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/script/events.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/script/types.hpp"

namespace script = hefesto::script;

namespace {

struct Recorder final : script::ScriptObserver {
  int denied = 0;
  int skipped = 0;
  int redefined = 0;
  int consent = 0;
  int menus = 0;
  void on_denied(const script::DeniedOperation&) override { ++denied; }
  void on_skipped_statement(const script::SkippedStatement&) override { ++skipped; }
  void on_helper_redefined(const script::HelperRedefinition&) override { ++redefined; }
  void on_consent_requested(const script::ConsentRequest&) override { ++consent; }
  void on_menu_model_changed() override { ++menus; }
};

}  // namespace

TEST_CASE("script events reach every observer once until removed", "[m4-scaffold]") {
  script::ScriptEvents events;
  Recorder recorder;
  events.add(recorder);
  events.add(recorder);
  CHECK(events.size() == 1);
  events.publish_denied({"exec", "ls", {"rc.tcl", 3}});
  events.publish_skipped_statement({".layout1.titlebar.foo configure", {}});
  events.publish_helper_redefined({"::magic::u2i", {}, {}, {}});
  events.publish_consent_requested({{"sky130", "sky130A", "5740c4a8"}, "/pdk/sky130A.magicrc", false});
  events.publish_menu_model_changed();
  CHECK(recorder.denied == 1);
  CHECK(recorder.skipped == 1);
  CHECK(recorder.redefined == 1);
  CHECK(recorder.consent == 1);
  CHECK(recorder.menus == 1);
  events.remove(recorder);
  events.publish_menu_model_changed();
  CHECK(recorder.menus == 1);
}

TEST_CASE("a PDK identity prints family, variant and version", "[m4-scaffold]") {
  const script::PdkIdentity pdk{"sky130", "sky130A", "5740c4a8"};
  CHECK(pdk.to_string() == "sky130/sky130A@5740c4a8");
  CHECK(pdk < script::PdkIdentity{"sky130", "sky130B", "5740c4a8"});
}
