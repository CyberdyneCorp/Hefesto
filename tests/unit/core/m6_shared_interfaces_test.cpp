// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M6 scaffold (openspec/changes/m6-lef-def-oasis tasks.md 2.4, 2.5): the
// shared read settings split out of gds::GdsSettings into
// MaskSession::stream_read (design D15), and the shared interfaces of design
// D4 that live outside the lef, def and oasis areas (extflat::read_flat,
// extract::StyleView). Owners replace these [m6-scaffold] tests with
// [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/mask/stream_read.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace extflat = hefesto::extflat;
namespace extract = hefesto::extract;
namespace gds = hefesto::gds;
namespace stream = hefesto::stream;
namespace tech = hefesto::tech;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
  }

  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, cmd::Origin::script)) {
      last = std::move(r);
    }
    return last;
  }

  std::string value(std::string_view line) {
    const auto result = run(line);
    REQUIRE(result.ok());
    return result.value.to_string();
  }
};

}  // namespace

// --- tasks.md 2.4: the StreamReadSettings split (design D15) ----------------

TEST_CASE("the shared read options live in MaskSession::stream_read", "[m6-scaffold]") {
  Session s;
  CHECK(s.state.mask.stream_read == stream::StreamReadSettings{});
  REQUIRE(s.run("gds readonly yes").ok());
  REQUIRE(s.run("gds flatten 7").ok());
  REQUIRE(s.run("gds ordering yes").ok());
  REQUIRE(s.run("gds flatglob none").ok());
  REQUIRE(s.run("gds polygon subcells keep").ok());
  CHECK(s.state.mask.stream_read.readonly);
  CHECK(s.state.mask.stream_read.flatten);
  CHECK(s.state.mask.stream_read.flatten_limit == 7);
  CHECK(s.state.mask.stream_read.ordering);
  CHECK(s.state.mask.stream_read.flatglob.empty());
  CHECK(s.state.mask.stream_read.polygon_subcells == stream::PolygonSubcells::keep);
  // The gds-only settings are untouched.
  CHECK(s.state.mask.gds == gds::GdsSettings{});
  // The queries answer as before the split.
  CHECK(s.value("gds readonly") == "1");
  CHECK(s.value("gds flatten") == "1");
  CHECK(s.value("gds ordering") == "1");
  CHECK(s.value("gds polygon subcells") == "keep");
}

TEST_CASE("the gds-only settings stay in GdsSettings", "[m6-scaffold]") {
  Session s;
  REQUIRE(s.run("gds magscale 2").ok());
  REQUIRE(s.run("gds datestamp 1234").ok());
  CHECK(s.state.mask.gds.magscale == 2.0);
  CHECK(s.state.mask.gds.datestamp == 1234);
  CHECK(s.state.mask.stream_read == stream::StreamReadSettings{});
}

// --- tasks.md 2.5: shared interfaces outside the lef/def/oasis areas --------

TEST_CASE("extflat::read_flat is a stub", "[m6-scaffold]") {
  Session s;
  diag::CollectingSink sink;
  hefesto::cell::LookupContext ctx{s.services.view().files, s.state.search_paths, s.state.path_variables, sink};
  const auto netlist = extflat::read_flat(ctx, "top", extflat::ReadOptions{});
  REQUIRE_FALSE(netlist.ok());
  CHECK(netlist.error().code == "CORE-NOTIMPL-006");

  const extflat::Netlist built{{{"top/a", "metal1", {1, 2}, false}, {"GND!", "pwell", {0, 0}, true}},
                               {{"top", 42}}};
  std::vector<std::string> names;
  built.visit_nodes([&names](const extflat::FlatNode& node) { names.push_back(node.name); });
  CHECK(names == std::vector<std::string>{"top/a", "GND!"});
  CHECK(built.stamps().front().timestamp == 42);
}

TEST_CASE("extract::StyleView answers nullopt without a style", "[m6-scaffold]") {
  diag::CollectingSink sink;
  const tech::Technology technology;
  const extract::StyleView none = extract::StyleView::from_section(tech::RawSection{"extract", {}}, technology, sink);
  CHECK_FALSE(none.has_style());
  CHECK_FALSE(none.gate_types().has_value());
  CHECK_FALSE(none.diff_types().has_value());
  CHECK_FALSE(none.active_types().has_value());
  CHECK_FALSE(none.substrate_types().has_value());

  extract::StyleTypes types;
  types.style = "default";
  types.gate = tech::TypeMask::of(10);
  types.diff = tech::TypeMask::of(11);
  const extract::StyleView view{types};
  REQUIRE(view.has_style());
  CHECK(view.gate_types()->test(10));
  CHECK(view.diff_types()->test(11));
  CHECK(view.active_types()->empty());
  CHECK(view.substrate_types()->empty());
}
