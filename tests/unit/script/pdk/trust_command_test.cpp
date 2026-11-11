// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `pdk trust <variant> <version>` and `pdk untrust <variant> <version>`
// (board M4-Script-Consent note 5, approved 2026-11-11; tasks.md 6.2):
// script::run_pdk_trust, which the `pdk` command dispatches to. Tested
// through a stand-in command so the test does not depend on the `pdk`
// command's wiring (INTEGRATION). Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "hefesto/script/pdk/consent.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
using script::PdkIdentity;
using script::TrustAction;
using script::test::PdkSession;

namespace {

const PdkIdentity kSky130{"sky130", "sky130A", "5740c4a81c42f9fe4af5ef2ee258a99151053d28"};

class MemoryBackend final : public script::TrustBackend {
 public:
  bool trusted(const PdkIdentity& pdk) const override { return recorded.contains(pdk); }
  hefesto::diag::Status record(const PdkIdentity& pdk) override {
    if (fail) {
      return hefesto::diag::make("PDK-STORE-001", "read-only store");
    }
    recorded.insert(pdk);
    return hefesto::diag::Done{};
  }
  hefesto::diag::Status forget(const PdkIdentity& pdk) override {
    recorded.erase(pdk);
    return hefesto::diag::Done{};
  }
  std::set<PdkIdentity> recorded;
  bool fail = false;
};

// A session with `pdktrust`/`pdkuntrust` standing in for the `pdk`
// subcommands; `fallback` is the `pdk` command's own store.
struct TrustSession {
  PdkSession s;
  MemoryBackend fallback;

  TrustSession() {
    for (const auto& [name, action] : {std::pair{"pdktrust", TrustAction::trust}, std::pair{"pdkuntrust", TrustAction::untrust}}) {
      hefesto::cmd::CommandSpec spec{name, std::string{name} + " variant version", "test stand-in",
                                     [this, act = action](hefesto::cmd::CommandContext& ctx, const hefesto::cmd::Invocation& inv) {
                                       return script::run_pdk_trust(ctx, inv.args, act, &fallback);
                                     }};
      REQUIRE_FALSE(s.registry.add(std::move(spec)).has_value());
    }
  }

  hefesto::cmd::CommandResult run(const char* name, std::vector<std::string> args) {
    return s.dispatcher->execute(hefesto::cmd::Invocation{name, std::move(args), hefesto::cmd::Origin::gui, std::nullopt});
  }
};

}  // namespace

TEST_CASE("pdk trust records the version and pdk untrust removes it",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  TrustSession t;
  auto& store = t.s.editor.script.pdk_trust;
  const auto trusted = t.run("pdktrust", {"sky130A", kSky130.version});
  REQUIRE(trusted.ok());
  REQUIRE(trusted.diagnostics.size() == 1);
  CHECK(trusted.diagnostics.front().code == "SCRIPT-TRUST-002");
  CHECK(store.trusted(kSky130));  // the family comes from the catalog
  CHECK(t.fallback.recorded.contains(kSky130));

  // A trusted version runs without a prompt.
  script::TestConsentHost host;
  CHECK(script::request_consent(store, t.s.editor.script.events, &host, kSky130, "/x/sky130A.magicrc", false).run);
  CHECK(host.requests().empty());

  const auto untrusted = t.run("pdkuntrust", {"sky130A", kSky130.version});
  REQUIRE(untrusted.ok());
  CHECK(untrusted.diagnostics.front().code == "SCRIPT-TRUST-004");
  CHECK_FALSE(store.trusted(kSky130));
  CHECK_FALSE(t.fallback.recorded.contains(kSky130));
  CHECK_FALSE(script::request_consent(store, t.s.editor.script.events, &host, kSky130, "/x/sky130A.magicrc", false).run);
  CHECK(host.requests().size() == 1);  // asks again (and the test host declines)
}

TEST_CASE("pdk trust and untrust forget a decline", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  TrustSession t;
  auto& store = t.s.editor.script.pdk_trust;
  store.decline(kSky130);
  REQUIRE(t.run("pdktrust", {"sky130A", kSky130.version}).ok());
  CHECK_FALSE(store.declined(kSky130));
  store.decline(kSky130);
  REQUIRE(t.run("pdkuntrust", {"sky130A", kSky130.version}).ok());
  CHECK_FALSE(store.declined(kSky130));  // the next script of that version asks again
}

TEST_CASE("pdk trust prefers the session's backend and reports a record it cannot save",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  TrustSession t;
  auto session_backend = std::make_unique<MemoryBackend>();
  MemoryBackend& own = *session_backend;
  own.fail = true;
  t.s.editor.script.pdk_trust.set_backend(std::move(session_backend));
  const auto run = t.run("pdktrust", {"sky130A", kSky130.version});
  REQUIRE(run.ok());
  REQUIRE(run.diagnostics.size() == 2);
  CHECK(run.diagnostics[1].code == "SCRIPT-TRUST-003");
  CHECK(t.fallback.recorded.empty());
  CHECK(t.s.editor.script.pdk_trust.entries().contains(kSky130));  // the session keeps the change
}

TEST_CASE("pdk trust needs a variant and a version", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  TrustSession t;
  for (const auto& args : {std::vector<std::string>{}, std::vector<std::string>{"sky130A"},
                           std::vector<std::string>{"sky130A", "abc", "extra"}}) {
    const auto run = t.run("pdktrust", args);
    REQUIRE_FALSE(run.ok());
    CHECK(run.diagnostics.front().code == "CMD-USAGE-001");
    CHECK(run.diagnostics.front().text == "Usage: pdk trust <variant> <version>");
  }
  CHECK(t.run("pdkuntrust", {}).diagnostics.front().text == "Usage: pdk untrust <variant> <version>");
}
