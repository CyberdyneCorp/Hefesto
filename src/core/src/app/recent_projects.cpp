// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The recent-projects list (hefesto-pdk-manager "Uninstall and cleanup").

#include "hefesto/app/recent_projects.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/project_file.hpp"

namespace hefesto::app {

diag::Outcome<std::vector<platform::Path>> load_recent_projects(platform::FileSystem& fs, const platform::Path& file) {
  std::vector<platform::Path> folders;
  if (!fs.exists(file)) {
    return folders;
  }
  auto text = fs.read(file);
  if (!text) {
    return diag::make(codes::kRecentProjects, "cannot read " + file.string() + ": " + text.error().text);
  }
  const auto doc = nlohmann::json::parse(text.value(), nullptr, false);
  if (doc.is_discarded() || !doc.is_object() || !doc.contains("projects") || !doc["projects"].is_array()) {
    return diag::make(codes::kRecentProjects, file.string() + ": expected {\"projects\": [...]}");
  }
  for (const auto& item : doc["projects"]) {
    if (!item.is_string() || item.get<std::string>().empty()) {
      return diag::make(codes::kRecentProjects, file.string() + ": a project entry is not a non-empty string");
    }
    if (folders.size() < kMaxRecentProjects) {
      folders.emplace_back(item.get<std::string>());
    }
  }
  return folders;
}

diag::Status note_recent_project(platform::FileSystem& fs, const platform::Path& file, const platform::Path& folder) {
  auto loaded = load_recent_projects(fs, file);
  std::vector<platform::Path> folders = loaded ? std::move(loaded).value() : std::vector<platform::Path>{};
  const platform::Path entry = folder.lexically_normal();
  std::erase_if(folders, [&](const platform::Path& p) { return p.lexically_normal() == entry; });
  folders.insert(folders.begin(), entry);
  if (folders.size() > kMaxRecentProjects) {
    folders.resize(kMaxRecentProjects);
  }
  nlohmann::json doc;
  doc["projects"] = nlohmann::json::array();
  for (const auto& p : folders) {
    doc["projects"].push_back(p.string());
  }
  if (auto made = fs.create_directories(file.parent_path()); !made && !file.parent_path().empty()) {
    return made;
  }
  return fs.write_atomic(file, doc.dump(2) + "\n");
}

std::vector<platform::Path> projects_pinning(platform::FileSystem& fs, const std::vector<platform::Path>& folders,
                                             std::string_view family, std::string_view version) {
  std::vector<platform::Path> out;
  for (const auto& folder : folders) {
    if (!fs.exists(folder / std::string{kProjectFileName})) {
      continue;
    }
    auto project = load_project(fs, folder);
    if (project && project.value().pdk && project.value().pdk->family == family &&
        project.value().pdk->version == version) {
      out.push_back(folder);
    }
  }
  return out;
}

}  // namespace hefesto::app
