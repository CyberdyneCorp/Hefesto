// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/pdk_panel_model.hpp"

namespace hefesto::shell {

namespace {

bool needs_version(PdkAction action) noexcept {
  return action == PdkAction::install || action == PdkAction::enable || action == PdkAction::pin ||
         action == PdkAction::uninstall;
}

std::string_view verb(PdkAction action) noexcept {
  switch (action) {
    case PdkAction::list: return "list";
    case PdkAction::remote: return "remote";
    case PdkAction::install: return "install";
    case PdkAction::enable: return "enable";
    case PdkAction::pin: return "pin";
    case PdkAction::uninstall: return "uninstall";
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

std::vector<PdkRow> pdk_rows(const pdk::Catalog& catalog, const std::vector<pdk::InstalledVersion>& installed) {
  std::vector<PdkRow> rows;
  for (const pdk::Family& family : catalog.families()) {
    for (const pdk::Variant& variant : family.variants) {
      PdkRow row;
      row.family = family.name;
      row.variant = variant.name;
      row.default_libraries = variant.default_libraries.size();
      const pdk::InstalledVersion* found = newest_installed(installed, variant.name);
      if (found == nullptr) {
        row.state = StatusChip{StatusKind::info, "not installed"};
      } else {
        row.installed_version = found->version.short_form();
        row.installed_hash = found->version.hash;
        row.enabled = found->enabled;
        row.state = found->enabled ? StatusChip{StatusKind::success, "enabled " + *row.installed_version}
                                   : StatusChip{StatusKind::info, "installed " + *row.installed_version};
      }
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

}  // namespace hefesto::shell
