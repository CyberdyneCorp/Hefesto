// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK installation (capability hefesto-pdk-manager: "PDK catalog", "GitHub
// releases source compatibility", "Download integrity verification", "Safe
// archive unpacking", "Install layout compatible with ciel", "Incremental
// library downloads", "Enabling a version", "Uninstall and cleanup",
// "Licenses and provenance", "Headless and scripted operation"; desktop
// parts of hefesto-files-and-pdks "PDK store").
//
// The core never opens sockets: downloads go through a host-supplied
// Fetcher. hefesto-cli and the desktop install the in-process HTTPS Fetcher
// of hefesto-licensing D-13 (URLSession on macOS, libcurl on Linux;
// hefesto::platform_net, outside the core). A host that supplies none but
// can run processes gets the curl-process Fetcher over the platform
// ProcessRunner (make_process_fetcher), else the unsupported one.
// The manager works on the local disk (std::filesystem): installs stream
// archives of hundreds of megabytes, which platform::FileSystem (whole-file
// reads) cannot do. Environment, home directory and symbolic links go
// through InstallHost.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/catalog.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/process_runner.hpp"
#include "hefesto/platform/storage.hpp"

namespace hefesto::pdk {

/// Downloads bytes. `get_text` fails with PDK-FETCH-002 for an HTTP 404 and
/// PDK-FETCH-001 for any other failure. `download` resumes a partial file
/// at `to` (HTTP range request) and replaces nothing else; when `progress`
/// is set it is called with (bytes in `to` so far, total or 0 when unknown),
/// possibly from a worker thread, never concurrently and never after
/// `download` returned.
class Fetcher {
 public:
  Fetcher() = default;
  Fetcher(const Fetcher&) = delete;
  Fetcher& operator=(const Fetcher&) = delete;
  virtual ~Fetcher() = default;

  [[nodiscard]] virtual diag::Outcome<std::string> get_text(std::string_view url) = 0;
  [[nodiscard]] virtual diag::Status download(std::string_view url, const platform::Path& to,
                                              const std::function<void(std::uint64_t, std::uint64_t)>& progress) = 0;
};

/// A Fetcher that always fails with PLATFORM-UNSUPPORTED-001 (headless default).
[[nodiscard]] std::unique_ptr<Fetcher> make_unsupported_fetcher();

/// A Fetcher that runs `curl` through `runner` (fallback for hosts without
/// the platform_net HTTPS Fetcher; HTTPS with the system
/// trust store, `file://` for local catalogs, `-C -` resume). A GitHub API
/// token is taken from `github_token` when set (passed on stdin, never argv).
[[nodiscard]] std::unique_ptr<Fetcher> make_process_fetcher(platform::ProcessRunner& runner,
                                                            std::optional<std::string> github_token = std::nullopt);

/// Decompresses a `.tar.zst` asset into a plain tar file, failing with
/// PDK-UNPACK-002 when the output would exceed `max_bytes` and
/// PDK-UNPACK-003 for invalid input.
class Decompressor {
 public:
  Decompressor() = default;
  Decompressor(const Decompressor&) = delete;
  Decompressor& operator=(const Decompressor&) = delete;
  virtual ~Decompressor() = default;

  [[nodiscard]] virtual diag::Status decompress(const platform::Path& in, const platform::Path& out,
                                                std::uint64_t max_bytes) = 0;
};

/// The streaming zstd library decoder (hefesto::zstd). It stops and reports
/// PDK-UNPACK limit as soon as the output exceeds `max_bytes`. `runner` is
/// unused (kept for API stability).
[[nodiscard]] std::unique_ptr<Decompressor> make_default_decompressor(platform::ProcessRunner& runner);
[[nodiscard]] std::unique_ptr<Decompressor> make_default_decompressor();
/// True: the zstd library is always linked into the core.
[[nodiscard]] bool have_zstd_library() noexcept;

/// Filesystem operations beyond platform::FileSystem.
class InstallHost {
 public:
  InstallHost() = default;
  InstallHost(const InstallHost&) = delete;
  InstallHost& operator=(const InstallHost&) = delete;
  virtual ~InstallHost() = default;

  [[nodiscard]] virtual diag::Status make_symlink(const platform::Path& target, const platform::Path& link) = 0;
  /// The link's target; std::nullopt when `link` is missing or not a symbolic link.
  [[nodiscard]] virtual diag::Outcome<std::optional<platform::Path>> read_symlink(const platform::Path& link) = 0;
  [[nodiscard]] virtual std::optional<std::string> environment(std::string_view name) = 0;  // PDK_ROOT
  [[nodiscard]] virtual platform::Path home_directory() = 0;
};

/// std::filesystem symbolic links, std::getenv and $HOME.
[[nodiscard]] std::unique_ptr<InstallHost> make_local_install_host();

/// SHA-256 of `data` as 64 lower-case hex digits ("Download integrity verification").
[[nodiscard]] std::string sha256_hex(std::string_view data);

/// One installed version of a variant, as found on disk.
struct InstalledVersion {
  std::string family;
  std::string variant;
  PdkVersion version;
  std::vector<std::string> libraries;  // libs.ref/* of this variant, sorted
  bool enabled = false;     // named by <root>/ciel/<family>/current
  bool read_only = false;   // legacy volare layout
  platform::Path path;      // <root>/ciel/<family>/versions/<hash>/<variant>
};

/// Where versions and assets come from ("PDK catalog", "GitHub releases
/// source compatibility"): a ciel static-web base URL (`https://...` or
/// `file://...`) or `github:<owner>/<repo>`.
struct CatalogSource {
  std::string location = std::string{kDefaultLocation};
  /// No Hefesto catalog signature exists yet, so every source is unverified
  /// for script trust; digests still come from the source.
  static constexpr std::string_view kDefaultLocation = "github:fossi-foundation/ciel-releases";

  [[nodiscard]] bool is_github() const noexcept { return location.starts_with("github:"); }
};

/// Phases of one asset in PdkManager::install, in order. `finalize` is
/// reported once per install, after the last asset, while the staged files
/// move into the version directory.
enum class InstallPhase : std::uint8_t { download, verify, unpack, finalize };

[[nodiscard]] std::string_view install_phase_name(InstallPhase phase) noexcept;
[[nodiscard]] std::optional<InstallPhase> parse_install_phase(std::string_view name) noexcept;

/// One install progress event (InstallOptions::progress).
///   download  bytes of the asset file so far; total = catalog size (0 = unknown)
///   verify    0 at the start, `total` when the size and SHA-256 matched
///   unpack    0 at the start, `total` (archive bytes) when unpacked
///   finalize  part = parts; bytes = total = 0
/// An asset found complete in the download cache reports one download event
/// with bytes = total.
struct InstallProgress {
  InstallPhase phase = InstallPhase::download;
  std::string asset;         // asset file name, e.g. "common.tar.zst"; empty for finalize
  std::size_t part = 0;      // 1-based index of the asset in this install
  std::size_t parts = 0;     // number of assets this install fetches
  std::uint64_t bytes = 0;
  std::uint64_t total = 0;

  friend bool operator==(const InstallProgress&, const InstallProgress&) = default;
};

/// Receives InstallProgress events. Called on the thread running install(),
/// except while a Fetcher downloads: a Fetcher may then call it from its own
/// worker thread. Calls never overlap and all of them happen before
/// install() returns, so the callback may capture the caller's locals by
/// reference, but must not block for long (it slows the download).
using InstallProgressFn = std::function<void(const InstallProgress&)>;

struct InstallOptions {
  CatalogSource source;
  bool accept_license = false;     // headless: accept the family's upstream license
  bool accept_unverified = false;  // install assets that have no known SHA-256
  /// Unpacked-size bound per asset, as a multiple of its catalog size.
  std::uint64_t unpack_factor = 4;
  /// Polled between assets; returning true cancels the install cleanly.
  std::function<bool()> cancelled;
  /// Commit date of the version when known (recorded in the provenance).
  std::optional<std::string> commit_date;
  /// Developer and test option: unpack only the archive files whose names
  /// start with one of these prefixes (e.g. `sky130A/libs.tech/magic/`);
  /// empty = everything. A filtered install is recorded as partial in the
  /// provenance. Used to keep the test PDK cache small (design D1).
  std::vector<std::string> include_prefixes;
  /// Optional progress observer (see InstallProgressFn).
  InstallProgressFn progress;
};

/// "Licenses and provenance": what the manager recorded for an installed
/// version (`<root>/ciel/<family>/.hefesto/<hash>.json`). Versions installed
/// by ciel or volare have no record: their source is "external" and the
/// repository comes from the family table.
struct Provenance {
  std::string family;
  std::string repository;   // open_pdks or IHP-Open-PDK
  std::string version;      // full hash
  std::optional<std::string> date;
  std::string source;       // catalog location, or "external"
  std::string license;      // SPDX identifier
  bool trusted = false;     // signed catalog or pack ("Script trust for managed PDKs")
  std::vector<std::pair<std::string, std::string>> assets;  // file name, SHA-256 ("" = unverified)
  std::vector<std::string> partial;                         // include prefixes of a filtered install
  /// When the user trusted this version's scripts (`YYYY-MM-DD`, UTC;
  /// set_trusted), shown with "scripts trusted" in the PDK panel; nullopt
  /// when not trusted or trusted before the date was recorded.
  std::optional<std::string> trusted_date;
};

/// True when `source` carries a verified Hefesto catalog signature. No
/// signed catalog exists yet (Q-TAPEOUT-3), so every source is unverified
/// and installs need explicit consent ("PDK catalog").
[[nodiscard]] bool source_verified(const CatalogSource& source) noexcept;

/// A project's pin as the store needs it ("Per-project version pinning").
struct PinnedVersion {
  std::string family;
  std::string variant;
  std::string hash;
};

/// An asset already on the local disk (offline pack import): it is verified
/// against `asset.size` and `asset.sha256` in place and never downloaded.
struct LocalAsset {
  Asset asset;
  platform::Path file;
};

/// The PDK manager over one PDK root.
class PdkManager {
 public:
  PdkManager(platform::FileSystem& fs, InstallHost& host, Fetcher& fetcher, Catalog catalog);

  /// Optional: the decompressor used by install (none = install fails with PDK-UNPACK-003).
  void set_decompressor(Decompressor* decompressor) noexcept { decompressor_ = decompressor; }

  /// "Install layout compatible with ciel": explicit root, PDK_ROOT, ~/.ciel
  /// (empty when the host has no home directory).
  [[nodiscard]] platform::Path resolve_root(const std::optional<platform::Path>& explicit_root) const;
  void set_root(platform::Path root) { root_ = std::move(root); }
  [[nodiscard]] const platform::Path& root() const noexcept { return root_; }

  [[nodiscard]] const Catalog& catalog() const noexcept { return catalog_; }
  /// Remote versions of `family`, newest first (prereleases included; the
  /// caller filters them unless the user asked for prereleases).
  [[nodiscard]] diag::Outcome<std::vector<PdkVersion>> remote_versions(const Family& family,
                                                                       const CatalogSource& source);
  /// The assets of one version (GitHub: family tag first, then variant tags).
  [[nodiscard]] diag::Outcome<std::vector<Asset>> version_assets(const Family& family, std::string_view hash,
                                                                 const CatalogSource& source);
  /// Every installed version and variant under the root (ciel layout, then
  /// the read-only volare layout), sorted by family, newest first, variant.
  [[nodiscard]] diag::Outcome<std::vector<InstalledVersion>> installed() const;

  /// Downloads (verified), unpacks safely and installs the variant's
  /// common part and `libraries` (a request resolved by
  /// Catalog::resolve_libraries) of version `hash`; parts already present
  /// are skipped ("Incremental library downloads").
  [[nodiscard]] diag::Status install(const Selection& selection, std::string_view hash,
                                     const std::vector<std::string>& libraries, const InstallOptions& options,
                                     diag::DiagnosticSink& sink);
  /// Installs local assets of version `hash` ("Offline PDK packs"): parts
  /// already present are skipped, the rest are verified (PDK-VERIFY-001 on
  /// a size or SHA-256 mismatch, nothing installed), unpacked and committed
  /// as by install(). The license and unverified-source consent rules apply.
  [[nodiscard]] diag::Status install_local(const Family& family, std::string_view hash,
                                           const std::vector<LocalAsset>& assets, const InstallOptions& options,
                                           diag::DiagnosticSink& sink);
  [[nodiscard]] diag::Status enable(const Family& family, std::string_view hash);
  [[nodiscard]] diag::Status uninstall(const Family& family, std::string_view hash);
  /// Removes `libs.ref/<library>` under every variant of the version.
  [[nodiscard]] diag::Status remove_library(const Family& family, std::string_view hash, std::string_view library);
  /// "Optional file deduplication" (ciel's optimize step): walks the
  /// version's variant directories in catalog order, files in path order,
  /// and replaces each regular file whose SHA-256 equals an earlier file's
  /// with a relative symbolic link to that file. Never crosses versions.
  /// Returns the number of files replaced; PDK-INSTALL-004 when the version
  /// is not installed. Off unless a front end asks (never on iPadOS).
  [[nodiscard]] diag::Outcome<std::size_t> deduplicate(const Family& family, std::string_view hash);
  /// The recorded provenance of an installed version; PDK-INSTALL-004 when
  /// the version is not installed.
  [[nodiscard]] diag::Outcome<Provenance> provenance(const Family& family, std::string_view hash) const;
  /// Records `trusted` in the provenance record of an installed version
  /// (hefesto-security "Consent before implicit script execution": trust is
  /// recorded per PDK and version; the user chose "trust this version" in
  /// the consent prompt). A version installed by ciel or volare gets a
  /// record with source "external". Trusting also records the date
  /// (Provenance::trusted_date: `date`, else today in UTC); revoking removes
  /// it. PDK-INSTALL-004 when the version is not installed. Owner: PDKINT
  /// (openspec/changes/m4-scripting design.md D2).
  [[nodiscard]] diag::Status set_trusted(const Family& family, std::string_view hash, bool trusted,
                                         std::string_view date = {});
  /// The enabled hash of `family` (<root>/ciel/<family>/current), if any.
  [[nodiscard]] std::optional<std::string> current(const Family& family) const;

  /// Path of the technology file of an installed version/variant
  /// (libs.tech/magic/<variant>.tech), for project pins; PDK-INSTALL-004
  /// when that version or variant is not installed.
  [[nodiscard]] diag::Outcome<platform::Path> tech_file(const Selection& selection, std::string_view hash) const;
  /// Directory of an installed version/variant (ciel layout, else volare).
  [[nodiscard]] std::optional<platform::Path> variant_dir(const Family& family, std::string_view variant,
                                                          std::string_view hash) const;

 private:
  [[nodiscard]] platform::Path family_dir(const Family& family) const;
  [[nodiscard]] diag::Status check_license(const Family& family, const InstallOptions& options);
  [[nodiscard]] diag::Status write_sources(const Family& family, std::string_view hash);
  [[nodiscard]] bool part_present(const Family& family, std::string_view hash, std::string_view content) const;
  [[nodiscard]] diag::Status check_consent(const Family& family, const InstallOptions& options);

  platform::FileSystem& fs_;
  InstallHost& host_;
  Fetcher& fetcher_;
  Decompressor* decompressor_ = nullptr;
  Catalog catalog_;
  platform::Path root_;
};

/// A platform::PdkStore that resolves technologies (variant names) from the
/// manager's installs: the pinned version first, then the enabled one, then
/// `<root>/<name>` (installs made by ciel, volare or open_pdks), then
/// `$PDKPATH` when it names that variant's directory.
[[nodiscard]] std::unique_ptr<platform::PdkStore> make_managed_pdk_store(const PdkManager& manager, InstallHost& host,
                                                                         std::optional<PinnedVersion> pin = std::nullopt);

/// The PDK manager of one front-end session with its host mechanisms
/// (hefesto-files-and-pdks "PDK store"): the local install host, `fetcher`
/// (front ends pass platform_net::make_http_fetcher), the zstd decoder, and
/// a manager whose root is `root`, else PDK_ROOT, else ~/.ciel. Keep it
/// alive as long as the PortableServices that hold the store made from it:
///
///   pdk_ = pdk::make_session_pdk(services.files(), platform_net::make_http_fetcher(options), pdk_root);
///   services.set_pdk_store(pdk::make_managed_pdk_store(*pdk_.manager, *pdk_.host));
///   // ... then take services.view()
struct SessionPdk {
  std::unique_ptr<InstallHost> host;
  std::unique_ptr<Fetcher> fetcher;
  std::unique_ptr<Decompressor> decompressor;
  std::unique_ptr<PdkManager> manager;
};

[[nodiscard]] SessionPdk make_session_pdk(platform::FileSystem& fs, std::unique_ptr<Fetcher> fetcher,
                                          const std::optional<platform::Path>& root = std::nullopt);
/// The same with the curl-process Fetcher when `runner` can run processes
/// (GITHUB_TOKEN from the environment), else the unsupported one.
[[nodiscard]] SessionPdk make_session_pdk(platform::FileSystem& fs, platform::ProcessRunner& runner,
                                          const std::optional<platform::Path>& root = std::nullopt);

}  // namespace hefesto::pdk
