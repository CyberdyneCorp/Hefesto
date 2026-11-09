// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hefesto.project file (hefesto-settings "Project files", "Global nets
// and power names"; hefesto-pdk-manager "Per-project version pinning").

#include "hefesto/app/project_file.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "hefesto/app/diag_codes.hpp"

namespace hefesto::app {

namespace {

using nlohmann::json;

constexpr std::string_view kFormat = "hefesto.project";
constexpr int kVersion = 1;

diag::Diagnostic format_error(std::string_view source, const std::string& text) {
  const std::string where = source.empty() ? std::string{kProjectFileName} : std::string{source};
  return diag::make(codes::kProjectFormat, where + ": " + text);
}

json nets_json(const GlobalNets& nets) {
  json out = json::object();
  if (nets.global_nets) {
    out["globalNets"] = *nets.global_nets;
  }
  const auto put = [&](const char* key, const std::optional<std::string>& value) {
    if (value) {
      out[key] = *value;
    }
  };
  put("vdd", nets.vdd);
  put("gnd", nets.gnd);
  put("sub", nets.sub);
  return out;
}

json settings_json(const ProjectFile& project) {
  json out = json::object();
  for (const auto& [name, value] : project.extra_settings) {
    out[name] = value;
  }
  out[std::string{settings::kMagicCompat}] = project.settings.compat.magic_compat;
  out[std::string{settings::kNetgenCompat}] = project.settings.compat.netgen_compat;
  out[std::string{settings::kScriptStrict}] = project.settings.script_strict;
  return out;
}

// Reads one value; `error` receives the first problem found.
class Reader {
 public:
  explicit Reader(std::string_view source) : source_(source) {}

  [[nodiscard]] const std::optional<diag::Diagnostic>& error() const noexcept { return error_; }

  std::vector<std::string> strings(const json& value, std::string_view key) {
    std::vector<std::string> out;
    if (!value.is_array()) {
      fail(std::string{key} + " must be an array of strings");
      return out;
    }
    for (const auto& item : value) {
      if (!item.is_string()) {
        fail(std::string{key} + " must be an array of strings");
        return out;
      }
      out.push_back(item.get<std::string>());
    }
    return out;
  }

  std::optional<std::string> string(const json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
      return std::nullopt;
    }
    if (!it->is_string()) {
      fail(std::string{key} + " must be a string");
      return std::nullopt;
    }
    return it->get<std::string>();
  }

  std::optional<PdkPin> pin(const json& value) {
    if (!value.is_object()) {
      fail("pdk must be an object");
      return std::nullopt;
    }
    PdkPin pin{string(value, "family").value_or(""), string(value, "variant").value_or(""),
               string(value, "version").value_or(""), {}};
    if (value.contains("libraries")) {
      pin.libraries = strings(value["libraries"], "pdk.libraries");
    }
    if (pin.family.empty() || pin.variant.empty() || pin.version.empty()) {
      fail("pdk needs family, variant and version");
    }
    return pin;
  }

  GlobalNets nets(const json& value) {
    GlobalNets nets;
    if (!value.is_object()) {
      fail("globalNets must be an object");
      return nets;
    }
    if (value.contains("globalNets")) {
      nets.global_nets = strings(value["globalNets"], "globalNets.globalNets");
    }
    nets.vdd = string(value, "vdd");
    nets.gnd = string(value, "gnd");
    nets.sub = string(value, "sub");
    return nets;
  }

  void settings(const json& value, ProjectFile& project) {
    if (!value.is_object()) {
      fail("settings must be an object");
      return;
    }
    for (const auto& [name, item] : value.items()) {
      setting(name, item, project);
    }
  }

  void fail(std::string text) {
    if (!error_) {
      error_ = format_error(source_, text);
    }
  }

 private:
  void setting(const std::string& name, const json& item, ProjectFile& project) {
    const auto typed = settings::get(project.settings, name);
    if (typed.ok()) {
      if (!item.is_boolean()) {
        fail("setting " + name + " must be true or false");
        return;
      }
      (void)settings::set(project.settings, name, item.get<bool>() ? "on" : "off");
    } else if (item.is_string()) {
      project.extra_settings[name] = item.get<std::string>();
    } else {
      fail("setting " + name + " must be a string");
    }
  }

  std::string_view source_;
  std::optional<diag::Diagnostic> error_;
};

void read_body(const json& root, Reader& reader, ProjectFile& project) {
  if (root.contains("pdk") && !root["pdk"].is_null()) {
    project.pdk = reader.pin(root["pdk"]);
  }
  if (root.contains("cellSearchPaths")) {
    project.cell_search_paths = reader.strings(root["cellSearchPaths"], "cellSearchPaths");
  }
  if (root.contains("settings")) {
    reader.settings(root["settings"], project);
  }
  if (root.contains("globalNets")) {
    project.nets = reader.nets(root["globalNets"]);
  }
  if (root.contains("documents")) {
    project.open_documents = reader.strings(root["documents"], "documents");
  }
  if (root.contains("windowLayout") && !root["windowLayout"].is_null()) {
    project.window_layout = root["windowLayout"].dump();
  }
}

}  // namespace

std::string to_text(const ProjectFile& project) {
  json doc = json::object();
  doc["format"] = kFormat;
  doc["version"] = kVersion;
  if (project.pdk) {
    doc["pdk"] = {{"family", project.pdk->family},
                  {"variant", project.pdk->variant},
                  {"version", project.pdk->version},
                  {"libraries", project.pdk->libraries}};
  }
  doc["cellSearchPaths"] = project.cell_search_paths;
  doc["settings"] = settings_json(project);
  doc["globalNets"] = nets_json(project.nets);
  doc["documents"] = project.open_documents;
  if (!project.window_layout.empty()) {
    json layout = json::parse(project.window_layout, nullptr, false);
    doc["windowLayout"] = layout.is_discarded() ? json(project.window_layout) : layout;
  }
  return doc.dump(2) + "\n";
}

diag::Outcome<ProjectFile> parse_project(std::string_view text, std::string_view source_name) {
  const json root = json::parse(text, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return format_error(source_name, "not a JSON object");
  }
  const auto format = root.find("format");
  if (format == root.end() || !format->is_string() || format->get<std::string>() != kFormat) {
    return format_error(source_name, "\"format\" is not \"hefesto.project\"");
  }
  const auto version = root.find("version");
  if (version == root.end() || !version->is_number_integer() || version->get<int>() > kVersion) {
    return format_error(source_name, "unsupported \"version\" (this release reads version 1)");
  }
  ProjectFile project;
  Reader reader{source_name};
  read_body(root, reader, project);
  if (reader.error()) {
    return *reader.error();
  }
  return project;
}

diag::Outcome<ProjectFile> load_project(platform::FileSystem& fs, const platform::Path& folder) {
  const platform::Path path = folder / std::string{kProjectFileName};
  auto text = fs.read(path);
  if (!text) {
    return diag::make(codes::kProjectIo, "cannot read " + path.string() + ": " + text.error().text);
  }
  return parse_project(text.value(), path.string());
}

diag::Status save_project(platform::FileSystem& fs, const platform::Path& folder, const ProjectFile& project) {
  const platform::Path path = folder / std::string{kProjectFileName};
  auto status = fs.write_atomic(path, to_text(project));
  if (!status) {
    return diag::make(codes::kProjectIo, "cannot write " + path.string() + ": " + status.error().text);
  }
  return diag::Done{};
}

}  // namespace hefesto::app
