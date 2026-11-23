// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The document options of `schematic` that CMD implements: open, new, save,
// close, reload, reload_symbols, path, resolve and pdk (capability
// schematic-command "Subcommand table"), and the evaluation of the
// `xschemrc` files when a document is opened (capability
// schematic-symbols-and-libraries "xschemrc as data").
//
// Owner: CMD.

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "internal.hpp"

namespace hefesto::schem::cmdimpl {

namespace {

constexpr std::array<std::string_view, 3> kWindows{"new", "tab", "current"};
constexpr std::array<std::string_view, 2> kKinds{"schematic", "symbol"};
constexpr std::array<std::string_view, 3> kPathActions{"list", "set", "prepend"};

std::string env_value(const cmd::CommandContext& ctx, std::string_view name) {
  return sandbox_env(ctx, name);
}

// The Hefesto copy of the xschem system library (Requirement "System
// library"), under the installation root of the project interpreter.
platform::Path sharedir(const cmd::CommandContext& ctx) {
  const std::string root = env_value(ctx, "CAD_ROOT");
  return root.empty() ? platform::Path{} : platform::Path{root} / "assets" / "schematic";
}

XschemrcInput xschemrc_input(const cmd::CommandContext& ctx, const std::string& pdk, const platform::Path& folder) {
  const platform::Path user_conf = folder / ".xschem";  // `~/.xschem` with env(HOME) the document folder
  const std::string pdk_root = env_value(ctx, "PDK_ROOT");
  if (!pdk.empty() && !pdk_root.empty()) {
    return pdk_xschemrc_input(pdk_root, pdk, sharedir(ctx), user_conf, folder);
  }
  XschemrcInput input;
  input.document_folder = folder;
  input.env = {{"HOME", folder.generic_string()}, {"PWD", folder.generic_string()}};
  input.variables = {{"XSCHEM_SHAREDIR", sharedir(ctx).generic_string()},
                     {"USER_CONF_DIR", user_conf.generic_string()}};
  return input;
}

// `input` with a missing PDK file left out; nullopt when the folder has no
// `xschemrc` either.
std::optional<XschemrcInput> existing_files(platform::FileSystem& fs, XschemrcInput input,
                                            const platform::Path& folder) {
  if (!input.rc_file.empty() && !fs.exists(input.rc_file)) input.rc_file.clear();
  if (input.rc_file.empty() && !fs.exists(folder / "xschemrc")) return std::nullopt;
  return input;
}

void publish_xschemrc(cmd::CommandContext& ctx, Session& session, XschemrcResult result) {
  for (const diag::Diagnostic& d : result.diagnostics) ctx.emit(d);
  session.search_path = result.search_path;
  if (script::Interpreter* interp = ctx.editor()->script.interp.get()) {
    for (const auto& [name, value] : result.variables) (void)interp->set_var(name, cmd::Value{value});
  }
  session.xschemrc = std::move(result);
}

platform::Path document_folder(Session& session) {
  const OpenDocument* active = active_document(session);
  if (active != nullptr && !active->document.path.empty()) return active->document.path.parent_path();
  return absolute_path(".");
}

ReadOptions read_options(cmd::CommandContext& ctx) {
  ReadOptions options;
  options.xschem_compat = ctx.project().compat.xschem_compat;
  options.warnings = &ctx.sink();
  return options;
}

cmd::Value path_list(const SearchPath& path) {
  cmd::Value::List list;
  for (const platform::Path& dir : path.directories()) list.emplace_back(dir.generic_string());
  return list;
}

SearchPath parse_path(cmd::CommandContext& ctx, Session& session, const std::string& value) {
  const std::map<std::string, std::string> env{{"HOME", document_folder(session).generic_string()}};
  return SearchPath::parse(value, env, ctx.services().files);
}

OpenDocument* find_open(Session& session, const platform::Path& path) {
  const auto it = std::find_if(session.documents.begin(), session.documents.end(),
                               [&path](const OpenDocument& open) { return open.document.path == path; });
  return it == session.documents.end() ? nullptr : &*it;
}

// Makes the open document `open` the active one (the last of the list).
DocumentId activate(Session& session, OpenDocument& open) {
  const DocumentId id = open.id;
  const auto it = session.documents.begin() + (&open - session.documents.data());
  std::rotate(it, it + 1, session.documents.end());
  return id;
}

DocumentId add_document(Session& session, Document document) {
  const DocumentId id = session.next_id++;
  session.documents.push_back(OpenDocument{id, std::move(document)});
  return id;
}

}  // namespace

std::string sandbox_env(const cmd::CommandContext& ctx, std::string_view name) {
  if (ctx.editor() == nullptr) return {};
  const auto& env = ctx.editor()->script.sandbox.environment;
  const auto it = env.find(name);
  return it == env.end() ? std::string{} : it->second;
}

std::optional<XschemrcInput> xschemrc_files(cmd::CommandContext& ctx, const Session& session,
                                            const platform::Path& document_folder) {
  return existing_files(ctx.services().files, xschemrc_input(ctx, active_pdk(ctx, session), document_folder),
                        document_folder);
}

std::string active_pdk(const cmd::CommandContext& ctx, const Session& session) {
  if (session.xschemrc) {
    const auto it = session.xschemrc->variables.find("PDK");
    if (it != session.xschemrc->variables.end() && !it->second.empty()) return it->second;
  }
  return env_value(ctx, "PDK");
}

diag::Status evaluate_xschemrc(cmd::CommandContext& ctx, Session& session, const platform::Path& document_folder,
                               const std::optional<std::string>& pdk) {
  platform::FileSystem& fs = ctx.services().files;
  XschemrcInput input = xschemrc_input(ctx, pdk.value_or(active_pdk(ctx, session)), document_folder);
  const bool pdk_rc = !input.rc_file.empty() && fs.exists(input.rc_file);
  if (pdk && !pdk_rc) {
    const std::string where = input.rc_file.empty() ? std::string{"no PDK store"} : input.rc_file.generic_string();
    return diag::make(kNoPdkData, "schematic: PDK " + *pdk + " has no xschem data (" + where + ")");
  }
  auto files = existing_files(fs, std::move(input), document_folder);
  if (!files) return diag::Done{};
  publish_xschemrc(ctx, session, SearchPath::from_xschemrc(fs, *files));
  return diag::Done{};
}

cmd::CommandResult option_open(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-window", true}};
  const auto parsed = parse_args(args, kFlags, "open");
  if (!parsed) return fail(parsed.error());
  if (parsed.value().positional.size() != 1) return usage("open");
  std::size_t window = 0;
  if (parsed.value().has(0)) {
    const auto found = match_keyword(*parsed.value().flags[0], kWindows);
    if (!found) return fail(found.error());
    window = found.value();
  }
  const platform::Path path = absolute_path(parsed.value().positional.front());
  if (OpenDocument* open = find_open(session, path)) return cmd::CommandResult::success(std::int64_t{activate(session, *open)});
  const OpenDocument* active = active_document(session);
  const bool replace = window == 2 && active != nullptr;
  if (replace && active->document.modified) return fail(kUnsaved, "schematic: unsaved changes");
  auto loaded = load_document(ctx.services().files, path, read_options(ctx));
  if (!loaded) return fail(loaded.error());
  (void)evaluate_xschemrc(ctx, session, path.parent_path());
  if (replace) session.documents.pop_back();
  return cmd::CommandResult::success(std::int64_t{add_document(session, std::move(loaded).value())});
}

cmd::CommandResult option_new(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (args.empty() || args.size() > 2) return usage("new");
  const auto kind = match_keyword(args[0], kKinds);
  if (!kind) return fail(kind.error());
  Document document;
  document.kind = kind.value() == 0 ? FileKind::schematic : FileKind::symbol;
  document.file_version = "1.3";  // the version Hefesto writes; nothing was read
  if (args.size() == 2) {
    document.path = absolute_path(args[1]);
    (void)evaluate_xschemrc(ctx, session, document.path.parent_path());
  }
  return cmd::CommandResult::success(std::int64_t{add_document(session, std::move(document))});
}

cmd::CommandResult option_save(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-force", false}};
  const auto parsed = parse_args(args, kFlags, "save");
  if (!parsed || parsed.value().positional.size() > 1) return parsed ? usage("save") : fail(parsed.error());
  Document& document = active_document(session)->document;
  const platform::Path destination =
      parsed.value().positional.empty() ? document.path : absolute_path(parsed.value().positional.front());
  if (destination.empty()) return usage("save");
  SaveOptions options;
  options.overwrite_changed_file = parsed.value().has(0);
  const auto saved = save_document(ctx.services().files, document, destination, options);
  if (!saved) return fail(saved.error());
  session.symbols.forget(destination);
  return cmd::CommandResult::success(destination.generic_string());
}

cmd::CommandResult option_close(cmd::CommandContext& /*ctx*/, Session& session, std::span<const std::string> args) {
  static constexpr std::array kFlags{FlagSpec{"-discard", false}};
  const auto parsed = parse_args(args, kFlags, "close");
  if (!parsed || !parsed.value().positional.empty()) return parsed ? usage("close") : fail(parsed.error());
  if (active_document(session)->document.modified && !parsed.value().has(0)) {
    return fail(kUnsaved, "schematic: unsaved changes");
  }
  session.documents.pop_back();
  return cmd::CommandResult::success();
}

cmd::CommandResult option_reload(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (!args.empty()) return usage("reload");
  OpenDocument& active = *active_document(session);
  if (active.document.path.empty()) return fail(kNoObject, "schematic: the document has no file to reload");
  auto loaded = load_document(ctx.services().files, active.document.path, read_options(ctx));
  if (!loaded) return fail(loaded.error());
  active.document = std::move(loaded).value();
  return cmd::CommandResult::success();
}

cmd::CommandResult option_reload_symbols(cmd::CommandContext& /*ctx*/, Session& session,
                                         std::span<const std::string> args) {
  if (!args.empty()) return usage("reload_symbols");
  session.symbols.clear();
  return cmd::CommandResult::success();
}

cmd::CommandResult option_path(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (args.empty()) return cmd::CommandResult::success(path_list(session.search_path));
  const auto action = match_keyword(args[0], kPathActions);
  if (!action) return fail(action.error());
  if (args.size() != (action.value() == 0 ? 1U : 2U)) return usage("path");
  if (action.value() == 1) session.search_path = parse_path(ctx, session, args[1]);
  if (action.value() == 2) session.search_path = session.search_path.prepended(parse_path(ctx, session, args[1]));
  return cmd::CommandResult::success(path_list(session.search_path));
}

cmd::CommandResult option_resolve(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (args.size() != 1) return usage("resolve");
  platform::FileSystem& fs = ctx.services().files;
  const ResolvedReference found = resolve_reference(args[0], document_folder(session), session.search_path, fs);
  const bool usable = !found.file.empty() && fs.exists(found.file);
  return cmd::CommandResult::success(usable ? found.file.generic_string() : std::string{});
}

cmd::CommandResult option_pdk(cmd::CommandContext& ctx, Session& session, std::span<const std::string> args) {
  if (args.size() > 1) return usage("pdk");
  if (args.size() == 1) {
    const auto status = evaluate_xschemrc(ctx, session, document_folder(session), args[0]);
    if (!status) return fail(status.error());
  }
  return cmd::CommandResult::success(active_pdk(ctx, session));
}

}  // namespace hefesto::schem::cmdimpl
