// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M6 scaffold (openspec/changes/m6-lef-def-oasis tasks.md 2.4, 2.5): the
// shared read settings split out of gds::GdsSettings into
// MaskSession::stream_read (design D15), and the shared interfaces of design
// D4: the session state (EditorState::lefdef, MaskSession::oasis), the
// LEF/DEF and OASIS core types and entry points, extflat::read_flat and
// extract::StyleView. Owners replace these [m6-scaffold] tests with
// [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/def/read/read.hpp"
#include "hefesto/def/write/write.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/gds/command/settings.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/lex/lexer.hpp"
#include "hefesto/lef/read/read.hpp"
#include "hefesto/lef/session.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/generate.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "hefesto/lef/write/write.hpp"
#include "hefesto/mask/stream_read.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/encoder.hpp"
#include "hefesto/oasis/codec/modal.hpp"
#include "hefesto/oasis/read/read.hpp"
#include "hefesto/oasis/write/write.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace def = hefesto::def;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace extflat = hefesto::extflat;
namespace extract = hefesto::extract;
namespace gds = hefesto::gds;
namespace lef = hefesto::lef;
namespace oasis = hefesto::oasis;
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

// EXTSUB landed read_flat and StyleView (their [cap:ext-flattening-*] and
// [cap:extract-tech-section] tests are in tests/unit/extflat and
// tests/unit/extract/tech); these keep the scaffold's interface checks.
TEST_CASE("extflat::read_flat reports a missing root and Netlist visits its nodes", "[m6-scaffold]") {
  Session s;
  diag::CollectingSink sink;
  hefesto::cell::LookupContext ctx{s.services.view().files, s.state.search_paths, s.state.path_variables, sink};
  const auto netlist = extflat::read_flat(ctx, "top", extflat::ReadOptions{});
  REQUIRE_FALSE(netlist.ok());
  CHECK(netlist.error().code == "EXTFLAT-READ-001");

  const extflat::Netlist built{
      {{"top/a", "metal1", {1, 2}, false, false, false, {}, {}}, {"GND!", "pwell", {0, 0}, true, false, false, {}, {}}},
      {{"top", 42}}};
  std::vector<std::string> names;
  built.visit_nodes([&names](const extflat::FlatNode& node) { names.push_back(node.name); });
  CHECK(names == std::vector<std::string>{"top/a", "GND!"});
  CHECK(built.stamps().front().timestamp == 42);
}

TEST_CASE("extract::StyleView answers nullopt without a style", "[m6-scaffold]") {
  diag::CollectingSink sink;
  const tech::Technology technology;
  // An empty extract section has Magic's "default" style.
  CHECK(extract::StyleView::from_section(tech::RawSection{"extract", {}}, technology, sink).has_style());
  const extract::StyleView none;
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

// --- tasks.md 2.5: the session state and the LEF/DEF and OASIS interfaces ---

namespace {

void check_stub(const cmd::CommandResult& result, std::string_view what) {
  INFO(what);
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == "CORE-NOTIMPL-006");
  CHECK(result.diagnostics.front().text == "not implemented yet: " + std::string{what});
}

template <typename T>
void check_stub(const diag::Outcome<T>& outcome) {
  REQUIRE_FALSE(outcome.ok());
  CHECK(outcome.error().code == "CORE-NOTIMPL-006");
}

}  // namespace

TEST_CASE("EditorState holds the LEF/DEF session and MaskSession the oasis settings", "[m6-scaffold]") {
  const editor::EditorState state;
  CHECK(state.lefdef.layers.size() == 0);
  CHECK(state.lefdef.vias.size() == 0);
  CHECK(state.lefdef.rules.size() == 0);
  CHECK_FALSE(state.lefdef.datestamp.has_value());
  CHECK(state.lefdef.nocheck.empty());
  CHECK(state.lefdef.units == lef::LefUnits{});
  CHECK(state.mask.oasis == oasis::OasisSettings{});
}

TEST_CASE("the LEF/DEF and OASIS entry points answer CORE-NOTIMPL-006", "[m6-scaffold]") {
  editor::EditorState state;
  hefesto::db::CellDef cell{"top", 1};
  // lef::read is implemented (LEFR): tests/unit/lef/read/.
  check_stub(lef::write(state, cell, "a.lef", lef::LefWriteOptions{}), "lef write");
  check_stub(lef::write_all(state, cell, "a.lef", lef::LefWriteOptions{}), "lef writeall");
  check_stub(def::read(state, "a.def", def::DefReadOptions{}), "def read");
  check_stub(def::write(state, cell, "a.def", def::DefWriteOptions{}), "def write");
  check_stub(oasis::read(state, "a.oas", oasis::OasisReadOptions{}), "oasis read");
  check_stub(oasis::write(state, cell, "a.oas", oasis::OasisWriteOptions{}), "oasis write");
  // lef::paint_polygon is implemented (LEFVIA): tests/unit/lef/via/.
}

TEST_CASE("the LEF/DEF core interfaces are stubs", "[m6-scaffold]") {
  diag::CollectingSink sink;
  const tech::Technology technology;
  const lef::LayerTable table = lef::LayerTable::from_section(tech::RawSection{"lef", {}}, technology, sink);
  CHECK(table.size() == 0);
  CHECK(table.find("metal1") == nullptr);
  CHECK(table.magic_to_lef(1) == nullptr);
  int visited = 0;
  table.enumerate([&visited](std::string_view, const lef::LayerRecord&) { ++visited; });
  CHECK(visited == 0);

  const std::vector<std::byte> bytes(4, std::byte{0});
  gds::MemorySource source{bytes};
  lef::Lexer lexer{source};
  CHECK(&lexer.source() == &source);
  // Implemented by LEFR (tests/unit/lef/lex/): four NULs are one empty line.
  const auto none = lexer.next();
  CHECK((none.ok() && !none.value()));
  CHECK(lexer.line() == 1);
  CHECK(lexer.estimate() == 0.0);

  lef::MessageCounter counter;
  CHECK(counter.count(diag::Severity::error));
  CHECK(counter.count(diag::Severity::warning));
  CHECK(counter.count(diag::Severity::warning));
  CHECK(counter.errors() == 1);
  CHECK(counter.warnings() == 2);
}

TEST_CASE("the OASIS codec interfaces keep their scaffold shape", "[m6-scaffold]") {
  // The codec is implemented (OASFMT); its behaviour is tested in
  // tests/unit/oasis/codec/. Only the interface shape is checked here.
  const std::vector<std::byte> bytes(4, std::byte{0});
  gds::MemorySource source{bytes};
  oasis::Decoder decoder{source};
  CHECK(&decoder.source() == &source);
  CHECK_FALSE(decoder.next().ok());  // not an OASIS file
  CHECK(decoder.modal() == oasis::ModalState{});
  CHECK(decoder.names().cellnames.empty());

  gds::MemorySink sink;
  oasis::Encoder encoder{sink};
  CHECK(&encoder.sink() == &sink);
  CHECK(encoder.options().compress == 6);

  CHECK(oasis::expand(oasis::Repetition{}) == std::vector<oasis::Point64>{oasis::Point64{}});

  oasis::ModalState modal;
  modal.layer = 3;
  modal.xy_absolute = false;
  modal.reset();
  CHECK(modal == oasis::ModalState{});
}

TEST_CASE(
    "to_gds_write_options takes the shared write options from the oasis "
    "settings",
    "[cap:oasis-command][req:Settings shared with the gds command]") {
  oasis::OasisSettings settings;
  settings.allow_abstract = true;
  settings.addendum = true;
  settings.arrays = true;
  settings.contacts = true;
  settings.labels = false;
  settings.library = true;
  settings.lower = false;
  settings.merge = true;
  settings.allow_undefined = true;
  settings.datestamp = 77;
  settings.magscale = 2.0;
  const gds::GdsWriteOptions options = oasis::to_gds_write_options(settings, stream::StreamReadSettings{});
  const gds::GdsSettings& g = options.settings;
  CHECK(g.allow_abstract);
  CHECK(g.addendum);
  CHECK(g.arrays);
  CHECK(g.contacts);
  CHECK_FALSE(g.labels);
  CHECK(g.library);
  CHECK_FALSE(g.lower);
  CHECK(g.merge);
  CHECK(g.allow_undefined);
  // datestamp is read-side only for OASIS: never a GDS write date.
  CHECK_FALSE(g.datestamp.has_value());
  CHECK(g.magscale == 2.0);
  // OASIS compresses with CBLOCKs, never with GDS gzip (`oasis compress` is 6).
  CHECK(g.compress == 0);
}
