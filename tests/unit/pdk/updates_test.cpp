// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-pdk-manager "Updates": offers per family, the automatic-check
// policy (24 h, setting) and its stamp file.

#include <catch2/catch_test_macros.hpp>

#include "harness.hpp"
#include "hefesto/pdk/updates.hpp"

namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using namespace hefesto::test::pdk_harness;  // NOLINT(google-build-using-namespace)
using std::chrono::hours;
using std::chrono::system_clock;

namespace {

const std::string kNewer = "dddddddddddddddddddddddddddddddddddddddd";
const std::string kPre = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";

// The family manifest lists a prerelease, a newer stable version and kHashA.
void publish_family(Harness& h) {
  h.fetcher.documents[std::string{kBase} + "/sky130/manifest.json"] =
      R"({"pdk":"sky130","versions":[{"version":")" + kHashA + R"(","date":"2024-08-17T01:34:30Z"},
         {"version":")" + kNewer + R"(","date":"2025-02-01T00:00:00Z"},
         {"version":")" + kPre + R"(","date":"2025-06-01T00:00:00Z","prerelease":true}]})";
}

}  // namespace

TEST_CASE("an update is offered per family with installed and offered versions",
          "[cap:hefesto-pdk-manager][req:Updates]") {
  Harness h;
  auto none = pdk::check_updates(h.manager, h.options.source);
  REQUIRE(none.ok());
  CHECK(none.value().empty());  // nothing installed: no family checked
  CHECK(h.fetcher.requests.empty());
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  publish_family(h);
  auto offers = pdk::check_updates(h.manager, h.options.source);
  REQUIRE(offers.ok());
  REQUIRE(offers.value().size() == 1);
  const auto& offer = offers.value().front();
  CHECK(offer.family == "sky130");
  CHECK(offer.installed.hash == kHashA);
  CHECK(offer.installed.commit_date == "2024-08-17T01:34:30Z");
  CHECK(offer.offered.hash == kNewer);  // the prerelease is never offered
  CHECK(offer.offered.commit_date == "2025-02-01T00:00:00Z");
}

TEST_CASE("no update when the newest stable version is installed", "[cap:hefesto-pdk-manager][req:Updates]") {
  Harness h;
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  h.fetcher.documents[std::string{kBase} + "/sky130/manifest.json"] =
      R"({"pdk":"sky130","versions":[{"version":")" + kHashA + R"(","date":"2024-08-17T01:34:30Z"}]})";
  auto offers = pdk::check_updates(h.manager, h.options.source);
  REQUIRE(offers.ok());
  CHECK(offers.value().empty());
}

TEST_CASE("the automatic check runs at most once per 24 hours and can be turned off",
          "[cap:hefesto-pdk-manager][req:Updates]") {
  const auto now = system_clock::time_point{hours{500'000}};
  CHECK(pdk::auto_update_check_due(true, std::nullopt, now));
  CHECK_FALSE(pdk::auto_update_check_due(false, std::nullopt, now));
  CHECK_FALSE(pdk::auto_update_check_due(true, now - hours{23}, now));
  CHECK(pdk::auto_update_check_due(true, now - hours{24}, now));
  CHECK(pdk::auto_update_check_due(true, now + hours{1}, now));  // clock moved back
}

TEST_CASE("the update-check stamp round trips; malformed stamps are ignored", "[cap:hefesto-pdk-manager][req:Updates]") {
  hefesto::test::TempDir dir;
  CHECK_FALSE(pdk::last_update_check(dir.path()));
  const auto when = system_clock::time_point{std::chrono::seconds{1'700'000'000}};
  REQUIRE(pdk::record_update_check(dir.path(), when).ok());
  CHECK(pdk::last_update_check(dir.path()) == when);
  for (const char* bad : {"", "abc", "-5", "99999999999999999999999999", "1700000000x"}) {
    hefesto::test::write_file(dir.path() / "ciel/.hefesto-update-check", bad);
    CHECK_FALSE(pdk::last_update_check(dir.path()));
  }
}
