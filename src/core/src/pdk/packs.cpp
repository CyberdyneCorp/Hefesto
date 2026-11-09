// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Offline PDK packs (hefesto-pdk-manager "Offline PDK packs"). Hefesto's
// own format (decision P-14); ciel has no packs.

#include "hefesto/pdk/packs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/sha256.hpp"
#include "hefesto/pdk/tar_reader.hpp"
#include "hefesto/pdk/tar_writer.hpp"

namespace hefesto::pdk {

namespace {

namespace stdfs = std::filesystem;
using nlohmann::json;
using platform::Path;

constexpr std::string_view kAssetSuffix = ".tar.zst";

diag::Diagnostic pack_error(std::string text) {
  return diag::make(codes::kPack, std::move(text));
}

bool is_dir(const Path& path) {
  std::error_code ec;
  return stdfs::is_directory(path, ec);
}

// Removes a scratch directory on every exit path.
class ScratchDir {
 public:
  explicit ScratchDir(Path path) : path_(std::move(path)) {
    std::error_code ec;
    stdfs::remove_all(path_, ec);
    stdfs::create_directories(path_, ec);
    ok_ = !ec;
  }
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir() {
    std::error_code ec;
    stdfs::remove_all(path_, ec);
  }
  [[nodiscard]] const Path& path() const noexcept { return path_; }
  [[nodiscard]] bool ok() const noexcept { return ok_; }

 private:
  Path path_;
  bool ok_ = false;
};

std::string string_or(const json& doc, const char* key, std::string fallback = {}) {
  const auto it = doc.find(key);
  return it != doc.end() && it->is_string() ? it->get<std::string>() : fallback;
}

// A string array field; a single string counts as a one-element array.
diag::Outcome<std::vector<std::string>> strings(const json& doc, const char* key) {
  std::vector<std::string> out;
  const auto it = doc.find(key);
  if (it == doc.end()) {
    return out;
  }
  if (it->is_string()) {
    out.push_back(it->get<std::string>());
    return out;
  }
  if (!it->is_array()) {
    return pack_error(std::string{"pack.json: \""} + key + "\" is not a list of strings");
  }
  for (const auto& item : *it) {
    if (!item.is_string()) {
      return pack_error(std::string{"pack.json: \""} + key + "\" is not a list of strings");
    }
    out.push_back(item.get<std::string>());
  }
  return out;
}

bool safe_file_name(std::string_view name) {
  return !name.empty() && !name.starts_with('.') && name.find_first_of("/\\") == std::string_view::npos &&
         name != kPackManifestName;
}

bool valid_digest(const std::string& digest) {
  return digest.empty() ||
         (digest.size() == 64 && digest.find_first_not_of("0123456789abcdef") == std::string::npos);
}

diag::Outcome<Asset> parse_pack_asset(const json& item) {
  if (!item.is_object()) {
    return pack_error("pack.json: an asset is not an object");
  }
  Asset asset;
  asset.content = string_or(item, "content");
  asset.filename = string_or(item, "filename");
  asset.sha256 = string_or(item, "sha256");
  if (const auto size = item.find("size"); size != item.end() && size->is_number_unsigned()) {
    asset.size = size->get<std::uint64_t>();
  }
  if (asset.content.empty() || !safe_file_name(asset.filename)) {
    return pack_error("pack.json: asset '" + asset.filename + "' needs a content and a plain file name");
  }
  if (!valid_digest(asset.sha256)) {
    return pack_error("pack.json: asset '" + asset.filename + "' has a malformed sha256");
  }
  return asset;
}

diag::Status parse_assets(const json& doc, PackManifest& out) {
  const auto it = doc.find("assets");
  if (it == doc.end() || !it->is_array() || it->empty()) {
    return pack_error("pack.json has no \"assets\" list");
  }
  for (const auto& item : *it) {
    auto asset = parse_pack_asset(item);
    if (!asset) {
      return asset.error();
    }
    out.assets.push_back(std::move(asset).value());
  }
  return diag::Done{};
}

diag::Status parse_lists(const json& doc, PackManifest& out) {
  for (auto [key, target] : {std::pair{"variants", &out.variants}, std::pair{"libraries", &out.libraries},
                             std::pair{"license", &out.license}, std::pair{"partial", &out.partial}}) {
    auto list = strings(doc, key);
    if (!list) {
      return list.error();
    }
    *target = std::move(list).value();
  }
  return diag::Done{};
}

// ---- export ----------------------------------------------------------------

// The installed version directory (ciel layout, else volare), if any.
std::optional<Path> version_dir_of(const PdkManager& manager, const Family& family, std::string_view hash) {
  for (const char* tool : {"ciel", "volare"}) {
    const Path dir = manager.root() / tool / family.name / "versions" / std::string{hash};
    if (is_dir(dir)) {
      return dir;
    }
  }
  return std::nullopt;
}

// The part a file of the version belongs to: `common`, or the library of
// `<variant>/libs.ref/<library>/...`; empty for the generated SOURCES file.
std::string part_of(const Path& relative) {
  std::vector<std::string> parts;
  for (const auto& element : relative) {
    parts.push_back(element.string());
  }
  if (parts.size() == 2 && parts[1] == "SOURCES") {
    return {};
  }
  return parts.size() >= 4 && parts[1] == "libs.ref" ? parts[2] : std::string{"common"};
}

using PartFiles = std::map<std::string, std::vector<std::pair<std::string, Path>>>;

// Regular files of every installed variant, grouped by part, in name order.
PartFiles collect_files(const Path& version_dir, const std::vector<std::string>& variants, diag::DiagnosticSink& sink) {
  PartFiles files;
  for (const auto& variant : variants) {
    std::error_code ec;
    for (stdfs::recursive_directory_iterator it{version_dir / variant, ec}, end; !ec && it != end; it.increment(ec)) {
      const auto status = it->symlink_status(ec);
      const Path relative = it->path().lexically_relative(version_dir);
      if (stdfs::is_symlink(status)) {
        sink.emit(diag::make(codes::kSkippedEntry, relative.generic_string() + " is a symbolic link; not packed"));
      } else if (stdfs::is_regular_file(status)) {
        if (const std::string part = part_of(relative); !part.empty()) {
          files[part].emplace_back(relative.generic_string(), it->path());
        }
      }
    }
  }
  for (auto& [part, list] : files) {
    std::sort(list.begin(), list.end());
  }
  return files;
}

diag::Outcome<LocalAsset> write_asset(const std::string& content, const std::vector<std::pair<std::string, Path>>& files,
                                      const Path& dir) {
  LocalAsset local{Asset{content, content + std::string{kAssetSuffix}, {}, {}, 0}, {}};
  local.file = dir / local.asset.filename;
  {
    std::ofstream file{local.file, std::ios::binary | std::ios::trunc};
    StoredZstdWriter frame{file};
    std::ostream out{&frame};
    TarWriter tar{out};
    for (const auto& [name, source] : files) {
      if (auto status = tar.add_file(name, source); !status) {
        return status.error();
      }
    }
    if (auto status = tar.finish(); !status) {
      return status.error();
    }
    frame.finish();
    if (!file.flush()) {
      return diag::make(codes::kInstallIo, "cannot write " + local.file.string());
    }
  }
  auto digest = sha256_file_hex(local.file);
  if (!digest) {
    return digest.error();
  }
  local.asset.sha256 = digest.value();
  std::error_code ec;
  local.asset.size = stdfs::file_size(local.file, ec);
  return local;
}

diag::Outcome<std::vector<LocalAsset>> rebuild_assets(const Path& version_dir, const std::vector<std::string>& variants,
                                                      const Path& dir, diag::DiagnosticSink& sink) {
  const PartFiles files = collect_files(version_dir, variants, sink);
  std::vector<LocalAsset> assets;
  for (const auto& [content, list] : files) {
    auto asset = write_asset(content, list, dir);
    if (!asset) {
      return asset.error();
    }
    // `common` first, as in a catalog install.
    assets.insert(content == "common" ? assets.begin() : assets.end(), std::move(asset).value());
  }
  return assets;
}

// The original assets, when every recorded one is still in the download
// cache with its recorded SHA-256.
std::optional<std::vector<LocalAsset>> cached_assets(const PdkManager& manager, const Family& family,
                                                     std::string_view hash, const Provenance& provenance) {
  const Path cache = manager.root() / "ciel" / family.name / ".hefesto-downloads" / std::string{hash};
  std::vector<LocalAsset> assets;
  for (const auto& [name, digest] : provenance.assets) {
    if (digest.empty() || !name.ends_with(kAssetSuffix)) {
      return std::nullopt;
    }
    const Path file = cache / name;
    auto actual = sha256_file_hex(file);
    if (!actual || actual.value() != digest) {
      return std::nullopt;
    }
    std::error_code ec;
    const std::uint64_t size = stdfs::file_size(file, ec);
    assets.push_back(LocalAsset{Asset{name.substr(0, name.size() - kAssetSuffix.size()), name, {}, digest, size}, file});
  }
  if (assets.empty()) {
    return std::nullopt;
  }
  return assets;
}

PackManifest base_manifest(const Family& family, std::string_view hash, const Provenance& provenance,
                           const Path& version_dir) {
  PackManifest manifest;
  manifest.family = family.name;
  manifest.version = std::string{hash};
  manifest.date = provenance.date;
  manifest.license = {provenance.license};
  manifest.source = provenance.source == "external" ? std::string{"local"} : provenance.source;
  manifest.partial = provenance.partial;
  for (const auto& variant : family.variants) {
    if (is_dir(version_dir / variant.name)) {
      manifest.variants.push_back(variant.name);
    }
  }
  return manifest;
}

diag::Status write_pack(const PackManifest& manifest, const std::vector<LocalAsset>& assets, const Path& target) {
  const Path temp = target.string() + ".tmp";
  {
    std::ofstream file{temp, std::ios::binary | std::ios::trunc};
    TarWriter tar{file};
    auto status = tar.add_bytes(kPackManifestName, pack_manifest_json(manifest));
    for (std::size_t i = 0; status && i < assets.size(); ++i) {
      status = tar.add_file(assets[i].asset.filename, assets[i].file);
    }
    status = status ? tar.finish() : status;
    if (!status) {
      file.close();
      std::error_code ec;
      stdfs::remove(temp, ec);
      return status;
    }
  }
  std::error_code ec;
  stdfs::rename(temp, target, ec);
  return ec ? diag::make(codes::kInstallIo, "cannot write " + target.string() + ": " + ec.message())
            : diag::Status{diag::Done{}};
}

// ---- import ----------------------------------------------------------------

diag::Outcome<PackManifest> read_manifest(const Path& dir) {
  std::ifstream in{dir / std::string{kPackManifestName}, std::ios::binary};
  if (!in) {
    return pack_error("the pack has no pack.json");
  }
  std::ostringstream text;
  text << in.rdbuf();
  return parse_pack_manifest(text.str());
}

diag::Outcome<std::vector<LocalAsset>> local_assets(const PackManifest& manifest, const Path& dir) {
  std::vector<LocalAsset> out;
  for (const auto& asset : manifest.assets) {
    const Path file = dir / asset.filename;
    std::error_code ec;
    if (!stdfs::is_regular_file(file, ec)) {
      return pack_error("the pack does not contain " + asset.filename + " named in pack.json");
    }
    out.push_back(LocalAsset{asset, file});
  }
  return out;
}

}  // namespace

diag::Outcome<PackManifest> parse_pack_manifest(std::string_view text) {
  const auto doc = json::parse(text, nullptr, false);
  if (doc.is_discarded() || !doc.is_object()) {
    return pack_error("pack.json is not a JSON object");
  }
  PackManifest out;
  out.family = string_or(doc, "family");
  out.version = string_or(doc, "version");
  out.source = string_or(doc, "source", "local");
  if (const auto date = doc.find("date"); date != doc.end() && date->is_string()) {
    out.date = date->get<std::string>();
  }
  out.rebuilt = doc.contains("rebuilt") && doc["rebuilt"].is_boolean() && doc["rebuilt"].get<bool>();
  if (out.family.empty() || !is_version_hash(out.version)) {
    return pack_error("pack.json needs a \"family\" and a 40-hex \"version\"");
  }
  if (auto status = parse_lists(doc, out); !status) {
    return status.error();
  }
  if (auto status = parse_assets(doc, out); !status) {
    return status.error();
  }
  return out;
}

std::string pack_manifest_json(const PackManifest& manifest) {
  json doc;
  doc["family"] = manifest.family;
  doc["variants"] = manifest.variants;
  doc["version"] = manifest.version;
  if (manifest.date) {
    doc["date"] = *manifest.date;
  }
  doc["libraries"] = manifest.libraries;
  doc["license"] = manifest.license;
  doc["source"] = manifest.source;
  doc["rebuilt"] = manifest.rebuilt;
  if (!manifest.partial.empty()) {
    doc["partial"] = manifest.partial;
  }
  doc["assets"] = json::array();
  for (const auto& asset : manifest.assets) {
    doc["assets"].push_back(
        {{"content", asset.content}, {"filename", asset.filename}, {"sha256", asset.sha256}, {"size", asset.size}});
  }
  return doc.dump(2) + "\n";
}

diag::Outcome<platform::Path> export_pack(const PdkManager& manager, const Family& family, std::string_view hash,
                                          const platform::Path& out_dir, diag::DiagnosticSink& sink) {
  const auto version_dir = version_dir_of(manager, family, hash);
  auto provenance = manager.provenance(family, hash);
  if (!version_dir || !provenance) {
    return diag::make(codes::kNotInstalled, family.name + " version " + std::string{hash} + " is not installed");
  }
  PackManifest manifest = base_manifest(family, hash, provenance.value(), *version_dir);
  const std::string name = family.name + "-" + std::string{hash} + std::string{kPackExtension};
  const ScratchDir scratch{out_dir / ("." + name + ".parts")};
  if (!scratch.ok()) {
    return diag::make(codes::kInstallIo, "cannot create " + scratch.path().string());
  }
  std::vector<LocalAsset> assets;
  if (auto cached = cached_assets(manager, family, hash, provenance.value())) {
    assets = std::move(*cached);
  } else {
    auto rebuilt = rebuild_assets(*version_dir, manifest.variants, scratch.path(), sink);
    if (!rebuilt) {
      return rebuilt.error();
    }
    assets = std::move(rebuilt).value();
    manifest.rebuilt = true;
  }
  for (const auto& local : assets) {
    manifest.assets.push_back(local.asset);
    if (local.asset.content != "common") {
      manifest.libraries.push_back(local.asset.content);
    }
  }
  const Path target = out_dir / name;
  if (auto status = write_pack(manifest, assets, target); !status) {
    return status.error();
  }
  sink.emit(diag::make(codes::kInfo, "exported " + family.name + " " + std::string{hash} + " to " + target.string() +
                                         (manifest.rebuilt ? " (assets rebuilt from the installed files)" : "")));
  return target;
}

diag::Outcome<PackManifest> import_pack(PdkManager& manager, const platform::Path& pack, const InstallOptions& options,
                                        diag::DiagnosticSink& sink) {
  std::error_code ec;
  const std::uint64_t pack_size = stdfs::file_size(pack, ec);
  std::ifstream in{pack, std::ios::binary};
  if (ec || !in || manager.root().empty()) {
    return diag::make(codes::kInstallIo, "cannot read the pack " + pack.string());
  }
  const ScratchDir scratch{manager.root() / "ciel" / ".hefesto-import" / pack.filename()};
  if (!scratch.ok()) {
    return diag::make(codes::kInstallIo, "cannot create " + scratch.path().string());
  }
  // An uncompressed tar never holds more file bytes than its own size.
  if (auto report = unpack_tar(in, scratch.path(), pack_size); !report) {
    return report.error();
  }
  auto manifest = read_manifest(scratch.path());
  if (!manifest) {
    return manifest.error();
  }
  const Family* family = manager.catalog().find_family(manifest.value().family);
  if (family == nullptr) {
    return pack_error("the pack is for an unknown PDK family '" + manifest.value().family + "'");
  }
  auto assets = local_assets(manifest.value(), scratch.path());
  if (!assets) {
    return assets.error();
  }
  InstallOptions pack_options = options;
  pack_options.source.location = "pack:" + manifest.value().source;
  pack_options.commit_date = manifest.value().date;
  pack_options.include_prefixes = manifest.value().partial;
  if (auto status = manager.install_local(*family, manifest.value().version, assets.value(), pack_options, sink);
      !status) {
    return status.error();
  }
  return manifest;
}

}  // namespace hefesto::pdk
