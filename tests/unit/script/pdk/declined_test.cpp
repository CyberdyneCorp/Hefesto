// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The declined path of hefesto-security "Consent before implicit script
// execution" (tasks.md 6.2/6.6; board M4-Script-Consent tile 6): a layout
// whose PDK scripts were declined opens read-only with the `minimum`
// technology, also when another technology was loaded; nothing writes it
// (save, save as, after `cellname writeable`, `load -force`); Review
// scripts... (cell::reopen_declined) runs the script and reads the cells
// again, editable. Magic has no consent: the read-only behaviour follows
// Magic's read-only cells and is the same with magic-compat ON and OFF.
// Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

#include "hefesto/cell/declined.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/tech/commands.hpp"
#include "hefesto/tech/loader.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
namespace cmd = hefesto::cmd;
using script::ConsentDecision;
using script::test::PdkSession;
using script::test::TempDir;
using script::test::write_file;

namespace {

constexpr const char* kTag = "[cap:hefesto-security][req:Consent before implicit script execution]";

std::string tiny_tech_text() {
  std::ifstream tech{std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech"};
  return std::string{std::istreambuf_iterator<char>{tech}, {}};
}

// A PDK `tiny` found by technology auto-discovery under $PDK_ROOT, and a
// layout of it in work/.
struct Declined {
  PdkSession s;
  TempDir dir;
  hefesto::platform::Path magic_dir = dir.path() / "pdk/tiny/libs.tech/magic";
  script::TestConsentHost host;  // declines unless answers are pushed

  explicit Declined(bool magic_compat = true) {
    s.project.compat.magic_compat = magic_compat;
    write_file(magic_dir / "tiny.tech", tiny_tech_text());
    write_file(magic_dir / "tiny.magicrc", "set ::tiny_rc [info script]\n");
    write_file(dir.path() / "work/inv.mag",
               "magic\ntech tiny\ntimestamp 0\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
    const std::string root = (dir.path() / "pdk").string();
    s.editor.path_variables.variable = [root](std::string_view name) -> std::optional<std::string> {
      return name == "PDK_ROOT" ? std::optional<std::string>{root} : std::nullopt;
    };
    s.editor.script.pdk_trust.set_consent_host(&host);
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    return s.dispatcher->execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }
  cmd::CommandResult load(const std::string& cell, std::vector<std::string> extra = {}) {
    std::vector<std::string> args{(dir.path() / "work" / cell).string()};
    args.insert(args.end(), extra.begin(), extra.end());
    return run("load", std::move(args));
  }
  [[nodiscard]] hefesto::db::CellDef& cell(std::string_view name) const {
    auto* def = s.editor.cells.find(name);
    REQUIRE(def != nullptr);
    return *def;
  }
  [[nodiscard]] std::string file_text(const std::string& name) const {
    std::ifstream in{dir.path() / "work" / name};
    return std::string{std::istreambuf_iterator<char>{in}, {}};
  }

  cmd::CommandResult reopen() {
    hefesto::diag::CollectingSink sink;
    cmd::CommandContext ctx(s.services.view(), s.project, s.script_session, sink, s.registry, &s.editor);
    return hefesto::cell::reopen_declined(ctx, {"tiny", "tiny", ""}, magic_dir / "tiny.magicrc");
  }
};

bool has_code(const cmd::CommandResult& result, std::string_view code) {
  for (const auto& d : result.diagnostics) {
    if (d.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a declined layout switches a loaded technology back to minimum", kTag) {
  // The TECH entry point (tech::load_builtin_technology): another
  // technology loaded without paint is replaced by `minimum`.
  const bool compat = GENERATE(true, false);
  Declined d{compat};
  std::string other = tiny_tech_text();
  other.replace(other.find("    tiny\n"), 9, "    other\n");
  write_file(d.dir.path() / "other.tech", other);
  REQUIRE(d.run("tech", {"load", (d.dir.path() / "other.tech").string()}).ok());
  REQUIRE(d.s.editor.technology->name() == "other");

  CHECK(d.load("inv").ok());
  CHECK(d.s.editor.technology->name() == "minimum");
  CHECK(d.s.editor.technology == hefesto::tech::builtin_technology());
  auto& inv = d.cell("inv");
  CHECK(inv.flags().no_edit);
  CHECK(hefesto::cell::is_declined(d.s.editor, inv));
  CHECK(d.s.editor.edit_cell == nullptr);
  CHECK(d.host.requests().size() == 1);
}

TEST_CASE("load_builtin_technology keeps a current minimum", "[cap:tech-loading-and-scaling][req:Built-in technologies]") {
  PdkSession s;
  hefesto::diag::CollectingSink sink;
  const auto before = s.editor.technology;
  auto services = s.services.view();
  REQUIRE(hefesto::tech::load_builtin_technology(s.editor, services, {}, sink).ok());
  CHECK(s.editor.technology == before);
  CHECK(sink.snapshot().empty());
}

TEST_CASE("a declined layout cannot be edited or written", kTag) {
  const bool compat = GENERATE(true, false);
  Declined d{compat};
  REQUIRE(d.load("inv").ok());
  auto& inv = d.cell("inv");
  REQUIRE(hefesto::cell::is_declined(d.s.editor, inv));
  const std::string original = d.file_text("inv.mag");

  // Editing commands find no edit cell; `edit` refuses the read-only cell.
  d.s.editor.box = hefesto::db::Rect{0, 0, 2, 2};
  CHECK_FALSE(d.run("paint", {"m1"}).ok());
  CHECK_FALSE(d.run("edit").ok());
  CHECK(d.s.editor.edit_cell == nullptr);

  // save, save as and writeall do not write it.
  const auto saved = d.run("save");
  CHECK_FALSE(saved.ok());
  CHECK(has_code(saved, "SCRIPT-TRUST-005"));
  const auto saved_as = d.run("save", {(d.dir.path() / "work/copy").string()});
  CHECK(has_code(saved_as, "SCRIPT-TRUST-005"));
  CHECK_FALSE(std::filesystem::exists(d.dir.path() / "work/copy.mag"));
  CHECK(inv.flags().no_edit);  // save as does not make it writeable (Q-CELL-37 is for writeable files)
  (void)d.run("writeall", {"force"});
  CHECK(d.file_text("inv.mag") == original);

  // Regression: `cellname writeable` clears no_edit, but the cell is still
  // never written (it holds only what `minimum` could read).
  (void)d.run("cellname", {"writeable", "inv", "true"});
  const auto forced = d.run("save", {"inv"});
  CHECK(has_code(forced, "SCRIPT-TRUST-005"));
  CHECK(d.file_text("inv.mag") == original);
}

TEST_CASE("a declined technology read with the technology ignored stays read-only", kTag) {
  // Regression: `load -force` (and subcell expansion, `flush`) read the cell
  // with the technology ignored; with the PDK declined it must not become an
  // editable `minimum` cell.
  const bool compat = GENERATE(true, false);
  Declined d{compat};
  REQUIRE(d.load("inv").ok());
  write_file(d.dir.path() / "work/nand.mag", "magic\ntech tiny\ntimestamp 0\n<< end >>\n");
  CHECK(d.load("nand", {"-force"}).ok());
  auto& nand = d.cell("nand");
  CHECK(nand.flags().no_edit);
  CHECK(hefesto::cell::is_declined(d.s.editor, nand));
  CHECK(d.s.editor.edit_cell == nullptr);
  REQUIRE(d.run("flush", {"inv"}).ok());
  CHECK(hefesto::cell::is_declined(d.s.editor, d.cell("inv")));
  CHECK(d.host.requests().size() == 1);  // asked once
}

TEST_CASE("Review scripts reads the declined layout again with its technology", kTag) {
  const bool compat = GENERATE(true, false);
  Declined d{compat};
  REQUIRE(d.load("inv").ok());
  REQUIRE(d.s.editor.technology->name() == "minimum");
  // The answer of the second prompt (Review scripts...): run once. The rc
  // file does not load the technology, so auto-discovery does, running it
  // again with the same answer.
  d.host.push_answer(ConsentDecision::run_once);
  d.host.push_answer(ConsentDecision::run_once);
  const auto reopened = d.reopen();
  INFO((reopened.diagnostics.empty() ? std::string{} : reopened.diagnostics.front().text));
  CHECK(reopened.ok());
  CHECK(d.s.editor.technology->name() == "tiny");
  CHECK(d.s.eval("info exists ::tiny_rc") == "1");
  auto& inv = d.cell("inv");
  CHECK_FALSE(inv.flags().no_edit);
  CHECK_FALSE(hefesto::cell::is_declined(d.s.editor, inv));
  CHECK(d.s.editor.edit_cell == &inv);
  CHECK(d.s.eval("cellname list window") == "inv");
  CHECK(d.s.editor.cell_session.declined.empty());
  CHECK_FALSE(d.s.editor.script.pdk_trust.declined_variant("tiny"));
  // The metal1 paint the minimum technology could not read is back, and the
  // cell can be edited and saved.
  CHECK_FALSE(inv.bbox().is_null());
  CHECK(d.run("save").ok());
}

TEST_CASE("Review scripts declined again keeps the layout read-only", kTag) {
  Declined d;
  REQUIRE(d.load("inv").ok());
  const auto reopened = d.reopen();  // the host declines again
  CHECK_FALSE(reopened.ok());
  CHECK(has_code(reopened, "SCRIPT-TRUST-001"));
  CHECK(d.s.editor.technology->name() == "minimum");
  CHECK(hefesto::cell::is_declined(d.s.editor, d.cell("inv")));
  CHECK(d.s.editor.script.pdk_trust.declined_variant("tiny"));
}

TEST_CASE("Review scripts whose technology cannot be loaded keeps the layout read-only", kTag) {
  Declined d;
  REQUIRE(d.load("inv").ok());
  std::filesystem::remove(d.magic_dir / "tiny.tech");
  d.host.push_answer(ConsentDecision::run_once);
  d.host.push_answer(ConsentDecision::run_once);
  const auto reopened = d.reopen();
  CHECK_FALSE(reopened.ok());
  CHECK(has_code(reopened, "SCRIPT-TRUST-006"));
  CHECK(hefesto::cell::is_declined(d.s.editor, d.cell("inv")));
  CHECK(d.cell("inv").flags().no_edit);
  CHECK(d.s.editor.script.pdk_trust.declined_variant("tiny"));
}

TEST_CASE("a declined record holds only for the definition read from that file", kTag) {
  Declined d;
  REQUIRE(d.load("inv").ok());
  auto& inv = d.cell("inv");
  REQUIRE(d.s.editor.cell_session.declined.contains(inv));
  inv.set_file_path(d.dir.path() / "elsewhere/inv");
  CHECK_FALSE(d.s.editor.cell_session.declined.contains(inv));
}
