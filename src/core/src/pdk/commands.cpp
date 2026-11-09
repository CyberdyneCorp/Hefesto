// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `pdk` command (hefesto-pdk-manager "Headless and scripted operation",
// "Per-project version pinning", "Licenses and provenance"). Every
// operation returns a structured value and fails (non-zero exit in
// hefesto-cli) on error; nothing prompts: consent is given with
// -accept-license and -accept-unverified.

#include "hefesto/pdk/commands.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

#include "hefesto/app/project_file.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pdk/diag_codes.hpp"

namespace hefesto::pdk {

namespace {

using cmd::CommandResult;
using cmd::Value;

constexpr std::string_view kSynopsis =
    "pdk root | list | remote FAMILY ?-prereleases? | install VARIANT ?VERSION|latest? ?-lib NAME ...? "
    "?-accept-license? ?-accept-unverified? ?-enable? | enable FAMILY VERSION | uninstall FAMILY VERSION | "
    "remove-library FAMILY VERSION LIBRARY | pin VARIANT ?VERSION? ?-lib NAME ...? ?-project DIR? | "
    "info FAMILY VERSION  (common options: -root DIR, -source LOCATION, -only PREFIX, -unpack-factor N)";

const std::set<std::string, std::less<>> kFlags = {"-accept-license", "-accept-unverified", "-enable", "-force",
                                                   "-prereleases"};
const std::set<std::string, std::less<>> kValued = {"-root", "-source", "-lib", "-only", "-project", "-unpack-factor"};

CommandResult usage(std::string text = {}) {
  return CommandResult::failure(diag::make(diag::codes::kCmdUsage,
                                           (text.empty() ? std::string{} : text + "; ") + "usage: " + std::string{kSynopsis}));
}

CommandResult from_status(const diag::Status& status, Value value = {}) {
  return status ? CommandResult::success(std::move(value)) : CommandResult::failure(status.error());
}

// The words of one `pdk` invocation: positionals, flags and option values.
struct Args {
  std::vector<std::string> positional;
  std::set<std::string, std::less<>> flags;
  std::map<std::string, std::vector<std::string>, std::less<>> values;

  [[nodiscard]] bool has(std::string_view flag) const { return flags.contains(flag); }
  [[nodiscard]] std::optional<std::string> value(std::string_view name) const {
    const auto it = values.find(name);
    return it == values.end() || it->second.empty() ? std::nullopt : std::optional<std::string>{it->second.back()};
  }
  [[nodiscard]] std::vector<std::string> all(std::string_view name) const {
    const auto it = values.find(name);
    return it == values.end() ? std::vector<std::string>{} : it->second;
  }
};

diag::Outcome<Args> parse_args(const std::vector<std::string>& words) {
  Args args;
  for (std::size_t i = 0; i < words.size(); ++i) {
    const std::string& word = words[i];
    if (kFlags.contains(word)) {
      args.flags.insert(word);
    } else if (kValued.contains(word)) {
      if (i + 1 >= words.size()) {
        return diag::make(diag::codes::kCmdUsage, "pdk: " + word + " needs a value");
      }
      args.values[word].push_back(words[++i]);
    } else if (word.starts_with('-') && word.size() > 1) {
      return diag::make(diag::codes::kCmdUsage, "pdk: unknown option " + word);
    } else {
      args.positional.push_back(word);
    }
  }
  return args;
}

Value string_list(const std::vector<std::string>& items) {
  Value::List out;
  for (const auto& item : items) {
    out.emplace_back(item);
  }
  return out;
}

std::string state_of(const InstalledVersion& v) {
  if (v.read_only) {
    return "read-only";
  }
  return v.enabled ? "enabled" : "installed";
}

// One `pdk` invocation with its host mechanisms and manager.
class PdkRun {
 public:
  PdkRun(cmd::CommandContext& ctx, const CommandHosts& hosts, Args args)
      : ctx_(ctx), args_(std::move(args)), host_(make_host(hosts)), fetcher_(make_fetcher(hosts, ctx, *host_)),
        decompressor_(make_decompressor(hosts, ctx)),
        manager_(ctx.services().files, *host_, *fetcher_, Catalog::builtin()) {
    manager_.set_decompressor(decompressor_.get());
    manager_.set_root(manager_.resolve_root(explicit_root()));
  }

  CommandResult run() {
    const std::string sub = args_.positional.empty() ? std::string{} : args_.positional.front();
    using Handler = CommandResult (PdkRun::*)();
    static const std::map<std::string, Handler, std::less<>> kSubcommands = {
        {"root", &PdkRun::root},       {"list", &PdkRun::list},       {"remote", &PdkRun::remote},
        {"install", &PdkRun::install}, {"enable", &PdkRun::enable},   {"uninstall", &PdkRun::uninstall},
        {"pin", &PdkRun::pin},         {"info", &PdkRun::info},       {"remove-library", &PdkRun::remove_library},
    };
    const auto it = kSubcommands.find(sub);
    if (it == kSubcommands.end()) {
      return usage(sub.empty() ? std::string{} : "pdk: unknown subcommand " + sub);
    }
    return (this->*(it->second))();
  }

 private:
  static std::unique_ptr<InstallHost> make_host(const CommandHosts& hosts) {
    return hosts.install_host ? hosts.install_host() : make_local_install_host();
  }
  // GITHUB_TOKEN, when set, raises the GitHub API rate limit (CI).
  static std::unique_ptr<Fetcher> make_fetcher(const CommandHosts& hosts, cmd::CommandContext& ctx, InstallHost& host) {
    if (hosts.fetcher) {
      return hosts.fetcher(ctx.services());
    }
    return ctx.services().processes.supported()
               ? make_process_fetcher(ctx.services().processes, host.environment("GITHUB_TOKEN"))
               : make_unsupported_fetcher();
  }
  static std::unique_ptr<Decompressor> make_decompressor(const CommandHosts& hosts, cmd::CommandContext& ctx) {
    return hosts.decompressor ? hosts.decompressor(ctx.services()) : make_default_decompressor(ctx.services().processes);
  }

  // -root, else the `pdk-root` setting ("Install layout compatible with ciel", step 1).
  [[nodiscard]] std::optional<platform::Path> explicit_root() const {
    if (auto root = args_.value("-root")) {
      return platform::Path{*root};
    }
    if (const auto* editor = ctx_.editor()) {
      if (const auto chosen = editor->settings.effective(app::kPdkRoot); chosen && !chosen->value.empty()) {
        return platform::Path{chosen->value};
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] CatalogSource source() const {
    CatalogSource src;
    if (auto location = args_.value("-source")) {
      src.location = *location;
    }
    return src;
  }

  [[nodiscard]] diag::Outcome<Selection> selection(std::size_t index) const {
    if (args_.positional.size() <= index) {
      return diag::make(diag::codes::kCmdUsage, "pdk " + args_.positional.front() + ": missing PDK family or variant");
    }
    return manager_.catalog().resolve(args_.positional[index]);
  }

  [[nodiscard]] std::optional<std::string> positional(std::size_t index) const {
    return args_.positional.size() > index ? std::optional<std::string>{args_.positional[index]} : std::nullopt;
  }

  CommandResult root() { return CommandResult::success(manager_.root().string()); }

  CommandResult list() {
    auto installed = manager_.installed();
    if (!installed) {
      return CommandResult::failure(installed.error());
    }
    Value::List rows;
    for (const auto& v : installed.value()) {
      rows.emplace_back(Value::List{v.variant, v.version.hash, v.version.commit_date.value_or(""), state_of(v),
                                    string_list(v.libraries)});
    }
    return CommandResult::success(std::move(rows));
  }

  CommandResult remote() {
    auto sel = selection(1);
    if (!sel) {
      return CommandResult::failure(sel.error());
    }
    auto versions = manager_.remote_versions(*sel.value().family, source());
    if (!versions) {
      return CommandResult::failure(versions.error());
    }
    Value::List rows;
    for (const auto& v : versions.value()) {
      if (!v.prerelease || args_.has("-prereleases")) {
        rows.emplace_back(Value::List{v.hash, v.commit_date.value_or(""), v.prerelease ? "prerelease" : "stable"});
      }
    }
    return CommandResult::success(std::move(rows));
  }

  // VERSION, or the newest stable remote version for `latest` / no version.
  diag::Outcome<PdkVersion> wanted_version(const Family& family, std::size_t index) {
    const auto given = positional(index);
    if (given && *given != "latest") {
      if (!is_version_hash(*given)) {
        return diag::make(diag::codes::kCmdUsage, "pdk: '" + *given + "' is not a 40-hex version hash");
      }
      PdkVersion v{*given, std::nullopt, std::nullopt, false};
      if (auto remote = manager_.remote_versions(family, source())) {
        const auto& list = remote.value();
        const auto it = std::find_if(list.begin(), list.end(), [&](const PdkVersion& r) { return r.hash == *given; });
        v = it == list.end() ? v : *it;
      }
      return v;
    }
    auto remote = manager_.remote_versions(family, source());
    if (!remote) {
      return remote.error();
    }
    for (const auto& v : remote.value()) {
      if (!v.prerelease) {
        return v;
      }
    }
    return diag::make(codes::kUnknownVersion, "the catalog lists no stable version of " + family.name);
  }

  [[nodiscard]] InstallOptions install_options(const PdkVersion& version) const {
    InstallOptions options;
    options.source = source();
    options.accept_license = args_.has("-accept-license");
    options.accept_unverified = args_.has("-accept-unverified");
    options.commit_date = version.commit_date;
    options.include_prefixes = args_.all("-only");
    if (auto factor = args_.value("-unpack-factor")) {
      options.unpack_factor = std::max<std::uint64_t>(1, std::strtoull(factor->c_str(), nullptr, 10));
    }
    return options;
  }

  CommandResult install() {
    auto sel = selection(1);
    if (!sel) {
      return CommandResult::failure(sel.error());
    }
    // Libraries are checked before any download ("Library sets per variant").
    if (auto libs = manager_.catalog().resolve_libraries(*sel.value().variant, args_.all("-lib")); !libs) {
      return CommandResult::failure(libs.error());
    }
    auto version = wanted_version(*sel.value().family, 2);
    if (!version) {
      return CommandResult::failure(version.error());
    }
    const std::string& hash = version.value().hash;
    auto status = manager_.install(sel.value(), hash, args_.all("-lib"), install_options(version.value()), ctx_.sink());
    if (status && args_.has("-enable")) {
      status = manager_.enable(*sel.value().family, hash);
    }
    return from_status(status, hash);
  }

  // FAMILY|VARIANT VERSION, the arguments of enable, uninstall, info.
  diag::Outcome<std::pair<const Family*, std::string>> family_and_hash() const {
    auto sel = selection(1);
    if (!sel) {
      return sel.error();
    }
    const auto hash = positional(2);
    if (!hash || !is_version_hash(*hash)) {
      return diag::make(diag::codes::kCmdUsage, "pdk " + args_.positional.front() + ": needs a 40-hex version hash");
    }
    return std::pair{sel.value().family, *hash};
  }

  CommandResult enable() {
    auto target = family_and_hash();
    if (!target) {
      return CommandResult::failure(target.error());
    }
    return from_status(manager_.enable(*target.value().first, target.value().second), target.value().second);
  }

  CommandResult uninstall() {
    auto target = family_and_hash();
    if (!target) {
      return CommandResult::failure(target.error());
    }
    return from_status(manager_.uninstall(*target.value().first, target.value().second));
  }

  CommandResult remove_library() {
    auto target = family_and_hash();
    const auto library = positional(3);
    if (!target || !library) {
      return target ? usage("pdk remove-library: missing library") : CommandResult::failure(target.error());
    }
    return from_status(manager_.remove_library(*target.value().first, target.value().second, *library));
  }

  CommandResult info() {
    auto target = family_and_hash();
    if (!target) {
      return CommandResult::failure(target.error());
    }
    auto record = manager_.provenance(*target.value().first, target.value().second);
    if (!record) {
      return CommandResult::failure(record.error());
    }
    const Provenance& p = record.value();
    Value::List assets;
    for (const auto& [name, digest] : p.assets) {
      assets.emplace_back(Value::List{name, digest.empty() ? "unverified" : digest});
    }
    return CommandResult::success(Value::List{
        Value::List{"repository", p.repository}, Value::List{"version", p.version},
        Value::List{"date", p.date.value_or("")}, Value::List{"source", p.source},
        Value::List{"license", p.license}, Value::List{"trusted", p.trusted ? "yes" : "no"},
        Value::List{"assets", std::move(assets)}});
  }

  // The newest installed stable version of the variant ("A new project
  // SHALL pin the newest installed stable version").
  std::optional<std::string> newest_installed(const Selection& sel) const {
    auto installed = manager_.installed();
    if (!installed) {
      return std::nullopt;
    }
    for (const auto& v : installed.value()) {
      if (v.family == sel.family->name && v.variant == sel.variant->name) {
        return v.version.hash;
      }
    }
    return std::nullopt;
  }

  CommandResult pin() {
    auto sel = selection(1);
    if (!sel) {
      return CommandResult::failure(sel.error());
    }
    auto libs = manager_.catalog().resolve_libraries(*sel.value().variant, args_.all("-lib"));
    if (!libs) {
      return CommandResult::failure(libs.error());
    }
    const auto hash = positional(2) ? positional(2) : newest_installed(sel.value());
    if (!hash || !is_version_hash(*hash)) {
      return usage("pdk pin: no installed version of " + sel.value().variant->name + "; give a 40-hex version");
    }
    return write_pin(app::PdkPin{sel.value().family->name, sel.value().variant->name, *hash, libs.value()});
  }

  CommandResult write_pin(const app::PdkPin& pin) {
    auto& fs = ctx_.services().files;
    const platform::Path folder{args_.value("-project").value_or(".")};
    app::ProjectFile project;
    if (fs.exists(folder / std::string{app::kProjectFileName})) {
      auto loaded = app::load_project(fs, folder);
      if (!loaded) {
        return CommandResult::failure(loaded.error());
      }
      project = std::move(loaded).value();
    }
    if (project.pdk && project.pdk->version != pin.version) {
      ctx_.emit(diag::make(codes::kRepin, "Re-pinning from " + project.pdk->version.substr(0, 7) + " to " +
                                              pin.version.substr(0, 7) +
                                              ": recorded DRC, extraction and sign-off results become stale."));
    }
    if (!manager_.variant_dir(*manager_.catalog().find_family(pin.family), pin.variant, pin.version)) {
      ctx_.emit(diag::make(codes::kInfo, pin.variant + " " + pin.version + " is not installed; install it with `pdk "
                                         "install " + pin.variant + " " + pin.version + "`"));
    }
    project.pdk = pin;
    return from_status(app::save_project(fs, folder, project), pin.version);
  }

  cmd::CommandContext& ctx_;
  Args args_;
  std::unique_ptr<InstallHost> host_;
  std::unique_ptr<Fetcher> fetcher_;
  std::unique_ptr<Decompressor> decompressor_;
  PdkManager manager_;
};

}  // namespace

bool register_pdk_commands(cmd::CommandRegistry& registry, CommandHosts hosts) {
  auto handler = [hosts = std::move(hosts)](cmd::CommandContext& ctx, const cmd::Invocation& inv) {
    auto args = parse_args(inv.args);
    if (!args) {
      return CommandResult::failure(args.error());
    }
    if (args.value().positional.empty()) {
      return usage();
    }
    return PdkRun{ctx, hosts, std::move(args).value()}.run();
  };
  return !registry.add(cmd::CommandSpec{
      "pdk", std::string{kSynopsis},
      "Lists, downloads (verified), installs, enables, pins, describes and removes PDK versions in the ciel layout "
      "under the PDK root (hefesto-pdk-manager).",
      std::move(handler)});
}

}  // namespace hefesto::pdk
