// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK updates (hefesto-pdk-manager "Updates").

#include "hefesto/pdk/updates.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>

#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::pdk {

namespace {

using std::chrono::system_clock;

platform::Path stamp_path(const platform::Path& root) {
  return root / "ciel" / ".hefesto-update-check";
}

// The offer for one family, if its newest stable remote version is newer
// than every installed one.
std::optional<UpdateOffer> offer_for(const Family& family, const std::vector<InstalledVersion>& installed,
                                     const std::vector<PdkVersion>& remote) {
  std::set<std::string, std::less<>> hashes;
  const InstalledVersion* any = nullptr;
  for (const auto& v : installed) {
    if (v.family == family.name) {
      hashes.insert(v.version.hash);
      any = any == nullptr ? &v : any;
    }
  }
  const auto stable = std::find_if(remote.begin(), remote.end(), [](const PdkVersion& v) { return !v.prerelease; });
  if (any == nullptr || stable == remote.end() || hashes.contains(stable->hash)) {
    return std::nullopt;
  }
  const auto newest_installed =
      std::find_if(remote.begin(), remote.end(), [&](const PdkVersion& v) { return hashes.contains(v.hash); });
  if (newest_installed != remote.end() && newest_installed < stable) {
    return std::nullopt;  // a newer (prerelease) version is installed
  }
  return UpdateOffer{family.name, newest_installed != remote.end() ? *newest_installed : any->version, *stable};
}

}  // namespace

diag::Outcome<std::vector<UpdateOffer>> check_updates(PdkManager& manager, const CatalogSource& source) {
  auto installed = manager.installed();
  if (!installed) {
    return installed.error();
  }
  std::vector<UpdateOffer> offers;
  for (const auto& family : manager.catalog().families()) {
    const bool has_install = std::any_of(installed.value().begin(), installed.value().end(),
                                         [&](const InstalledVersion& v) { return v.family == family.name; });
    if (!has_install) {
      continue;
    }
    auto remote = manager.remote_versions(family, source);
    if (!remote) {
      return remote.error();
    }
    if (auto offer = offer_for(family, installed.value(), remote.value())) {
      offers.push_back(std::move(*offer));
    }
  }
  return offers;
}

bool auto_update_check_due(bool enabled, std::optional<system_clock::time_point> last,
                           system_clock::time_point now) noexcept {
  if (!enabled) {
    return false;
  }
  return !last || *last > now || now - *last >= kUpdateCheckInterval;
}

std::optional<system_clock::time_point> last_update_check(const platform::Path& root) {
  std::ifstream in{stamp_path(root)};
  std::string word;
  if (!(in >> word)) {
    return std::nullopt;
  }
  std::int64_t seconds = 0;
  const auto [end, ec] = std::from_chars(word.data(), word.data() + word.size(), seconds);
  // Bounded to ~year 2500 so the conversion to system_clock never overflows.
  constexpr std::int64_t kMaxSeconds = 16'725'225'600;
  if (ec != std::errc{} || end != word.data() + word.size() || seconds < 0 || seconds > kMaxSeconds) {
    return std::nullopt;
  }
  return system_clock::time_point{std::chrono::seconds{seconds}};
}

diag::Status record_update_check(const platform::Path& root, system_clock::time_point now) {
  const platform::Path path = stamp_path(root);
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  std::ofstream out{path, std::ios::trunc};
  out << seconds << "\n";
  return out ? diag::Status{diag::Done{}} : diag::make(codes::kInstallIo, "cannot write " + path.string());
}

}  // namespace hefesto::pdk
