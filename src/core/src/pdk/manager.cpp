// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PDK manager (hefesto-pdk-manager). The install layout, the
// "installed"/"present" rules, enable and uninstall follow ciel
// (ciel ref: ciel/common.py:45-148, ciel/manage.py:150-350, Apache-2.0);
// verification, safe unpacking, staging and provenance are Hefesto's own
// (decision P-14).

#include "hefesto/pdk/manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <system_error>

#include <nlohmann/json.hpp>

#include "hefesto/diag/codes.hpp"
#include "hefesto/pdk/diag_codes.hpp"
#include "hefesto/pdk/sha256.hpp"
#include "hefesto/pdk/tar_reader.hpp"

namespace hefesto::pdk {

namespace {

namespace stdfs = std::filesystem;
using platform::Path;

constexpr std::string_view kLicenseMarker = ".hefesto-license-accepted";

diag::Diagnostic io_error(const std::string& what, const std::error_code& ec) {
  return diag::make(codes::kInstallIo, what + ": " + ec.message());
}

bool is_dir(const Path& path) {
  std::error_code ec;
  return stdfs::is_directory(path, ec);
}

bool exists_no_follow(const Path& path) {
  std::error_code ec;
  return stdfs::exists(stdfs::symlink_status(path, ec));
}

void remove_tree_quietly(const Path& path) {
  std::error_code ec;
  stdfs::remove_all(path, ec);
}

std::vector<std::string> subdirectory_names(const Path& dir) {
  std::vector<std::string> names;
  std::error_code ec;
  for (stdfs::directory_iterator it{dir, ec}, end; !ec && it != end; it.increment(ec)) {
    if (it->is_directory(ec) && !it->path().filename().string().starts_with(".")) {
      names.push_back(it->path().filename().string());
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::optional<std::string> read_first_word(const Path& path) {
  std::ifstream in{path};
  std::string word;
  if (in >> word) {
    return word;
  }
  return std::nullopt;
}

diag::Status write_text(const Path& path, std::string_view text) {
  std::error_code ec;
  stdfs::create_directories(path.parent_path(), ec);
  const Path temp = path.string() + ".tmp";
  {
    std::ofstream out{temp, std::ios::binary | std::ios::trunc};
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) {
      return diag::make(codes::kInstallIo, "cannot write " + path.string());
    }
  }
  stdfs::rename(temp, path, ec);
  return ec ? diag::Status{io_error("cannot write " + path.string(), ec)} : diag::Status{diag::Done{}};
}

// Moves every file of `from` that `to` lacks into `to` (incremental install).
diag::Status merge_tree(const Path& from, const Path& to) {
  std::error_code ec;
  for (stdfs::recursive_directory_iterator it{from, ec}, end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const Path target = to / stdfs::relative(it->path(), from, ec);
    if (exists_no_follow(target)) {
      continue;
    }
    stdfs::create_directories(target.parent_path(), ec);
    stdfs::rename(it->path(), target, ec);
    if (ec) {
      return io_error("cannot move " + target.string(), ec);
    }
  }
  return ec ? diag::Status{io_error("cannot read " + from.string(), ec)} : diag::Status{diag::Done{}};
}

std::string join_url(std::string_view base, std::string_view rest) {
  std::string url{base};
  while (url.ends_with('/')) {
    url.pop_back();
  }
  return url + "/" + std::string{rest};
}

// The parts of an install: `common` or a library name, with its asset and,
// for an offline pack, the asset file already on disk.
struct Part {
  std::string content;
  Asset asset;
  std::optional<Path> local;
};

void info(diag::DiagnosticSink& sink, std::string text) {
  sink.emit(diag::make(codes::kInfo, std::move(text)));
}

std::uint64_t file_size_or_zero(const Path& path) {
  std::error_code ec;
  const std::uint64_t size = stdfs::file_size(path, ec);
  return ec ? 0 : size;
}

// One install run: download, verify, stage, commit. Removes its staging
// directory on every exit path.
class Installer {
 public:
  Installer(Fetcher& fetcher, Decompressor* decompressor, const InstallOptions& options, diag::DiagnosticSink& sink)
      : fetcher_(fetcher), decompressor_(decompressor), options_(options), sink_(sink) {}
  Installer(const Installer&) = delete;
  Installer& operator=(const Installer&) = delete;
  ~Installer() {
    if (!staging_.empty()) {
      remove_tree_quietly(staging_);
    }
  }

  diag::Status run(const std::vector<Part>& parts, const Path& family_dir, std::string_view hash) {
    const Path version_dir = family_dir / "versions" / std::string{hash};
    const Path downloads = family_dir / ".hefesto-downloads" / std::string{hash};
    staging_ = family_dir / "versions" / ("." + std::string{hash} + ".staging");
    remove_tree_quietly(staging_);
    std::error_code ec;
    stdfs::create_directories(staging_, ec);
    stdfs::create_directories(downloads, ec);
    if (ec) {
      return io_error("cannot create " + staging_.string(), ec);
    }
    parts_ = parts.size();
    for (std::size_t i = 0; i < parts.size(); ++i) {
      if (auto status = install_part(parts[i], i + 1, downloads); !status) {
        return status;
      }
    }
    asset_.clear();
    report(InstallPhase::finalize, 0, 0);
    if (auto status = commit(version_dir); !status) {
      return status;
    }
    remove_tree_quietly(downloads);
    return diag::Done{};
  }

 private:
  void report(InstallPhase phase, std::uint64_t bytes, std::uint64_t total) const {
    if (options_.progress) {
      options_.progress(InstallProgress{phase, asset_, phase == InstallPhase::finalize ? parts_ : part_, parts_, bytes,
                                        total});
    }
  }

  diag::Status install_part(const Part& part, std::size_t index, const Path& downloads) {
    if (options_.cancelled && options_.cancelled()) {
      return diag::make(codes::kInstallIo, "install cancelled; nothing was installed");
    }
    asset_ = part.asset.filename;
    part_ = index;
    const Path archive = part.local ? *part.local : downloads / part.asset.filename;
    auto status = part.local ? verify_reported(part.asset, archive) : fetch_verified(part.asset, archive);
    return status ? unpack(part.asset, archive) : status;
  }

  diag::Status fetch_verified(const Asset& asset, const Path& archive) {
    if (exists_no_follow(archive) && verify(asset, archive, /*quiet=*/true).ok()) {
      info(sink_, "using the downloaded " + asset.filename);
      const std::uint64_t size = file_size_or_zero(archive);
      report(InstallPhase::download, size, size);
      report(InstallPhase::verify, size, size);
      return diag::Done{};
    }
    info(sink_, "downloading " + asset.filename + (asset.size > 0 ? " (" + std::to_string(asset.size) + " bytes)" : ""));
    report(InstallPhase::download, file_size_or_zero(archive), asset.size);
    std::function<void(std::uint64_t, std::uint64_t)> on_bytes;
    if (options_.progress) {
      on_bytes = [this, expected = asset.size](std::uint64_t bytes, std::uint64_t total) {
        report(InstallPhase::download, bytes, expected > 0 ? expected : total);
      };
    }
    if (auto status = fetcher_.download(asset.url, archive, on_bytes); !status) {
      return status;
    }
    auto verified = verify_reported(asset, archive);
    if (!verified) {
      std::error_code ec;
      stdfs::remove(archive, ec);
    }
    return verified;
  }

  diag::Status verify_reported(const Asset& asset, const Path& archive) {
    const std::uint64_t size = file_size_or_zero(archive);
    report(InstallPhase::verify, 0, size);
    auto status = verify(asset, archive, /*quiet=*/false);
    if (status) {
      report(InstallPhase::verify, size, size);
    }
    return status;
  }

  // "Download integrity verification": size, then SHA-256 of the whole file.
  diag::Status verify(const Asset& asset, const Path& archive, bool quiet) const {
    std::error_code ec;
    const std::uint64_t size = stdfs::file_size(archive, ec);
    if (ec || (asset.size > 0 && size != asset.size)) {
      return diag::make(codes::kVerify, asset.filename + ": size mismatch: expected " + std::to_string(asset.size) +
                                            " bytes, got " + (ec ? std::string{"none"} : std::to_string(size)));
    }
    if (asset.sha256.empty()) {
      return diag::Done{};  // accepted as unverified before any download started
    }
    auto digest = sha256_file_hex(archive);
    if (!digest) {
      return digest.error();
    }
    if (digest.value() != asset.sha256) {
      return diag::make(codes::kVerify, asset.filename + ": SHA-256 mismatch: expected " + asset.sha256 + ", got " +
                                            digest.value());
    }
    if (!quiet) {
      info(sink_, "verified " + asset.filename + " sha256 " + asset.sha256);
    }
    return diag::Done{};
  }

  diag::Status unpack(const Asset& asset, const Path& archive) {
    if (decompressor_ == nullptr) {
      return diag::make(codes::kBadArchive, "no Zstandard decoder is available to unpack " + asset.filename);
    }
    const std::uint64_t archive_size = file_size_or_zero(archive);
    report(InstallPhase::unpack, 0, archive_size);
    const std::uint64_t base = asset.size > 0 ? asset.size : archive_size;
    const std::uint64_t limit = saturating_multiply(base, options_.unpack_factor);
    const Path tar = staging_.string() + ".tar";
    auto status = decompressor_->decompress(archive, tar, limit);
    if (status) {
      std::ifstream in{tar, std::ios::binary};
      auto report_or = unpack_tar(in, staging_, limit, entry_filter());
      status = report_or ? diag::Status{diag::Done{}} : diag::Status{report_or.error()};
      if (report_or) {
        report_skipped(report_or.value());
        info(sink_, "unpacked " + asset.filename + ": " + std::to_string(report_or.value().files) + " files");
        report(InstallPhase::unpack, archive_size, archive_size);
      }
    }
    std::error_code ec;
    stdfs::remove(tar, ec);
    return status;
  }

  static std::uint64_t saturating_multiply(std::uint64_t a, std::uint64_t b) {
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    return (b != 0 && a > kMax / b) ? kMax : a * b;
  }

  [[nodiscard]] EntryFilter entry_filter() const {
    if (options_.include_prefixes.empty()) {
      return {};
    }
    return [this](std::string_view name) {
      return std::any_of(options_.include_prefixes.begin(), options_.include_prefixes.end(),
                         [&](const std::string& prefix) { return name.starts_with(prefix); });
    };
  }

  void report_skipped(const UnpackReport& report) {
    for (const auto& name : report.skipped) {
      sink_.emit(diag::make(codes::kSkippedEntry, "archive entry '" + name + "' is not a regular file; not created"));
    }
  }

  // Renames the staging directory into place, or merges it into an existing version.
  diag::Status commit(const Path& version_dir) {
    std::error_code ec;
    if (!is_dir(version_dir)) {
      stdfs::rename(staging_, version_dir, ec);
      if (ec) {
        return io_error("cannot install " + version_dir.string(), ec);
      }
      staging_.clear();
      return diag::Done{};
    }
    return merge_tree(staging_, version_dir);
  }

  Fetcher& fetcher_;
  Decompressor* decompressor_;
  const InstallOptions& options_;
  diag::DiagnosticSink& sink_;
  Path staging_;
  std::string asset_;
  std::size_t part_ = 0;
  std::size_t parts_ = 0;
};

diag::Outcome<std::vector<Part>> match_parts(const std::vector<std::string>& wanted, const std::vector<Asset>& assets,
                                             std::string_view hash, const InstallOptions& options) {
  std::vector<Part> parts;
  for (const auto& content : wanted) {
    const auto it = std::find_if(assets.begin(), assets.end(), [&](const Asset& a) { return a.content == content; });
    if (it == assets.end()) {
      return diag::make(codes::kMissingAsset, "version " + std::string{hash} + " has no asset for '" + content + "'");
    }
    if (it->sha256.empty() && !options.accept_unverified) {
      return diag::make(codes::kUnverified, it->filename + " has no known SHA-256; pass -accept-unverified to install "
                                                           "it from an unverified source");
    }
    parts.push_back(Part{content, *it, std::nullopt});
  }
  return parts;
}

std::string provenance_json(const Family& family, std::string_view hash, const std::vector<Part>& parts,
                            const InstallOptions& options) {
  nlohmann::json doc;
  doc["family"] = family.name;
  doc["repository"] = family.upstream_repo;
  doc["version"] = std::string{hash};
  doc["source"] = options.source.location;
  doc["license"] = family.license;
  if (options.commit_date) {
    doc["date"] = *options.commit_date;
  }
  doc["trusted"] = false;  // no signed Hefesto catalog yet ("Script trust for managed PDKs")
  if (!options.include_prefixes.empty()) {
    doc["partial"] = options.include_prefixes;
  }
  for (const auto& part : parts) {
    doc["assets"][part.asset.filename] = {
        {"content", part.content}, {"sha256", part.asset.sha256}, {"size", part.asset.size}};
  }
  return doc.dump(2) + "\n";
}

void merge_provenance(const Path& path, const std::string& text) {
  auto merged = nlohmann::json::parse(text, nullptr, false);
  std::ifstream in{path};
  if (in) {
    auto old = nlohmann::json::parse(in, nullptr, false);
    if (!old.is_discarded() && old.contains("assets") && old["assets"].is_object()) {
      for (const auto& [name, value] : old["assets"].items()) {
        if (!merged["assets"].contains(name)) {
          merged["assets"][name] = value;
        }
      }
    }
  }
  (void)write_text(path, merged.dump(2) + "\n");
}

std::optional<std::string> provenance_date(const Path& path) {
  std::ifstream in{path};
  if (!in) {
    return std::nullopt;
  }
  auto doc = nlohmann::json::parse(in, nullptr, false);
  if (doc.is_discarded() || !doc.contains("date") || !doc["date"].is_string()) {
    return std::nullopt;
  }
  return doc["date"].get<std::string>();
}

std::vector<std::string> json_strings(const nlohmann::json& doc, const char* key) {
  std::vector<std::string> out;
  if (doc.contains(key) && doc[key].is_array()) {
    for (const auto& item : doc[key]) {
      if (item.is_string()) {
        out.push_back(item.get<std::string>());
      }
    }
  }
  return out;
}

std::string json_string(const nlohmann::json& doc, const char* key, std::string fallback) {
  return doc.contains(key) && doc[key].is_string() ? doc[key].get<std::string>() : fallback;
}

// Fills `out` from a provenance record; false when there is none.
bool read_provenance(const Path& path, Provenance& out) {
  std::ifstream in{path};
  if (!in) {
    return false;
  }
  const auto doc = nlohmann::json::parse(in, nullptr, false);
  if (doc.is_discarded() || !doc.is_object()) {
    return false;
  }
  out.repository = json_string(doc, "repository", out.repository);
  out.source = json_string(doc, "source", out.source);
  out.license = json_string(doc, "license", out.license);
  if (doc.contains("date") && doc["date"].is_string()) {
    out.date = doc["date"].get<std::string>();
  }
  out.trusted = doc.contains("trusted") && doc["trusted"].is_boolean() && doc["trusted"].get<bool>();
  if (doc.contains("assets") && doc["assets"].is_object()) {
    for (const auto& [name, value] : doc["assets"].items()) {
      out.assets.emplace_back(name, value.is_object() ? json_string(value, "sha256", "") : std::string{});
    }
  }
  out.partial = json_strings(doc, "partial");
  return true;
}

// Where one install goes, for run_install.
struct InstallTarget {
  Fetcher& fetcher;
  Decompressor* decompressor;
  const Family& family;
  Path family_dir;
  Path root;
};

// Downloads or verifies, unpacks and commits `parts`, then records the provenance.
diag::Status run_install(const InstallTarget& target, std::string_view hash, const std::vector<Part>& parts,
                         const InstallOptions& options, diag::DiagnosticSink& sink) {
  Installer installer{target.fetcher, target.decompressor, options, sink};
  if (auto status = installer.run(parts, target.family_dir, hash); !status) {
    return status;
  }
  merge_provenance(target.family_dir / ".hefesto" / (std::string{hash} + ".json"),
                   provenance_json(target.family, hash, parts, options));
  info(sink, "installed " + target.family.name + " " + std::string{hash} + " under " + target.root.string());
  return diag::Done{};
}

std::vector<std::string> libraries_of(const Path& variant_dir) {
  return subdirectory_names(variant_dir / "libs.ref");
}

// Scans <tool_dir>/<family>/versions/* for one layout (ciel or volare).
void scan_layout(const Path& tool_dir, const Family& family, bool read_only, std::vector<InstalledVersion>& out) {
  const Path family_dir = tool_dir / family.name;
  const auto current = read_first_word(family_dir / "current");
  for (const auto& hash : subdirectory_names(family_dir / "versions")) {
    const Path version_dir = family_dir / "versions" / hash;
    const auto date = provenance_date(family_dir / ".hefesto" / (hash + ".json"));
    for (const auto& variant : family.variants) {
      const Path dir = version_dir / variant.name;
      if (!is_dir(dir)) {
        continue;
      }
      InstalledVersion entry{family.name, variant.name, PdkVersion{hash, date, std::nullopt, false},
                             libraries_of(dir), current == hash, read_only, dir};
      out.push_back(std::move(entry));
    }
  }
}

}  // namespace

std::string_view install_phase_name(InstallPhase phase) noexcept {
  switch (phase) {
    case InstallPhase::download:
      return "download";
    case InstallPhase::verify:
      return "verify";
    case InstallPhase::unpack:
      return "unpack";
    case InstallPhase::finalize:
      return "finalize";
  }
  return "download";
}

std::optional<InstallPhase> parse_install_phase(std::string_view name) noexcept {
  for (const auto phase : {InstallPhase::download, InstallPhase::verify, InstallPhase::unpack, InstallPhase::finalize}) {
    if (install_phase_name(phase) == name) {
      return phase;
    }
  }
  return std::nullopt;
}

bool source_verified(const CatalogSource& /*source*/) noexcept {
  return false;
}

std::string sha256_hex(std::string_view data) {
  Sha256 hash;
  hash.update(data);
  return hash.finish_hex();
}

PdkManager::PdkManager(platform::FileSystem& fs, InstallHost& host, Fetcher& fetcher, Catalog catalog)
    : fs_(fs), host_(host), fetcher_(fetcher), catalog_(std::move(catalog)) {}

platform::Path PdkManager::resolve_root(const std::optional<platform::Path>& explicit_root) const {
  if (explicit_root) {
    return *explicit_root;
  }
  if (auto env = host_.environment("PDK_ROOT")) {
    return platform::Path{*env};
  }
  const Path home = host_.home_directory();
  return home.empty() ? Path{} : home / ".ciel";  // no home (sandbox, tests): no PDK root
}

platform::Path PdkManager::family_dir(const Family& family) const {
  return root_ / "ciel" / family.name;
}

diag::Outcome<std::vector<PdkVersion>> PdkManager::remote_versions(const Family& family, const CatalogSource& source) {
  if (!source.is_github()) {
    auto text = fetcher_.get_text(join_url(source.location, family.name + "/manifest.json"));
    if (!text) {
      return text.error();
    }
    auto versions = parse_family_manifest(text.value());
    if (versions) {
      sort_versions(versions.value());
    }
    return versions;
  }
  const std::string repo = source.location.substr(7);
  std::vector<PdkVersion> all;
  for (int page = 1; page <= 10; ++page) {  // GitHub pages of 100 releases
    auto text = fetcher_.get_text("https://api.github.com/repos/" + repo + "/releases?per_page=100&page=" +
                                  std::to_string(page));
    if (!text) {
      return text.error();
    }
    auto versions = parse_github_releases(text.value(), family);
    if (!versions) {
      return versions.error();
    }
    const bool last_page = text.value().find("\"tag_name\"") == std::string::npos;
    all.insert(all.end(), versions.value().begin(), versions.value().end());
    if (last_page) {
      break;
    }
  }
  sort_versions(all);
  return all;
}

diag::Outcome<std::vector<Asset>> PdkManager::version_assets(const Family& family, std::string_view hash,
                                                             const CatalogSource& source) {
  if (!source.is_github()) {
    auto text = fetcher_.get_text(join_url(source.location, family.name + "/" + std::string{hash} + "/manifest.json"));
    if (!text) {
      return text.error().code == codes::kFetchNotFound
                 ? diag::make(codes::kUnknownVersion, "version " + std::string{hash} + " of " + family.name +
                                                          " is not in the catalog " + source.location)
                 : text.error();
    }
    return parse_version_manifest(text.value());
  }
  const std::string repo = source.location.substr(7);
  for (const auto& tag : github_release_tags(family, hash)) {
    auto text = fetcher_.get_text("https://api.github.com/repos/" + repo + "/releases/tags/" + tag);
    if (text) {
      return parse_github_release_assets(text.value());
    }
    if (text.error().code != codes::kFetchNotFound) {
      return text.error();
    }
  }
  return diag::make(codes::kUnknownVersion,
                    "no release of " + family.name + " version " + std::string{hash} + " in " + source.location);
}

diag::Outcome<std::vector<InstalledVersion>> PdkManager::installed() const {
  std::vector<InstalledVersion> out;
  for (const auto& family : catalog_.families()) {
    std::vector<InstalledVersion> found;
    scan_layout(root_ / "ciel", family, false, found);
    scan_layout(root_ / "volare", family, true, found);
    std::stable_sort(found.begin(), found.end(), [](const InstalledVersion& a, const InstalledVersion& b) {
      return a.read_only < b.read_only;
    });
    out.insert(out.end(), found.begin(), found.end());
  }
  return out;
}

diag::Outcome<Provenance> PdkManager::provenance(const Family& family, std::string_view hash) const {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  const Path legacy_dir = root_ / "volare" / family.name / "versions" / std::string{hash};
  if (!is_dir(version_dir) && !is_dir(legacy_dir)) {
    return diag::make(codes::kNotInstalled, family.name + " version " + std::string{hash} + " is not installed");
  }
  Provenance out{family.name, family.upstream_repo, std::string{hash}, std::nullopt, "external", family.license,
                 false, {}, {}};
  (void)read_provenance(family_dir(family) / ".hefesto" / (std::string{hash} + ".json"), out);
  return out;
}

std::optional<std::string> PdkManager::current(const Family& family) const {
  return read_first_word(family_dir(family) / "current");
}

diag::Status PdkManager::check_license(const Family& family, const InstallOptions& options) {
  const Path marker = family_dir(family) / std::string{kLicenseMarker};
  if (exists_no_follow(marker)) {
    return diag::Done{};
  }
  if (!options.accept_license) {
    return diag::make(codes::kLicense, "the " + family.name + " PDK is licensed under " + family.license +
                                           "; accept the license (-accept-license) before the first install");
  }
  return write_text(marker, family.license + "\n");
}

diag::Status PdkManager::write_sources(const Family& family, std::string_view hash) {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  for (const auto& variant : family.variants) {
    const Path sources = version_dir / variant.name / "SOURCES";
    if (is_dir(version_dir / variant.name) && !exists_no_follow(sources)) {
      if (auto status = write_text(sources, family.upstream_repo + " " + std::string{hash} + "\n"); !status) {
        return status;
      }
    }
  }
  return diag::Done{};
}

bool PdkManager::part_present(const Family& family, std::string_view hash, std::string_view content) const {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  if (content == "common") {
    return is_dir(version_dir / family.variants.front().name / "libs.tech");
  }
  return std::any_of(family.variants.begin(), family.variants.end(), [&](const Variant& v) {
    return is_dir(version_dir / v.name / "libs.ref" / std::string{content});
  });
}

diag::Status PdkManager::check_consent(const Family& family, const InstallOptions& options) {
  if (auto status = check_license(family, options); !status) {
    return status;
  }
  if (!source_verified(options.source) && !options.accept_unverified) {
    return diag::make(codes::kUnverified, "the PDK source " + options.source.location +
                                              " is unverified (no Hefesto catalog signature); pass -accept-unverified "
                                              "to install from it");
  }
  return diag::Done{};
}

diag::Status PdkManager::install(const Selection& selection, std::string_view hash,
                                 const std::vector<std::string>& libraries, const InstallOptions& options,
                                 diag::DiagnosticSink& sink) {
  if (selection.family == nullptr || selection.variant == nullptr || !is_version_hash(hash)) {
    return diag::make(codes::kSelect, "install needs a PDK variant and a 40-hex version hash");
  }
  const Family& family = *selection.family;
  auto libs = catalog_.resolve_libraries(*selection.variant, libraries);
  if (!libs) {
    return libs.error();
  }
  std::vector<std::string> wanted;
  if (!part_present(family, hash, "common")) {
    wanted.emplace_back("common");
  }
  std::copy_if(libs.value().begin(), libs.value().end(), std::back_inserter(wanted),
               [&](const std::string& lib) { return !part_present(family, hash, lib); });
  if (wanted.empty()) {
    info(sink, family.name + " " + std::string{hash}.substr(0, 7) + " is already installed with the requested libraries");
    return write_sources(family, hash);
  }
  if (auto status = check_consent(family, options); !status) {
    return status;
  }
  auto assets = version_assets(family, hash, options.source);
  if (!assets) {
    return assets.error();
  }
  auto parts = match_parts(wanted, assets.value(), hash, options);
  if (!parts) {
    return parts.error();
  }
  if (auto status = run_install(InstallTarget{fetcher_, decompressor_, family, family_dir(family), root_}, hash,
                                parts.value(), options, sink);
      !status) {
    return status;
  }
  return write_sources(family, hash);
}

diag::Status PdkManager::install_local(const Family& family, std::string_view hash,
                                       const std::vector<LocalAsset>& assets, const InstallOptions& options,
                                       diag::DiagnosticSink& sink) {
  if (!is_version_hash(hash)) {
    return diag::make(codes::kSelect, "install needs a 40-hex version hash");
  }
  std::vector<Part> parts;
  for (const auto& local : assets) {
    if (part_present(family, hash, local.asset.content)) {
      continue;
    }
    if (local.asset.sha256.empty() && !options.accept_unverified) {
      return diag::make(codes::kUnverified, local.asset.filename + " has no known SHA-256; pass -accept-unverified");
    }
    parts.push_back(Part{local.asset.content, local.asset, local.file});
  }
  if (parts.empty()) {
    info(sink, family.name + " " + std::string{hash}.substr(0, 7) + " is already installed with these parts");
    return write_sources(family, hash);
  }
  if (auto status = check_consent(family, options); !status) {
    return status;
  }
  if (auto status = run_install(InstallTarget{fetcher_, decompressor_, family, family_dir(family), root_}, hash, parts,
                                options, sink);
      !status) {
    return status;
  }
  return write_sources(family, hash);
}

diag::Status PdkManager::enable(const Family& family, std::string_view hash) {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  if (!is_dir(version_dir)) {
    return diag::make(codes::kNotInstalled, family.name + " version " + std::string{hash} + " is not installed");
  }
  for (const auto& variant : family.variants) {  // check everything before changing anything
    const Path link = root_ / variant.name;
    auto target = host_.read_symlink(link);
    if (!target) {
      return target.error();
    }
    if (!target.value() && exists_no_follow(link)) {
      return diag::make(codes::kEnableConflict,
                        link.string() + " exists and is not a symbolic link; remove " + link.string() + " and retry");
    }
  }
  for (const auto& variant : family.variants) {
    const Path link = root_ / variant.name;
    std::error_code ec;
    stdfs::remove(link, ec);  // a symbolic link (checked above) or nothing
    const Path target = Path{"ciel"} / family.name / "versions" / std::string{hash} / variant.name;
    if (auto status = host_.make_symlink(target, link); !status) {
      return status;
    }
  }
  return write_text(family_dir(family) / "current", std::string{hash} + "\n");
}

diag::Status PdkManager::uninstall(const Family& family, std::string_view hash) {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  if (!is_dir(version_dir)) {
    return diag::make(codes::kNotInstalled, family.name + " version " + std::string{hash} + " is not installed");
  }
  std::error_code ec;
  if (current(family) == std::string{hash}) {
    for (const auto& variant : family.variants) {
      if (auto target = host_.read_symlink(root_ / variant.name); target && target.value()) {
        stdfs::remove(root_ / variant.name, ec);
      }
    }
    stdfs::remove(family_dir(family) / "current", ec);
  }
  stdfs::remove_all(version_dir, ec);
  stdfs::remove(family_dir(family) / ".hefesto" / (std::string{hash} + ".json"), ec);
  return ec ? diag::Status{io_error("cannot remove " + version_dir.string(), ec)} : diag::Status{diag::Done{}};
}

diag::Status PdkManager::remove_library(const Family& family, std::string_view hash, std::string_view library) {
  const Path version_dir = family_dir(family) / "versions" / std::string{hash};
  if (!is_dir(version_dir)) {
    return diag::make(codes::kNotInstalled, family.name + " version " + std::string{hash} + " is not installed");
  }
  std::error_code ec;
  for (const auto& variant : family.variants) {
    stdfs::remove_all(version_dir / variant.name / "libs.ref" / std::string{library}, ec);
  }
  return ec ? diag::Status{io_error("cannot remove library " + std::string{library}, ec)} : diag::Status{diag::Done{}};
}

std::optional<platform::Path> PdkManager::variant_dir(const Family& family, std::string_view variant,
                                                      std::string_view hash) const {
  for (const char* tool : {"ciel", "volare"}) {
    const Path dir = root_ / tool / family.name / "versions" / std::string{hash} / std::string{variant};
    if (is_dir(dir)) {
      return dir;
    }
  }
  return std::nullopt;
}

diag::Outcome<platform::Path> PdkManager::tech_file(const Selection& selection, std::string_view hash) const {
  if (selection.family == nullptr || selection.variant == nullptr) {
    return diag::make(codes::kSelect, "no PDK variant selected");
  }
  const auto dir = variant_dir(*selection.family, selection.variant->name, hash);
  const std::string& variant = selection.variant->name;
  if (!dir || !fs_.exists(*dir / "libs.tech" / "magic" / (variant + ".tech"))) {
    return diag::make(codes::kNotInstalled,
                      variant + " version " + std::string{hash} + " is not installed under " + root_.string());
  }
  return *dir / "libs.tech" / "magic" / (variant + ".tech");
}

namespace {

class ManagedPdkStore final : public platform::PdkStore {
 public:
  ManagedPdkStore(const PdkManager& manager, InstallHost& host, std::optional<PinnedVersion> pin)
      : manager_(manager), host_(host), pin_(std::move(pin)) {}

  std::vector<platform::Path> roots() const override { return {manager_.root()}; }

  diag::Outcome<platform::Path> find_technology(std::string_view name) override {
    if (auto selection = manager_.catalog().resolve(name); selection && selection.value().variant_explicit) {
      if (auto path = from_versions(selection.value())) {
        return *path;
      }
    }
    for (const auto& candidate : plain_candidates(name)) {
      if (is_file(candidate)) {
        return candidate;
      }
    }
    return diag::make(diag::codes::kPlatformTechNotFound,
                      "technology '" + std::string{name} + "' not found under " + manager_.root().string());
  }

 private:
  static bool is_file(const Path& path) {
    std::error_code ec;
    return stdfs::is_regular_file(path, ec);
  }

  // Pinned version first, then the enabled one.
  std::optional<platform::Path> from_versions(const Selection& selection) const {
    std::vector<std::string> hashes;
    if (pin_ && pin_->family == selection.family->name) {
      hashes.push_back(pin_->hash);
    }
    if (auto current = manager_.current(*selection.family)) {
      hashes.push_back(*current);
    }
    for (const auto& hash : hashes) {
      if (auto path = manager_.tech_file(selection, hash)) {
        return path.value();
      }
    }
    return std::nullopt;
  }

  std::vector<platform::Path> plain_candidates(std::string_view name) const {
    const std::string tech = std::string{name} + ".tech";
    std::vector<platform::Path> out{manager_.root() / std::string{name} / "libs.tech" / "magic" / tech};
    if (auto pdkpath = host_.environment("PDKPATH")) {
      const Path dir{*pdkpath};
      if (dir.filename() == name || dir.parent_path().filename() == name) {
        out.push_back(dir / "libs.tech" / "magic" / tech);
      }
    }
    return out;
  }

  const PdkManager& manager_;
  InstallHost& host_;
  std::optional<PinnedVersion> pin_;
};

}  // namespace

std::unique_ptr<platform::PdkStore> make_managed_pdk_store(const PdkManager& manager, InstallHost& host,
                                                           std::optional<PinnedVersion> pin) {
  return std::make_unique<ManagedPdkStore>(manager, host, std::move(pin));
}

SessionPdk make_session_pdk(platform::FileSystem& fs, platform::ProcessRunner& runner,
                            const std::optional<platform::Path>& root) {
  SessionPdk out;
  out.host = make_local_install_host();
  out.fetcher = runner.supported() ? make_process_fetcher(runner, out.host->environment("GITHUB_TOKEN"))
                                   : make_unsupported_fetcher();
  out.decompressor = make_default_decompressor(runner);
  out.manager = std::make_unique<PdkManager>(fs, *out.host, *out.fetcher, Catalog::builtin());
  out.manager->set_decompressor(out.decompressor.get());
  out.manager->set_root(out.manager->resolve_root(root));
  return out;
}

}  // namespace hefesto::pdk
