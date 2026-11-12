// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-security "Consent before implicit script execution" and the trust
// store (tasks.md 6.2): trust per PDK and version, recorded in the PDK
// store; the batch (CLI) policy; the implicit `<tech>.magicrc` of
// technology auto-discovery (tasks.md 6.6). Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/pdk/catalog.hpp"
#include "hefesto/pdk/manager.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/tech_rc.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
using script::ConsentDecision;
using script::PdkIdentity;
using script::test::PdkSession;
using script::test::TempDir;
using script::test::write_file;

namespace {

const PdkIdentity kSky130{"sky130", "sky130A", "5740c4a81c42f9fe4af5ef2ee258a99151053d28"};
const PdkIdentity kSky130Old{"sky130", "sky130A", "0fe599b2afb6708d281543108caf8310912f54af"};

// A backend that remembers what was recorded.
class MemoryBackend final : public script::TrustBackend {
 public:
  bool trusted(const PdkIdentity& pdk) const override { return recorded.contains(pdk); }
  hefesto::diag::Status record(const PdkIdentity& pdk) override {
    recorded.insert(pdk);
    return hefesto::diag::Done{};
  }
  hefesto::diag::Status forget(const PdkIdentity& pdk) override {
    recorded.erase(pdk);
    return hefesto::diag::Done{};
  }
  std::set<PdkIdentity> recorded;
};

class Recorder final : public script::ScriptObserver {
 public:
  void on_consent_requested(const script::ConsentRequest& request) override { requests.push_back(request); }
  std::vector<script::ConsentRequest> requests;
};

// A host with no environment, for the PDK manager.
class NoHost final : public hefesto::pdk::InstallHost {
 public:
  hefesto::diag::Status make_symlink(const hefesto::platform::Path&, const hefesto::platform::Path&) override {
    return hefesto::diag::Done{};
  }
  hefesto::diag::Outcome<std::optional<hefesto::platform::Path>> read_symlink(const hefesto::platform::Path&) override {
    return std::optional<hefesto::platform::Path>{};
  }
  std::optional<std::string> environment(std::string_view) override { return std::nullopt; }
  hefesto::platform::Path home_directory() override { return {}; }
};

}  // namespace

TEST_CASE("trust is recorded per PDK and version", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::TrustStore store;
  script::ScriptEvents events;
  script::TestConsentHost host;
  host.push_answer(ConsentDecision::trust_version);
  const auto first = script::request_consent(store, events, &host, kSky130, "/pdk/sky130A.magicrc", false);
  CHECK(first.run);
  REQUIRE(first.diagnostics.size() == 1);
  CHECK(first.diagnostics.front().code == "SCRIPT-TRUST-002");
  CHECK(store.trusted(kSky130));
  CHECK_FALSE(store.trusted(kSky130Old));  // another version is not trusted

  // Trusted: no second question.
  CHECK(script::request_consent(store, events, &host, kSky130, "/pdk/sky130A.magicrc", false).run);
  CHECK(host.requests().size() == 1);
  // Another version asks again (and the host's fallback declines).
  const auto old = script::request_consent(store, events, &host, kSky130Old, "/pdk/sky130A.magicrc", false);
  CHECK_FALSE(old.run);
  CHECK(host.requests().size() == 2);
}

TEST_CASE("a declined version is asked once per session", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::TrustStore store;
  script::ScriptEvents events;
  Recorder recorder;
  events.add(recorder);
  script::TestConsentHost host;  // declines, then would run
  host.push_answer(ConsentDecision::decline);
  host.push_answer(ConsentDecision::run_once);
  CHECK_FALSE(script::request_consent(store, events, &host, kSky130, "/x/sky130A.magicrc", false).run);
  CHECK(store.declined(kSky130));
  CHECK(store.declined_variant("sky130A"));
  CHECK_FALSE(store.declined_variant("sky130B"));
  // The board's prompt is shown once per PDK version: the decline holds.
  const auto again = script::request_consent(store, events, &host, kSky130, "/x/sky130A.magicrc", true);
  CHECK_FALSE(again.run);
  REQUIRE(again.diagnostics.size() == 1);
  CHECK(again.diagnostics.front().code == "SCRIPT-TRUST-001");
  CHECK(host.requests().size() == 1);
  CHECK(recorder.requests.size() == 1);
  // "Review scripts..." forgets the decline and asks again.
  store.forget_decline(kSky130);
  CHECK(script::request_consent(store, events, &host, kSky130, "/x/sky130A.magicrc", true).run);
  CHECK(host.requests().size() == 2);
  events.remove(recorder);
}

TEST_CASE("run once does not trust the version", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::TrustStore store;
  script::ScriptEvents events;
  Recorder recorder;
  events.add(recorder);
  script::TestConsentHost host{ConsentDecision::run_once};
  CHECK(script::request_consent(store, events, &host, kSky130, "/x/sky130A.magicrc", true).run);
  CHECK(script::request_consent(store, events, &host, kSky130, "/x/sky130A.magicrc", true).run);
  CHECK(host.requests().size() == 2);
  CHECK_FALSE(store.trusted(kSky130));
  REQUIRE(recorder.requests.size() == 2);
  CHECK(recorder.requests.front() == script::ConsentRequest{kSky130, "/x/sky130A.magicrc", true});
  events.remove(recorder);
}

TEST_CASE("no consent host declines", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::TrustStore store;
  script::ScriptEvents events;
  const auto outcome = script::request_consent(store, events, nullptr, kSky130, "/x/sky130A.magicrc", true);
  CHECK_FALSE(outcome.run);
  REQUIRE(outcome.diagnostics.size() == 1);
  CHECK(outcome.diagnostics.front().code == "SCRIPT-TRUST-001");
  CHECK(outcome.diagnostics.front().text.find("read-only with the minimum technology") != std::string::npos);
  // The store's own host is used when the caller passes none (after the
  // decline is forgotten: a declined version is not asked again).
  script::TestConsentHost host{ConsentDecision::run_once};
  store.set_consent_host(&host);
  store.forget_decline(kSky130);
  CHECK(script::request_consent(store, events, nullptr, kSky130, "/x/sky130A.magicrc", true).run);
}

TEST_CASE("trusting a version records it in the backend", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::TrustStore store;
  auto backend = std::make_unique<MemoryBackend>();
  MemoryBackend& memory = *backend;
  store.set_backend(std::move(backend));
  script::ScriptEvents events;
  script::TestConsentHost host;
  host.push_answer(ConsentDecision::trust_version);
  REQUIRE(script::request_consent(store, events, &host, kSky130, "/x", false).run);
  CHECK(memory.recorded.contains(kSky130));
  store.revoke(kSky130);
  CHECK(store.trusted(kSky130));  // the backend still trusts it (a later session)
}

TEST_CASE("the PDK store records script trust in the version's provenance",
          "[cap:hefesto-pdk-manager][req:Script trust for managed PDKs]") {
  TempDir dir;
  write_file(dir.path() / "ciel/sky130/versions" / kSky130.version / "sky130A/libs.tech/magic/sky130A.tech", "tech\n");
  hefesto::platform::LocalFileSystem fs;
  NoHost host;
  auto backend = script::make_pdk_store_trust(fs, host, dir.path());
  CHECK_FALSE(backend->trusted(kSky130));  // an external (ciel) install asks first
  REQUIRE(backend->record(kSky130).ok());
  CHECK(backend->trusted(kSky130));
  CHECK_FALSE(backend->trusted(kSky130Old));
  CHECK_FALSE(backend->record(kSky130Old).ok());  // not installed

  // The record is the provenance file, readable by a new manager.
  auto fetcher = hefesto::pdk::make_unsupported_fetcher();
  hefesto::pdk::PdkManager manager{fs, host, *fetcher, hefesto::pdk::Catalog::builtin()};
  manager.set_root(dir.path());
  const auto* family = manager.catalog().find_family("sky130");
  REQUIRE(family != nullptr);
  const auto provenance = manager.provenance(*family, kSky130.version);
  REQUIRE(provenance.ok());
  CHECK(provenance.value().trusted);
  CHECK(provenance.value().source == "external");
  REQUIRE(manager.set_trusted(*family, kSky130.version, false).ok());
  CHECK_FALSE(manager.provenance(*family, kSky130.version).value().trusted);
}

TEST_CASE("the batch policy runs only the PDKs named by --trust-pdk",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  script::BatchConsentHost none{{}};
  CHECK(none.ask(kSky130, "/x") == ConsentDecision::decline);
  for (const char* selector : {"sky130", "sky130A", "sky130A@5740c4a8", "sky130/sky130A@5740c4a81c42f9fe4af5ef2ee258a99151053d28"}) {
    INFO(selector);
    script::BatchConsentHost cli{{selector}};
    CHECK(cli.ask(kSky130, "/x") == ConsentDecision::run_once);
  }
  script::BatchConsentHost other{{"gf180mcuD", "sky130A@0fe5", "sky130B"}};
  CHECK(other.ask(kSky130, "/x") == ConsentDecision::decline);
  CHECK(other.ask(kSky130Old, "/x") == ConsentDecision::run_once);
}

TEST_CASE("the PDK of a script comes from its path", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  CHECK(script::pdk_identity_of("/r/ciel/sky130/versions/abc/sky130A/libs.tech/magic/sky130A.magicrc") ==
        PdkIdentity{"sky130", "sky130A", "abc"});
  CHECK(script::pdk_identity_of("ihp-sg13/versions/5e6d/ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl") ==
        PdkIdentity{"ihp-sg13", "ihp-sg13g2", "5e6d"});
  // A plain open_pdks install: family from the catalog, unknown version.
  CHECK(script::pdk_identity_of("/usr/local/share/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.tcl") ==
        PdkIdentity{"gf180mcu", "gf180mcuD", ""});
  CHECK_FALSE(script::pdk_identity_of("/home/u/.magicrc").has_value());
  CHECK(script::pdk_root_for("/r/ciel/sky130/versions/abc/sky130A") == hefesto::platform::Path{"/r/ciel/sky130/versions/abc"});
}

// ---- the implicit <tech>.magicrc (Technology auto-discovery) --------------------

namespace {

struct Discovery {
  PdkSession s;
  TempDir dir;
  hefesto::platform::Path magic_dir = dir.path() / "pdk/tiny/libs.tech/magic";

  Discovery() {
    std::ifstream tech{std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech"};
    write_file(magic_dir / "tiny.tech", std::string{std::istreambuf_iterator<char>{tech}, {}});
    write_file(magic_dir / "tiny.magicrc", "set ::tiny_rc [info script]\n");
    write_file(dir.path() / "work/inv.mag", "magic\ntech tiny\ntimestamp 0\n<< metal1 >>\nrect 0 0 4 4\n<< properties >>\nstring FIXED_BBOX 0 0 4 4\n<< end >>\n");
    const std::string root = (dir.path() / "pdk").string();
    s.editor.path_variables.variable = [root](std::string_view name) -> std::optional<std::string> {
      return name == "PDK_ROOT" ? std::optional<std::string>{root} : std::nullopt;
    };
  }

  hefesto::cmd::CommandResult load() {
    return s.dispatcher->execute(hefesto::cmd::Invocation{
        "load", {(dir.path() / "work/inv").string()}, hefesto::cmd::Origin::script, std::nullopt});
  }
};

bool has_code(const hefesto::cmd::CommandResult& result, std::string_view code) {
  for (const auto& d : result.diagnostics) {
    if (d.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Unknown PDK script", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  Discovery d;
  script::TestConsentHost host;  // declines
  d.s.editor.script.pdk_trust.set_consent_host(&host);
  const auto loaded = d.load();
  CHECK(loaded.ok());
  CHECK(d.s.editor.technology->name() == "minimum");
  CHECK(d.s.eval("info exists ::tiny_rc") == "0");
  REQUIRE(host.requests().size() == 1);
  CHECK(host.requests().front().script == (d.magic_dir / "tiny.magicrc").string());
  CHECK(host.requests().front().pdk.variant == "tiny");
  // The file opens read-only with the minimum technology (which has no
  // layers, so the paint is not drawn; the rest of the file is read): the
  // cell is shown but is not the edit cell.
  auto* inv = d.s.editor.cells.find("inv");
  REQUIRE(inv != nullptr);
  CHECK(inv->flags().available);
  CHECK(inv->flags().no_edit);
  CHECK(d.s.editor.edit_cell == nullptr);
  CHECK(d.s.eval("cellname list window") == "inv");
  CHECK(inv->properties().find("FIXED_BBOX") != nullptr);
  CHECK(has_code(loaded, "SCRIPT-TRUST-001"));
  CHECK_FALSE(d.s.dispatcher->execute({"paint", {"m1"}, hefesto::cmd::Origin::script, std::nullopt}).ok());

  // Another cell of the declined PDK opens read-only without a second prompt.
  write_file(d.dir.path() / "work/nand.mag", "magic\ntech tiny\ntimestamp 0\n<< end >>\n");
  (void)d.s.dispatcher->execute(hefesto::cmd::Invocation{
      "load", {(d.dir.path() / "work/nand").string()}, hefesto::cmd::Origin::script, std::nullopt});
  CHECK(host.requests().size() == 1);
  REQUIRE(d.s.editor.cells.find("nand") != nullptr);
  CHECK(d.s.editor.cells.find("nand")->flags().no_edit);
}

TEST_CASE("a declined pinned PDK opens the project's cells read-only",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  // The startup's decline of the project's pinned rc file (run_startup_file)
  // is remembered by the trust store; a cell of that PDK found nowhere on
  // the discovery path still opens read-only instead of as a new cell.
  Discovery d;
  d.s.editor.path_variables.variable = [](std::string_view) { return std::optional<std::string>{}; };
  d.s.editor.script.pdk_trust.decline({"tiny", "tiny", "abc"});
  const auto loaded = d.load();
  CHECK(loaded.ok());
  auto* inv = d.s.editor.cells.find("inv");
  REQUIRE(inv != nullptr);
  CHECK(inv->flags().no_edit);
  CHECK(d.s.editor.technology->name() == "minimum");
}

TEST_CASE("a cell of an undeclined technology that cannot be found keeps Magic's mismatch",
          "[cap:scripting-pdk-integration][req:Technology auto-discovery from layout files]") {
  Discovery d;
  d.s.editor.path_variables.variable = [](std::string_view) { return std::optional<std::string>{}; };
  (void)d.load();
  auto* inv = d.s.editor.cells.find("inv");
  REQUIRE(inv != nullptr);
  CHECK_FALSE(inv->flags().no_edit);  // DBWloadWindow's "Creating new cell"
  CHECK(d.s.editor.edit_cell == inv);
}

TEST_CASE("Opening a cell runs the technology's rc file after consent",
          "[cap:scripting-pdk-integration][req:Technology auto-discovery from layout files]") {
  Discovery d;
  script::TestConsentHost host{ConsentDecision::run_once};
  d.s.editor.script.pdk_trust.set_consent_host(&host);
  const auto loaded = d.load();
  INFO((loaded.diagnostics.empty() ? std::string{} : loaded.diagnostics.front().text));
  CHECK(loaded.ok());
  CHECK(d.s.editor.technology->name() == "tiny");
  CHECK(d.s.eval("set ::tiny_rc") == (d.magic_dir / "tiny.magicrc").string());
  REQUIRE(host.requests().size() == 1);
  CHECK(host.requests().front().implicit == false);  // TestConsentHost records the question only
  // The found directory joins the system path.
  const auto& system = d.s.editor.search_paths.system;
  CHECK(std::find(system.begin(), system.end(), d.magic_dir.generic_string()) != system.end());
}

TEST_CASE("run_tech_magicrc reports a declined script", "[cap:scripting-pdk-integration][req:Technology auto-discovery from layout files]") {
  PdkSession s;
  TempDir dir;
  write_file(dir.path() / "x.magicrc", "set ::x 1\n");
  script::TestConsentHost host;
  auto declined = script::run_tech_magicrc(s.editor, {"x", "x", ""}, dir.path() / "x.magicrc", &host);
  CHECK_FALSE(declined.ok());
  CHECK(declined.diagnostics.front().code == "SCRIPT-TRUST-001");
  s.editor.script.pdk_trust.forget_decline({"x", "x", ""});
  host.push_answer(ConsentDecision::run_once);
  write_file(dir.path() / "bad.magicrc", "set ::x 1\nfrobnicate\n");
  CHECK(script::run_tech_magicrc(s.editor, {"x", "x", ""}, dir.path() / "bad.magicrc", &host).ok());  // errors ignored
  CHECK(s.eval("set ::x") == "1");
  hefesto::editor::EditorState bare;
  CHECK(script::run_tech_magicrc(bare, {"x", "x", ""}, dir.path() / "x.magicrc", &host).diagnostics.front().code ==
        "SCRIPT-PDK-008");
}
