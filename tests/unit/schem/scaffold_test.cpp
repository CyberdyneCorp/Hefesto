// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M8 scaffold (openspec/changes/m8-schematic tasks.md 5.1, 5.2): the schem
// code area exists, EditorState carries a schem::Session, and the
// area-contract stubs answer their documented neutral values or
// CORE-NOTIMPL-007. Owners replace these [m8-scaffold] checks with
// [cap:][req:] tests as the stubs go away.
//
// Owner: SCAFFOLD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/schem/edit/operations.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/expr.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sim/deck.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"

namespace diag = hefesto::diag;
namespace schem = hefesto::schem;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-007";

bool is_not_implemented(const std::vector<diag::Diagnostic>& diagnostics) {
  return diagnostics.size() == 1 && diagnostics.front().code == kNotImplemented;
}

}  // namespace

TEST_CASE("CORE-NOTIMPL-007 and the schem code area are declared", "[m8-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(diag::codes::kSchemNotImplemented == kNotImplemented);
  const auto area = diag::find_code_area("schem");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "SCHEM");
  CHECK(area->doc == "docs/diagnostics/codes/schem.md");
}

TEST_CASE("EditorState carries an empty schematic session", "[m8-scaffold]") {
  const hefesto::editor::EditorState state;
  CHECK(state.schem.documents.empty());
  CHECK(state.schem.next_id == 1);
  CHECK(state.schem.symbols.size() == 0);
  CHECK(state.schem.symbols.placeholder().placeholder);
  CHECK(state.schem.search_path.directories().empty());
  CHECK_FALSE(state.schem.xschemrc.has_value());
}

TEST_CASE("a new document and the netlist options have their documented defaults", "[m8-scaffold]") {
  const schem::Document document;
  CHECK(document.kind == schem::FileKind::schematic);
  CHECK(document.file_version == "1.0");
  CHECK_FALSE(document.modified);
  CHECK(schem::kWriterVersion == "3.4.8RC");
  CHECK(schem::ReadOptions{}.xschem_compat);

  const schem::NetlistOptions options;
  CHECK(options.hierarchy == schem::Hierarchy::hier);
  CHECK_FALSE(options.lvs_netlist);
  CHECK_FALSE(options.top_is_subckt);
  CHECK_FALSE(options.lvs_ignore);
  CHECK(options.spiceprefix);
  CHECK(options.bus_replacement_char.empty());
  CHECK_FALSE(options.uppercase_subckt);
  CHECK(options.hiersep == ".");
  CHECK_FALSE(options.portable_paths);
}

TEST_CASE("the SYMLIB stubs answer their documented values", "[m8-scaffold]") {
  // pin_order, SymbolCache::resolve and SearchPath::from_xschemrc are
  // implemented (tasks 7.1-7.3); their tests are in tests/unit/schem/sym and
  // tests/unit/schem/lib.
  const schem::TclBridge bridge;
  CHECK(bridge.evaluate("tcleval(1)", {}) == "?\n");
}

TEST_CASE("the NETLIST stubs answer their documented values", "[m8-scaffold]") {
  const schem::Session session;
  const auto result = schem::netlist(session, schem::Document{}, {}, {});
  CHECK(result.failed);
  CHECK(result.text.empty());
  CHECK(is_not_implemented(result.diagnostics));
  // evaluate_expr and expand_bus are implemented: tests/unit/schem/net/.
}

TEST_CASE("the EDIT, CMD and SIMINT stubs answer their documented values", "[m8-scaffold]") {
  CHECK(schem::edit_operations().empty());

  hefesto::cmd::CommandRegistry registry;
  CHECK_FALSE(schem::register_schematic_command(registry).has_value());
  CHECK(registry.find("schematic") != nullptr);
  CHECK(schem::register_schematic_command(registry).has_value());  // duplicate

  const schem::Session session;
  const auto deck = schem::build_deck(session, schem::Document{});
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == kNotImplemented);
  CHECK(schem::vector_name(session, schem::HierPath{{"x1"}}, "net1") == "net1");
}
