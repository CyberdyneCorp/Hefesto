// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The startup sequence shared by hefesto-cli and the desktop app
// (app-startup "Startup sequence" steps 6-11, "Startup file order",
// "Default technology selection", "Command-line file arguments", "Crash
// recovery at startup"; hefesto-settings "Layered settings resolution",
// "Magic startup-file compatibility"; hefesto-pdk-manager "Per-project
// version pinning"). The order of technology choices and the messages
// follow Magic 8.3.694 (fc5daa9e) utils/main.c:835-1258; Copyright (C)
// 1985, 1990 Regents of the University of California, see
// THIRD_PARTY_NOTICES.md. Technologies and cells are loaded through the
// `tech load` and `load` commands so the TECH and CELL rules apply
// unchanged and the actions are logged like any command.

#include "hefesto/app/startup.hpp"

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/recent_projects.hpp"
#include "hefesto/app/settings_layers.hpp"
#include "hefesto/cell/fonts.hpp"
#include "hefesto/cell/host.hpp"
#include "hefesto/cell/search_path.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/script.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/tech/loader.hpp"

namespace hefesto::app {

namespace {

using platform::Path;

// A host without environment, home or symbolic links (iPadOS, tests).
class NullInstallHost final : public pdk::InstallHost {
 public:
  diag::Status make_symlink(const Path& /*target*/, const Path& link) override {
    return diag::make(diag::codes::kPlatformUnsupported, "symbolic links are not available: " + link.string());
  }
  diag::Outcome<std::optional<Path>> read_symlink(const Path& /*link*/) override { return std::optional<Path>{}; }
  std::optional<std::string> environment(std::string_view /*name*/) override { return std::nullopt; }
  Path home_directory() override { return {}; }
};

SettingLayer layer_of_role(std::string_view role) {
  if (role == "pdk") {
    return SettingLayer::pdk;
  }
  return role == "user" || role == "local" ? SettingLayer::user : SettingLayer::builtin;
}

std::string deprecation_note(const StartupFile& file) {
  return file.role == "user"
             ? R"(Note:  Use of the file name "~/.magic" is deprecated.  Please change this to "~/.magicrc".)"
             : R"(Note:  Use of the file name ".magic" is deprecated.  Please change this to ".magicrc".)";
}

// The command that reads a non-cell file argument.
std::optional<cmd::Invocation> reader_for(const FileArgument& file) {
  switch (file.kind) {
    case FileKind::lef:
      return cmd::Invocation{"lef", {"read", file.name}, cmd::Origin::cli, std::nullopt};
    case FileKind::def:
      return cmd::Invocation{"def", {"read", file.name}, cmd::Origin::cli, std::nullopt};
    case FileKind::cif:
      return cmd::Invocation{"cif", {"read", file.name}, cmd::Origin::cli, std::nullopt};
    case FileKind::gds:
      return cmd::Invocation{"gds", {"read", file.name}, cmd::Origin::cli, std::nullopt};
    case FileKind::cell:
      return cmd::Invocation{"load", {file.name}, cmd::Origin::cli, std::nullopt};
    case FileKind::script:
      return std::nullopt;
  }
  return std::nullopt;
}

// One run of the startup sequence.
class Startup {
 public:
  Startup(cmd::Dispatcher& dispatcher, editor::EditorState& state, settings::ProjectSettings& typed,
          platform::FileSystem& fs, const StartupOptions& options, StartupEnvironment env, diag::DiagnosticSink& sink)
      : dispatcher_(dispatcher), state_(state), typed_(typed), fs_(fs), options_(options), env_(std::move(env)),
        sink_(sink), host_(env_.host != nullptr ? *env_.host : null_host_), fetcher_(pdk::make_unsupported_fetcher()),
        manager_(fs, host_, *fetcher_, pdk::Catalog::builtin()) {}

  StartupResult run() {
    settings_layers();
    configure_pdk_root();
    configure_paths();
    const auto files = startup_files(fs_, options_, with_pdk_startup_file());
    result_.startup_files = files;
    auto next = files.begin();
    for (; next != files.end() && next->role == "pre"; ++next) {
      apply_startup_file(*next);
    }
    load_override_technology();
    for (; next != files.end(); ++next) {
      apply_startup_file(*next);
    }
    select_technology();
    discover_pdk_libraries();
    result_.technology = state_.technology->name();
    open_startup_window();
    if (options_.recover && !options_.no_window) {
      recover();
    } else {
      load_files();
    }
    set_box();
    return std::move(result_);
  }

 private:
  void report(std::string_view code, std::string text) { sink_.emit(diag::make(code, std::move(text))); }

  // Built-in, user preferences, project and session layers; the effective
  // compat settings go to the typed store before any technology loads.
  void settings_layers() {
    install_builtin_defaults(state_.settings);
    if (env_.user_preferences) {
      if (auto status = load_preferences(fs_, *env_.user_preferences, state_.settings); !status) {
        sink_.emit(status.error());
      }
    }
    if (env_.project_folder) {
      auto project = load_project(fs_, *env_.project_folder);
      if (project) {
        apply_project(state_.settings, project.value());
        result_.project = std::move(project).value();
      } else {
        sink_.emit(project.error());
      }
    }
    for (const auto& [name, value] : options_.session_settings) {
      state_.settings.set(SettingLayer::session, name, value);
    }
    apply_effective(state_.settings, typed_);
    record_recent_project();
  }

  // The recent-projects list: published for the `pdk` command and updated
  // with the project opened by this startup.
  void record_recent_project() {
    if (!env_.recent_projects) {
      return;
    }
    state_.settings.set(SettingLayer::session, kRecentProjectsFile, env_.recent_projects->string());
    if (result_.project && env_.project_folder) {
      if (auto status = note_recent_project(fs_, *env_.recent_projects, *env_.project_folder); !status) {
        report(codes::kRecentProjects, "cannot update the recent-projects list: " + status.error().text);
      }
    }
  }

  // An explicit root from the front end becomes a session setting, so the
  // `pdk` command and the PDK store of later commands use it too.
  void configure_pdk_root() {
    std::optional<Path> root = env_.pdk_root;
    if (root) {
      state_.settings.set(SettingLayer::session, kPdkRoot, root->string());
    } else {
      if (const auto chosen = state_.settings.effective(kPdkRoot); chosen && !chosen->value.empty()) {
        root = Path{chosen->value};
      }
    }
    manager_.set_root(manager_.resolve_root(root));
  }

  // Search paths: the project's (relative to its folder) and the library path.
  void configure_paths() {
    install_path_variables();
    if (result_.project) {
      for (const auto& entry : result_.project->cell_search_paths) {
        const Path path{entry};
        state_.search_paths.path.push_back(path.is_absolute() ? entry : (*env_.project_folder / path).string());
      }
    }
    state_.search_paths.library = cell_library_path(options_.technology);
    state_.search_paths.system = system_library_path();
    load_default_fonts();
  }

  // Path-variable expansion: environment (through the host), CAD_ROOT, ~.
  void install_path_variables() {
    state_.path_variables.install_root = env_.install_root;
    // The lookup outlives this Startup object (later `load` and `path`
    // commands use it), so it captures only the caller-owned host and values,
    // never `this`.
    pdk::InstallHost* const host = env_.host;
    const Path install_root = env_.install_root;
    state_.path_variables.variable = [host, install_root](std::string_view name) -> std::optional<std::string> {
      if (host != nullptr) {
        if (auto value = host->environment(name)) {
          return value;
        }
      }
      if (name == "CAD_ROOT" && !install_root.empty()) {
        return install_root.string();
      }
      return std::nullopt;
    };
    const Path home = env_.home;
    state_.path_variables.home_of = [home](std::string_view user) -> std::optional<std::string> {
      return user.empty() && !home.empty() ? std::optional<std::string>{home.string()} : std::nullopt;
    };
  }

  // Magic's system startup file loads FreeSans, FreeSerif and FreeMono
  // from the system path; done here so `flabel` records round-trip.
  void load_default_fonts() {
    std::vector<Path> dirs;
    for (const std::string& entry : state_.search_paths.system) {
      if (const auto dir = cell::expand_entry(entry, state_.path_variables)) {
        dirs.push_back(dir->empty() ? env_.working_directory : Path{*dir});
      }
    }
    (void)cell::load_default_fonts(fs_, dirs, state_.fonts, sink_);
  }

  [[nodiscard]] std::optional<pdk::Selection> pinned_selection() const {
    if (!result_.project || !result_.project->pdk) {
      return std::nullopt;
    }
    auto selection = manager_.catalog().resolve(result_.project->pdk->variant);
    if (!selection) {
      return std::nullopt;
    }
    return selection.value();
  }

  // The pinned version's PDK startup file (<variant>.magicrc), when installed.
  StartupEnvironment with_pdk_startup_file() const {
    StartupEnvironment env = env_;
    const auto selection = pinned_selection();
    if (env.pdk_startup_file || !selection) {
      return env;
    }
    const auto& pin = *result_.project->pdk;
    if (auto dir = manager_.variant_dir(*selection->family, selection->variant->name, pin.version)) {
      env.pdk_startup_file = *dir / "libs.tech" / "magic" / (selection->variant->name + ".magicrc");
    }
    return env;
  }

  // "Magic startup-file compatibility" before the Tcl binding: the plain
  // settings land in the file's layer; script commands are not run (M4).
  void apply_startup_file(const StartupFile& file) {
    if (file.deprecated_name) {
      report(codes::kStartupNote, deprecation_note(file));
    }
    auto text = fs_.read(file.path);
    if (!text) {
      report(codes::kStartupFile, "Startup file \"" + file.path.string() + "\" not found or unreadable!");
      return;
    }
    const auto values = startup_file_settings(text.value());
    for (const auto& [name, value] : values) {
      state_.settings.set(layer_of_role(file.role), name, value);
    }
    apply_effective(state_.settings, typed_);
    report(codes::kStartupFile, "Startup file \"" + file.path.string() + "\" (" + file.role + "): " +
                                    std::to_string(values.size()) +
                                    " plain setting(s) applied; its script commands run once the Tcl binding exists (M4).");
  }

  // Path of the technology file for `name`, or `name` itself for TECH's own search.
  std::string resolve_technology(const std::string& name) {
    if (name == "minimum" || name.find('/') != std::string::npos) {
      return name;
    }
    std::optional<pdk::PinnedVersion> pin;
    if (result_.project && result_.project->pdk) {
      const auto& p = *result_.project->pdk;
      pin = pdk::PinnedVersion{p.family, p.variant, p.version};
    }
    if (!manager_.root().empty()) {
      auto store = pdk::make_managed_pdk_store(manager_, host_, pin);
      if (auto path = store->find_technology(name)) {
        return path.value().string();
      }
    }
    for (const Path& dir : {env_.working_directory, env_.install_root / "magic" / "sys",
                            env_.install_root / "magic" / "sys" / "current"}) {
      if (!dir.empty() && fs_.exists(dir / (name + ".tech"))) {
        return (dir / (name + ".tech")).string();
      }
    }
    return name;
  }

  // Runs `tech load` (TECH's load_session_technology behind it) through the
  // dispatcher, so the load is observable like any command (the startup
  // tests record it) and front ends see its diagnostics.
  bool load_technology(const std::string& name) {
    const std::string target = resolve_technology(name);
    const auto run = dispatcher_.execute(cmd::Invocation{"tech", {"load", target}, cmd::Origin::cli, std::nullopt});
    if (!run.ok()) {
      report(codes::kTechFailed, "Failed to load technology \"" + name + "\"");
      return false;
    }
    if (target != name) {
      result_.technology_file = Path{target};
    }
    loaded_tech_path_ = target;
    report(codes::kTechUsing, "Using technology \"" + state_.technology->name() + "\".");
    return true;
  }

  // Step 2 of "Startup file order": the -T technology; a failure cancels the override.
  void load_override_technology() {
    if (options_.technology) {
      tech_loaded_ = load_technology(*options_.technology);
    }
  }

  // "Default technology selection" (after the startup files).
  void select_technology() {
    if (tech_loaded_) {
      return;
    }
    std::optional<std::string> wanted = options_.technology;
    if (!wanted && load_pinned_technology()) {
      return;
    }
    if (!wanted && !pin_missing_ && !options_.files.empty()) {
      wanted = technology_of_file(options_.files.front());
    }
    if (wanted) {
      tech_loaded_ = load_technology(*wanted);
    }
    if (!tech_loaded_ && state_.technology->name() != "minimum") {
      state_.technology = tech::builtin_technology();
    }
  }

  // "Automatic technology discovery" step 6 (open_pdks layout): Magic adds the
  // PDK's libs.ref libraries to the library path when no startup script set
  // the search path. Startup scripts (the PDK .magicrc, whose `addpath` lines
  // do this) cannot run before the Tcl binding (M4), so the rule applies
  // whenever the technology was loaded from an open_pdks libs.tech directory.
  void discover_pdk_libraries() {
    const std::string& tech_file = loaded_tech_path_;
    const std::size_t at = tech_file.find("libs.tech");
    if (at == std::string::npos) {
      return;
    }
    const Path root{tech_file.substr(0, at) + "libs.ref"};
    const auto host = cell::make_memory_file_host(fs_, env_.working_directory);
    const auto found = cell::discover_library_paths(fs_, *host, root);
    if (found.empty()) {
      return;
    }
    auto& library = state_.search_paths.library;
    library.insert(library.end(), found.begin(), found.end());
    std::string joined;
    for (const auto& entry : library) {
      joined += (joined.empty() ? "" : " ") + entry;
    }
    report(codes::kLibraryPath, "Cell path is now \"" + joined + "\"");
  }

  // "Per-project version pinning": the pinned version, never the enabled one.
  bool load_pinned_technology() {
    const auto selection = pinned_selection();
    if (!selection) {
      return false;
    }
    const auto& pin = *result_.project->pdk;
    auto path = manager_.tech_file(*selection, pin.version);
    if (!path) {
      pin_missing_ = true;
      report(codes::kPinMissing, "The project pins " + pin.variant + " " + pin.version +
                                     ", which is not installed under " + manager_.root().string() +
                                     "; install exactly that version with `pdk install " + pin.variant + " " +
                                     pin.version + "`. Using the minimum technology.");
      return false;
    }
    tech_loaded_ = load_technology(path.value().string());
    return tech_loaded_;
  }

  // Magic's DBGetTech on the first file argument (`<name>.mag`, then `<name>`).
  std::optional<std::string> technology_of_file(const FileArgument& file) {
    const Path base = Path{file.name}.is_absolute() ? Path{file.name} : env_.working_directory / file.name;
    for (const Path& candidate : {Path{base.string() + ".mag"}, base}) {
      if (!fs_.exists(candidate)) {
        continue;
      }
      if (auto text = fs_.read(candidate)) {
        return mag_technology(text.value());
      }
    }
    return std::nullopt;
  }

  void recover() {
    if (dispatcher_.registry().find("crash") == nullptr) {
      report(codes::kRecoverUnavailable,
             "Crash recovery (-r) is not available in this release; command-line files were not loaded.");
      return;
    }
    (void)dispatcher_.execute(cmd::Invocation{"crash", {"recover"}, cmd::Origin::cli, std::nullopt});
  }

  // Q-UI-25: command-line order; the window ends on the first layout cell.
  void load_files() {
    std::optional<std::string> first_cell;
    std::optional<std::string> last_cell;
    for (const auto& file : options_.files) {
      report(codes::kFileLoading, "Loading \"" + file.name + "\" from command line.");
      if (load_file(file) && file.kind == FileKind::cell) {
        first_cell = first_cell.value_or(file.name);
        last_cell = file.name;
      }
    }
    if (first_cell && last_cell != first_cell) {
      (void)dispatcher_.execute(cmd::Invocation{"load", {*first_cell}, cmd::Origin::cli, std::nullopt});
    }
  }

  bool load_file(const FileArgument& file) {
    if (file.kind == FileKind::script) {
      return run_script_file(file);
    }
    const auto invocation = reader_for(file);
    if (dispatcher_.registry().find(invocation->name) == nullptr) {
      report(codes::kFileUnsupported, "Cannot read " + std::string{file_kind_name(file.kind)} + " file \"" +
                                          file.name + "\" in this release; skipped.");
      return false;
    }
    const bool ok = dispatcher_.execute(*invocation).ok();
    if (ok) {
      result_.loaded_files.push_back(file.name);
    }
    return ok;
  }

  bool run_script_file(const FileArgument& file) {
    auto text = fs_.read(file.name);
    if (!text) {
      report(codes::kFileUnsupported, "Cannot read script \"" + file.name + "\": " + text.error().text);
      return false;
    }
    const auto run = cmd::run_script(dispatcher_, text.value(), cmd::Origin::script, file.name);
    result_.loaded_files.push_back(file.name);
    return run.ok();
  }

  // The startup layout window (a null-display frame until a front end gives it
  // a screen area), opened before the file arguments so `load` shows its cell
  // there; and the technology's display styles, when its styletype files can
  // be found (a headless session works without them, Q-TECH-24).
  void open_startup_window() {
    if (options_.no_window) {
      return;
    }
    load_display_styles();
    if (state_.windows.current() == nullptr) {
      (void)state_.windows.open_frame(state_.edit_cell);
    }
  }

  void load_display_styles() {
    display::StyleLoadOptions styles;
    styles.magic_compat = typed_.compat.magic_compat;
    if (!env_.install_root.empty()) {
      styles.library_dirs = {env_.install_root / "magic" / "sys", env_.install_root / "magic" / "sys" / "current"};
    }
    diag::NullSink quiet;  // missing display files are normal in a headless run
    auto loaded = display::load_style_set(fs_, *state_.technology, styles, quiet);
    if (loaded.ok()) {
      state_.styles = std::move(loaded).value();
    }
  }

  // Step 11: the box is the edit cell's bounding box.
  void set_box() {
    if (state_.edit_cell != nullptr) {
      state_.box = state_.edit_cell->bbox();
      state_.box_root = state_.edit_cell;
    }
  }

  cmd::Dispatcher& dispatcher_;
  editor::EditorState& state_;
  settings::ProjectSettings& typed_;
  platform::FileSystem& fs_;
  const StartupOptions& options_;
  StartupEnvironment env_;
  diag::DiagnosticSink& sink_;
  NullInstallHost null_host_;
  std::string loaded_tech_path_;  // as given to `tech load`; empty when none loaded
  pdk::InstallHost& host_;
  std::unique_ptr<pdk::Fetcher> fetcher_;
  pdk::PdkManager manager_;
  StartupResult result_;
  bool tech_loaded_ = false;
  bool pin_missing_ = false;
};

}  // namespace

diag::Outcome<StartupResult> run_startup(cmd::Dispatcher& dispatcher, settings::ProjectSettings& project_settings,
                                         platform::FileSystem& fs, const StartupOptions& options,
                                         const StartupEnvironment& env, diag::DiagnosticSink& sink) {
  editor::EditorState* state = dispatcher.editor();
  if (state == nullptr) {
    return diag::make(diag::codes::kCmdSession, "startup needs an editor session");
  }
  Startup startup{dispatcher, *state, project_settings, fs, options, env, sink};
  return startup.run();
}

}  // namespace hefesto::app
