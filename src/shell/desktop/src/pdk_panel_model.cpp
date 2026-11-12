// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/pdk_panel_model.hpp"

#include <algorithm>
#include <array>

namespace hefesto::shell {

namespace {

bool needs_version(PdkAction action) noexcept {
  return action == PdkAction::install || action == PdkAction::enable || action == PdkAction::pin ||
         action == PdkAction::uninstall || action == PdkAction::trust || action == PdkAction::untrust;
}

std::string_view verb(PdkAction action) noexcept {
  switch (action) {
    case PdkAction::list: return "list";
    case PdkAction::remote: return "remote";
    case PdkAction::install: return "install";
    case PdkAction::enable: return "enable";
    case PdkAction::pin: return "pin";
    case PdkAction::uninstall: return "uninstall";
    case PdkAction::trust: return "trust";
    case PdkAction::untrust: return "untrust";
  }
  return "list";
}

const pdk::InstalledVersion* newest_installed(const std::vector<pdk::InstalledVersion>& installed,
                                              std::string_view variant) {
  const pdk::InstalledVersion* best = nullptr;
  for (const pdk::InstalledVersion& v : installed) {
    if (v.variant != variant) continue;
    if (v.enabled) return &v;
    if (best == nullptr || v.version.commit_date.value_or("") > best->version.commit_date.value_or("")) best = &v;
  }
  return best;
}

}  // namespace

std::string_view pdk_action_label(PdkAction action) noexcept {
  switch (action) {
    case PdkAction::list: return "Refresh";
    case PdkAction::remote: return "Browse versions";
    case PdkAction::install: return "Install";
    case PdkAction::enable: return "Enable";
    case PdkAction::pin: return "Pin to project";
    case PdkAction::uninstall: return "Uninstall";
    case PdkAction::trust: return "Trust scripts";
    case PdkAction::untrust: return "Revoke trust";
  }
  return "";
}

std::optional<std::string> pdk_command(PdkAction action, std::string_view family, std::string_view variant,
                                       std::string_view version, PdkInstallFlags flags) {
  std::string line = "pdk " + std::string(verb(action));
  if (action == PdkAction::list) {
    return line;
  }
  if (action == PdkAction::remote) {
    return line + " " + std::string(family.empty() ? variant : family);
  }
  if (needs_version(action) && version.empty()) {
    return std::nullopt;
  }
  line += " " + std::string(variant) + " " + std::string(version);
  if (action == PdkAction::install) {
    if (flags.accept_license) line += " -accept-license";
    if (flags.accept_unverified) line += " -accept-unverified";
  }
  return line;
}

namespace {

void set_installed(PdkRow& row, const pdk::InstalledVersion& found, const ScriptTrust& trusted) {
  row.installed_version = found.version.short_form();
  row.installed_hash = found.version.hash;
  row.enabled = found.enabled;
  row.state = found.enabled ? StatusChip{StatusKind::success, "enabled " + *row.installed_version}
                            : StatusChip{StatusKind::info, "installed " + *row.installed_version};
  const ScriptTrustState state = trusted ? trusted(found) : ScriptTrustState{};
  row.scripts_trusted = state.trusted;
  row.scripts = row.scripts_trusted ? StatusChip{StatusKind::success, "scripts trusted"}
                                    : StatusChip{StatusKind::info, "scripts ask before running"};
  row.scripts_date = state.trusted && state.date ? display_date(*state.date) : std::string{};
}

bool all_digits(std::string_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

std::vector<PdkRow> pdk_rows(const pdk::Catalog& catalog, const std::vector<pdk::InstalledVersion>& installed,
                             const ScriptTrust& trusted) {
  std::vector<PdkRow> rows;
  for (const pdk::Family& family : catalog.families()) {
    for (const pdk::Variant& variant : family.variants) {
      PdkRow row;
      row.family = family.name;
      row.variant = variant.name;
      row.default_libraries = variant.default_libraries.size();
      if (const pdk::InstalledVersion* found = newest_installed(installed, variant.name)) {
        set_installed(row, *found, trusted);
      } else {
        row.state = StatusChip{StatusKind::info, "not installed"};
      }
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

std::string display_date(std::string_view iso_date) {
  static constexpr std::array<std::string_view, 12> kMonths = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const bool shaped = iso_date.size() == 10 && iso_date[4] == '-' && iso_date[7] == '-' &&
                      all_digits(iso_date.substr(0, 4)) && all_digits(iso_date.substr(5, 2)) &&
                      all_digits(iso_date.substr(8, 2));
  if (!shaped) return std::string{iso_date};
  const int month = (iso_date[5] - '0') * 10 + (iso_date[6] - '0');
  const int day = (iso_date[8] - '0') * 10 + (iso_date[9] - '0');
  if (month < 1 || month > 12 || day < 1 || day > 31) return std::string{iso_date};
  return std::to_string(day) + " " + std::string{kMonths[static_cast<std::size_t>(month - 1)]} + " " +
         std::string{iso_date.substr(0, 4)};
}

std::string PdkRow::scripts_label() const {
  if (scripts.text.empty()) return {};
  return scripts_date.empty() ? scripts.label() : scripts.label() + " " + scripts_date;
}

std::optional<PdkAction> trust_action(const PdkRow& row) {
  if (!row.installed_hash) return std::nullopt;
  return row.scripts_trusted ? PdkAction::untrust : PdkAction::trust;
}

}  // namespace hefesto::shell
