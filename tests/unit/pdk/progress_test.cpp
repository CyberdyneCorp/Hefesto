// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Install progress (hefesto-pdk-manager "Headless and scripted operation":
// the same operations back the desktop PDK manager panel): the
// InstallOptions::progress events and their PDK-PROGRESS-001 form.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>

#include "harness.hpp"
#include "hefesto/pdk/commands.hpp"
#include "hefesto/pdk/diag_codes.hpp"

namespace pdk = hefesto::pdk;
using namespace hefesto::test::pdk_harness;  // NOLINT(google-build-using-namespace)
using pdk::InstallPhase;

TEST_CASE("install reports download, verify, unpack per asset, then finalize",
          "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  Harness h;
  std::vector<pdk::InstallProgress> events;
  h.options.progress = [&events](const pdk::InstallProgress& p) { events.push_back(p); };
  REQUIRE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  REQUIRE_FALSE(events.empty());
  CHECK(events.front().phase == InstallPhase::download);
  CHECK(events.front().asset == "common.tar.zst");
  CHECK(events.front().part == 1);
  CHECK(events.front().parts == 2);
  CHECK(events.front().bytes == 0);
  CHECK(events.front().total > 0);  // the catalog size, even though the fetcher reports none
  CHECK(events.back() == pdk::InstallProgress{InstallPhase::finalize, "", 2, 2, 0, 0});
  // Phases never go backwards within an asset; parts never go backwards.
  for (std::size_t i = 1; i < events.size(); ++i) {
    const auto& a = events[i - 1];
    const auto& b = events[i];
    CHECK((b.part > a.part || (b.part == a.part && b.phase >= a.phase)));
    CHECK(b.bytes <= std::max(b.total, b.bytes));
  }
  const auto verified = std::count_if(events.begin(), events.end(), [](const pdk::InstallProgress& p) {
    return p.phase == InstallPhase::verify && p.bytes == p.total && p.total > 0;
  });
  CHECK(verified == 2);
  const auto unpacked = std::count_if(events.begin(), events.end(), [](const pdk::InstallProgress& p) {
    return p.phase == InstallPhase::unpack && p.bytes == p.total && p.total > 0;
  });
  CHECK(unpacked == 2);
}

TEST_CASE("a failed verification reports no completed verify and no finalize",
          "[cap:hefesto-pdk-manager][req:Download integrity verification]") {
  Harness h;
  h.corrupt(kHashA, "common");
  std::vector<pdk::InstallProgress> events;
  h.options.progress = [&events](const pdk::InstallProgress& p) { events.push_back(p); };
  REQUIRE_FALSE(h.install("sky130A", kHashA, {"sky130_fd_sc_hd"}).ok());
  for (const auto& p : events) {
    CHECK(p.phase != InstallPhase::finalize);
    CHECK_FALSE((p.phase == InstallPhase::verify && p.bytes > 0));
  }
}

// Regression: the unpack bound was asset size * unpack factor without a
// saturation check, so a huge -unpack-factor wrapped to a tiny bound and the
// install failed with PDK-UNPACK-002.
TEST_CASE("a huge unpack factor saturates instead of wrapping", "[cap:hefesto-pdk-manager][req:Safe archive unpacking]") {
  Harness h;
  h.options.unpack_factor = std::uint64_t{1} << 63U;  // asset sizes are multiples of 512: the product wrapped to 0
  const auto status = h.install("sky130A", kHashA, {"sky130_fd_sc_hd"});
  INFO((status.ok() ? std::string{} : status.error().code + " " + status.error().text));
  CHECK(status.ok());
}

TEST_CASE("progress text round trip and malformed texts", "[cap:hefesto-pdk-manager][req:Headless and scripted operation]") {
  const pdk::InstallProgress p{InstallPhase::unpack, "sky130_fd_sc_hd.tar.zst", 2, 3, 512, 1024};
  const std::string text = pdk::format_install_progress(p);
  CHECK(text == "phase=unpack part=2/3 bytes=512 total=1024 asset=sky130_fd_sc_hd.tar.zst");
  const auto parsed = pdk::parse_install_progress(hefesto::diag::make(pdk::codes::kProgress, text));
  REQUIRE(parsed);
  CHECK(*parsed == p);
  const auto finalize = pdk::InstallProgress{InstallPhase::finalize, "", 3, 3, 0, 0};
  CHECK(pdk::parse_install_progress(hefesto::diag::make(pdk::codes::kProgress, pdk::format_install_progress(finalize))) ==
        finalize);
  CHECK_FALSE(pdk::parse_install_progress(hefesto::diag::make(pdk::codes::kInfo, text)));
  for (const char* bad : {"", "phase=unpack", "phase=melt part=1/1 bytes=0 total=0 asset=x",
                          "phase=unpack part=1 bytes=0 total=0 asset=x",
                          "phase=unpack part=1/1 bytes=99999999999999999999999 total=0 asset=x",
                          "phase=unpack part=1/1 bytes=-1 total=0 asset=x", "asset=x"}) {
    CHECK_FALSE(pdk::parse_install_progress(hefesto::diag::make(pdk::codes::kProgress, bad)));
  }
}
