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
#include "hefesto/cmd/units.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/pcell/framework/hooks.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/binding/variables.hpp"
#include "hefesto/script/pdk/startup.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/undo/undo_log.hpp"

namespace hefesto::app {

namespace {

constexpr std::string_view kUnnamedCell = "(UNNAMED)";

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
    open_interpreter(files);
    auto next = files.begin();
    for (; next != files.end() && next->role == "pre"; ++next) {
      run_startup_file(*next);
    }
    load_override_technology();
    for (; next != files.end() && !quit_requested(); ++next) {
      run_startup_file(*next);
    }
    state_.technology_override = false;  // Magic's main.c:1132: startup is done
    if (quit_requested()) {
      return std::move(result_);  // `quit` in a startup file ends startup (scripting-pdk-integration)
    }
    select_technology();
    discover_pdk_libraries();
    result_.technology = state_.technology->name();
    const drc::BackgroundMode drc_state = defer_drc();
    open_startup_window();
    if (options_.recover && !options_.no_window) {
      recover();
    } else {
      load_files();
    }
    set_box();
    resume_drc(drc_state);
    // Magic's UndoFlush after the command-line files (utils/main.c:1236):
    // the first `undo` of the session must not revert the startup loads.
    state_.undo.clear();
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
    apply_editor_settings();
    record_recent_project();
  }

  // Settings read once into editor state: the M1 `units` key (now the
  // `units` command's cmd::UnitsState; m2 design "Migration Plan") and the
  // undo history limit (layout-undo "History limit").
  void apply_editor_settings() {
    if (const auto units = state_.settings.effective("units")) {
      for (const cmd::InputUnit unit : {cmd::InputUnit::default_units, cmd::InputUnit::internal,
                                        cmd::InputUnit::lambda, cmd::InputUnit::user, cmd::InputUnit::microns}) {
        if (cmd::units_name(unit) == units->value) {
          state_.units = cmd::UnitsState{unit, false};
        }
      }
    }
    if (const auto limit = state_.settings.effective(undo::kHistoryLimitSetting)) {
      if (auto status = undo::set_history_limit(state_.undo, limit->value); !status) {
        sink_.emit(status.error());
      }
    }
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

  // ---- startup files as scripts (scripting-pdk-integration "Startup files
  // are scripts"; design D7). Owner: PDKINT.

  // The project's interpreter, created when the project opens (one per
  // EditorState, hefesto-scripting "One interpreter per open project"),
  // with the engine commands bound, the cross-PDK helper hook and the PDK
  // store as the trust backend. A front end that opened it already keeps it.
  void open_interpreter(const std::vector<StartupFile>& files) {
    if (state_.script.interp == nullptr) {
      (void)script::open_project_interpreter(state_, sandbox_policy(files));
      const auto bound = script::bind_registry(*state_.script.interp, dispatcher_.registry(), dispatcher_);
      for (const auto& d : bound.diagnostics) {
        sink_.emit(d);  // the binding installs the Q-SCRIPT-41 helper-origin hook itself
      }
      // toolkit.tcl's session hooks (Magic ref: tcltk/toolkit.tcl:111-117;
      // pcell-toolkit "Selection-driven dialog refresh and key binding"): the
      // `select` tag that refreshes an open gencell dialog and Tk's `raise`
      // for `.params`, before any startup or PDK script.
      for (const auto& d : pcell::install_toolkit(*state_.script.interp, state_).diagnostics) {
        sink_.emit(d);
      }
      if (env_.configure_interpreter) {
        env_.configure_interpreter(*state_.script.interp);
      }
    }
    if (state_.script.pdk_trust.backend() == nullptr && env_.host != nullptr && !manager_.root().empty()) {
      state_.script.pdk_trust.set_backend(script::make_pdk_store_trust(fs_, *env_.host, manager_.root()));
    }
  }

  // Granted roots and the synthesized environment (D-31) of the project's
  // interpreter; enforced by TCL's sandbox (design D5). Each startup file is
  // granted itself (an explicitly chosen document), not its directory.
  // Security findings S-11 and S-03: the PDK store and the install root are
  // read-only; the working directory is granted only when the front end asks
  // for it and it is not "/", the home folder or a folder above it.
  script::SandboxPolicy sandbox_policy(const std::vector<StartupFile>& files) const {
    script::SandboxPolicy policy;
    for (const StartupFile& file : files) {
      policy.granted_roots.push_back(file.path);
    }
    for (const FileArgument& file : options_.files) {
      if (file.kind == FileKind::script) {
        policy.granted_roots.push_back(absolute(file.name));  // an explicitly named document (design D5)
      } else if (const Path folder = absolute(file.name).parent_path(); grantable_folder(folder)) {
        policy.granted_roots.push_back(folder);  // the folder of an opened layout: its project
      }
    }
    if (env_.grant_working_directory && grantable_folder(env_.working_directory)) {
      policy.granted_roots.push_back(env_.working_directory);
    }
    for (const Path& root : {manager_.root(), env_.install_root}) {
      if (!root.empty()) {
        policy.read_only_roots.push_back(root);
      }
    }
    if (env_.project_folder) {
      policy.granted_roots.push_back(*env_.project_folder);
    }
    if (!env_.temporary_folder.empty()) {
      policy.granted_roots.push_back(env_.temporary_folder);
    }
    policy.limits = env_.script_limits;
    if (!env_.install_root.empty()) {
      policy.environment["CAD_ROOT"] = env_.install_root.string();
    }
    if (const auto pdk = pinned_variant_dir()) {
      policy.environment["PDK_ROOT"] = script::pdk_root_for(*pdk).string();
      policy.environment["PDK"] = pdk->filename().string();
    } else if (!manager_.root().empty()) {
      policy.environment["PDK_ROOT"] = manager_.root().string();
    }
    return policy;
  }

  // A folder that may become a granted root implicitly: not "/", not the home
  // folder and not a folder above it (S-11: a desktop app launched from the
  // Finder runs in "/", a shell often in $HOME).
  [[nodiscard]] bool grantable_folder(const Path& folder) const {
    if (folder.empty() || !folder.is_absolute()) {
      return false;
    }
    const Path normal = folder.lexically_normal();
    if (normal == normal.root_path()) {
      return false;
    }
    if (env_.home.empty()) {
      return true;
    }
    const Path relative = env_.home.lexically_normal().lexically_relative(normal);
    return !relative.empty() && *relative.begin() == "..";  // home is not at or below `folder`
  }

  [[nodiscard]] std::optional<Path> pinned_variant_dir() const {
    const auto selection = pinned_selection();
    if (!selection) {
      return std::nullopt;
    }
    return manager_.variant_dir(*selection->family, selection->variant->name, result_.project->pdk->version);
  }

  [[nodiscard]] bool quit_requested() { return dispatcher_.session().quit_requested; }

  script::StartupFile script_file(const StartupFile& file) const {
    script::StartupFile out;
    out.path = file.path;
    out.legacy_name = file.deprecated_name || file.path.filename() == "magic_setup";
    if (file.role == "pre") {
      out.kind = script::StartupKind::pre;
    } else if (file.role == "system") {
      out.kind = script::StartupKind::system;
    } else if (file.role == "pdk") {
      out.kind = script::StartupKind::pdk;
      out.pdk = script::pdk_identity_of(file.path);
    } else {
      out.kind = file.role == "user" ? script::StartupKind::user : script::StartupKind::local;
      out.name = out.legacy_name ? file.path.filename().string() : options_.rc_name();
      // A PDK's own rc file named with -rcfile is explicit startup of that
      // PDK's scripts: it asks for consent too (design D7, hefesto-security).
      out.pdk = script::pdk_identity_of(file.path);
    }
    return out;
  }

  // Evaluates one startup file in the project's interpreter (after consent
  // for a PDK's rc file), with Magic's error texts.
  void run_startup_file(const StartupFile& file) {
    if (file.deprecated_name) {
      report(codes::kStartupNote, deprecation_note(file));
    }
    script::StartupPlan plan;
    plan.files.push_back(script_file(file));
    const auto run = script::run_startup_files(state_, plan);
    for (const auto& d : run.diagnostics) {
      if (quit_requested() && d.code == "SCRIPT-PDK-001") {
        continue;  // the unwinding of `quit` (the binding stops the evaluation), not an error
      }
      sink_.emit(d);
      if (d.code == "SCRIPT-TRUST-001" && file.role == "pdk") {
        pdk_declined_ = true;
      }
    }
    if (file.role == "pdk" && !pdk_declined_) {
      pdk_rc_ran_ = true;
    }
    // VDD, GND, SUB and globals(...) set by the file seed its setting layer
    // (hefesto-settings "Layered settings resolution"; BINDING's traces).
    script::adopt_engine_variables(state_.settings, layer_of_role(file.role));
    apply_effective(state_.settings, typed_);
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
      state_.technology_override = tech_loaded_;  // a failed -T load revokes it
    }
  }

  // "Default technology selection" (after the startup files).
  void select_technology() {
    // A startup file that loaded a technology (Magic's TechFileName set)
    // decides it (Magic ref: utils/main.c:1141-1151).
    if (tech_loaded_ || !state_.technology->file_name().empty()) {
      return;
    }
    std::optional<std::string> wanted = options_.technology;
    if (!wanted && load_pinned_technology()) {
      return;
    }
    if (!wanted && !pin_missing_ && !options_.files.empty()) {
      wanted = technology_of_file(options_.files.front());
      if (wanted && state_.script.pdk_trust.declined_variant(*wanted)) {
        wanted.reset();  // declined PDK scripts: minimum and read-only (hefesto-security)
      }
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
    if (pdk_rc_ran_) {
      return;  // the PDK's rc file set the search path (DBio.c: only without an rc file)
    }
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
    if (!selection || pdk_declined_) {
      return false;  // declined PDK script: the minimum technology (hefesto-security)
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

  // A `.tcl` file argument: Tcl's `source` in the project's interpreter
  // (opened by run() before any file argument), with Magic's error text
  // (Magic ref: utils/main.c:1212-1221).
  bool run_script_file(const FileArgument& file) {
    const auto run = state_.script.interp->source(absolute(file.name));
    result_.loaded_files.push_back(file.name);
    if (!run.ok() && !quit_requested()) {
      const std::string message = run.diagnostics.empty() ? std::string{} : run.diagnostics.front().text;
      report("SCRIPT-PDK-001", "Error parsing \"" + file.name + "\": " + message);
    }
    return run.ok() || quit_requested();
  }

  // Steps 10 and 15 of "Script-host startup sequence": the DRC state the
  // startup files left is saved and background DRC is off while the window
  // opens and the command-line files load, then turned back on if it was on
  // (Magic ref: tcltk/magic.tcl.in:355-356,467-470). Only in wrapper mode
  // (a display other than NULL named with -d): with NULL graphics, or no
  // -d, Magic's C startup loads the files before that step, with DRC as the
  // startup files left it (utils/main.c:1189-1225). Not dispatched, so the
  // startup's command log holds only what the user asked for. Owner: PDKINT.
  [[nodiscard]] bool wrapper_mode() const {
    if (!options_.display_type) {
      return false;
    }
    const auto display = resolve_display(options_.display_type, true);
    return display.ok() && !display.value().headless;
  }

  drc::BackgroundMode defer_drc() {
    const drc::BackgroundMode saved = state_.drc.background;
    if (!wrapper_mode()) {
      return saved;
    }
    state_.drc.background = drc::BackgroundMode::off;
    return saved;
  }

  void resume_drc(drc::BackgroundMode saved) {
    if (saved == drc::BackgroundMode::off || state_.drc.background != drc::BackgroundMode::off) {
      return;  // was off, or a file argument turned it on itself
    }
    state_.drc.background = drc::BackgroundMode::on;  // `drc on`
    if (state_.drc.worker != nullptr) {
      state_.drc.worker->wake();
    }
  }

  // `name` made absolute against the session's working directory.
  [[nodiscard]] Path absolute(const std::string& name) const {
    const Path path{name};
    return path.is_absolute() || env_.working_directory.empty() ? path : env_.working_directory / path;
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
      adopt_unnamed_edit_cell();
      (void)state_.windows.open_frame(state_.edit_cell);
    }
  }

  // Magic's startup window shows "(UNNAMED)" and makes it the edit cell
  // (DBWloadWindow with no name), so commands before any `load` paint into
  // it and `save NAME` writes it (CMDEDIT-HIER-09), and undoing the first
  // `load` makes it the edit cell again (MODEL-G-050). Not recorded.
  void adopt_unnamed_edit_cell() {
    if (state_.edit_cell != nullptr) {
      return;
    }
    db::CellDef* unnamed = state_.cells.find(kUnnamedCell);
    if (unnamed == nullptr) {
      auto made = state_.cells.create(kUnnamedCell, state_.technology->plane_count());
      if (!made) {
        return;
      }
      unnamed = made.value();
      unnamed->flags().available = true;
    }
    state_.edit_cell = unnamed;
    state_.edit_root = unnamed;
    state_.edit_transform = db::kIdentity;
  }

  void load_display_styles() {
    display::StyleLoadOptions styles;
    styles.magic_compat = typed_.compat.magic_compat;
    if (!env_.install_root.empty()) {
      styles.library_dirs = {env_.install_root / "magic" / "sys", env_.install_root / "magic" / "sys" / "current"};
    }
    state_.style_dirs = styles.library_dirs;
    diag::NullSink quiet;  // missing display files are normal in a headless run
    auto loaded = display::load_style_set(fs_, *state_.technology, styles, quiet);
    if (loaded.ok()) {
      state_.styles = std::move(loaded).value();
    }
  }

  // Step 11: the box is the edit cell's bounding box.
  void set_box() {
    // Magic's startup window on "(UNNAMED)" has no box (UI-G-003).
    if (state_.edit_cell != nullptr && state_.edit_cell->name() != kUnnamedCell) {
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
  bool pdk_declined_ = false;  // the user declined the pinned PDK's rc file
  bool pdk_rc_ran_ = false;    // the pinned PDK's rc file ran
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
