// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The restricted commands of the script sandbox (design D5; X-3).
//
//   removed     the policy's removed commands (exec, socket, load, unload,
//               exit) are deleted and replaced by a command that raises a
//               Tcl error naming the operation.
//   guarded     open, interp, package, cd, glob, source and the `file`
//               subcommands that touch the filesystem keep their Tcl
//               implementation behind a guard (Tcl_SetCommandInfo, so
//               compiled and ensemble calls reach it too) that
//                 - refuses `open |…`, `interp create` without `-safe`,
//                   `interp invokehidden|expose|marktrusted`, `package
//                   require` of a package Tcl's embedded library does not
//                   provide, and `file tempfile` outside a granted root;
//                 - turns a path denial of the filesystem layer during the
//                   call into the Tcl error "<operation>: "<path>" is outside
//                   the sandbox's granted roots", even when Tcl itself would
//                   have answered (`file exists` → 0).
//   children    a safe child interpreter's object command is guarded the
//               same way, so its hidden commands stay unreachable.
//
// Every refusal is reported by SandboxSession::refuse (diagnostic with the
// script file and line, DeniedOperation event).

#include <tcl.h>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "hefesto/script/tcl/embedded_library.hpp"
#include "script/tcl/sandbox_runtime.hpp"

namespace hefesto::script::tcl_detail {

namespace {

constexpr std::string_view kProcessDenied = "SCRIPT-SANDBOX-001";
constexpr std::string_view kSocketDenied = "SCRIPT-SANDBOX-002";
constexpr std::string_view kExtensionDenied = "SCRIPT-SANDBOX-003";
constexpr std::string_view kPathDenied = "SCRIPT-SANDBOX-004";
constexpr std::string_view kInterpDenied = "SCRIPT-SANDBOX-005";
constexpr std::string_view kPackageDenied = "SCRIPT-SANDBOX-006";
constexpr std::string_view kRemovedDenied = "SCRIPT-SANDBOX-007";

// True when `word` abbreviates `name` (Tcl's unique-prefix matching of
// subcommands and options; an ambiguous prefix is treated as a match, which
// only turns Tcl's "ambiguous" error into a refusal).
bool abbreviates(std::string_view word, std::string_view name, std::size_t minimum = 1) {
  return word.size() >= minimum && word.size() <= name.size() && name.substr(0, word.size()) == word;
}

std::string words_after(int first, int objc, Tcl_Obj* const objv[]) {
  std::string text;
  for (int i = first; i < objc; ++i) {
    text += i == first ? "" : " ";
    text += as_view(objv[i]);
  }
  return text;
}

Refusal path_refusal(const std::string& operation, const PathDenial& denial) {
  return Refusal{kPathDenied, operation, denial.path,
                 operation + ": \"" + denial.path + "\" is outside the sandbox's granted roots"};
}

// ---- removed commands ---------------------------------------------------------------

struct Removed {
  SandboxSession* session;
  std::string name;
};

Refusal removed_refusal(const std::string& name, std::string target) {
  if (name == "exec") {
    return {kProcessDenied, name, std::move(target), "exec: process execution is not permitted in the script sandbox"};
  }
  if (name == "socket") {
    return {kSocketDenied, name, std::move(target), "socket: network sockets are not permitted in the script sandbox"};
  }
  if (name == "load" || name == "unload") {
    return {kExtensionDenied, name, std::move(target),
            name + ": loading native extensions is not permitted in the script sandbox"};
  }
  if (name == "exit") {
    return {kRemovedDenied, name, std::move(target), "exit: not available in the script sandbox (use quit)"};
  }
  return {kRemovedDenied, name, std::move(target), name + ": not available in the script sandbox"};
}

int removed_command(ClientData data, Tcl_Interp* /*interp*/, int objc, Tcl_Obj* const objv[]) {
  const auto& removed = *static_cast<const Removed*>(data);
  return removed.session->refuse(removed_refusal(removed.name, words_after(1, objc, objv)));
}

void delete_removed(ClientData data) {
  delete static_cast<Removed*>(data);
}

// ---- guards -------------------------------------------------------------------------

// True when the command being executed is code of Tcl's embedded library
// (its frame's file is under the embedded root): Tcl probes host paths there
// (clock.tcl's zoneinfo directories, tm.tcl's module roots), which the layer
// denies; such a probe keeps Tcl's own answer ("does not exist") and is not
// reported. The access is denied all the same.
bool in_tcl_library(Tcl_Interp* interp) {
  return is_embedded_path(current_location(interp).file);
}

struct Guard;
using PreCheck = std::optional<Refusal> (*)(Guard& guard, int objc, Tcl_Obj* const objv[]);
using PostHook = void (*)(Guard& guard, int objc, Tcl_Obj* const objv[]);

struct Guard {
  SandboxSession* session = nullptr;
  Tcl_CmdInfo original{};
  std::string operation;
  PreCheck pre = nullptr;
  PostHook post = nullptr;  // after an ok call
  /// `source`: the call evaluates a script, whose own commands report their
  /// denials; what is left unclaimed is reported without changing the result.
  bool container = false;
};

int guarded_call(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
  Guard& guard = *static_cast<Guard*>(data);
  // S-06: the command runs with its own project's roots, whichever project
  // services the thread's event queue (an `after` script of project A run by
  // project B's `update`). S-05: Tcl caches which filesystem claimed a path
  // object; a new epoch makes every path of this call checked again (a link
  // moved into a path that was harmless when first checked).
  const ActiveSession active{*guard.session};
  Tcl_FSMountsChanged(nullptr);
  if (guard.pre != nullptr) {
    if (const auto refusal = guard.pre(guard, objc, objv)) {
      return guard.session->refuse(*refusal);
    }
  }
  const std::size_t mark = guard.session->pending();
  const int code = guard.original.objProc(guard.original.objClientData, interp, objc, objv);
  if (guard.container) {
    guard.session->report_unclaimed(guard.operation, as_view(objv[objc - 1]), mark);
    return code;
  }
  const auto denials = guard.session->claim(mark);
  if (!denials.empty() && !in_tcl_library(interp)) {
    return guard.session->refuse(path_refusal(guard.operation, denials.front()));
  }
  if (code == TCL_OK && guard.post != nullptr) {
    guard.post(guard, objc, objv);
  }
  return code;
}

void delete_guard(ClientData data) {
  Guard* guard = static_cast<Guard*>(data);
  if (guard->original.deleteProc != nullptr) {
    guard->original.deleteProc(guard->original.deleteData);
  }
  delete guard;
}

// Puts a guard in front of the command `name`; false when there is none.
bool guard_command(SandboxSession& session, const std::string& name, Guard settings) {
  Tcl_Interp* interp = session.interp();
  Tcl_CmdInfo info;
  if (Tcl_GetCommandInfo(interp, name.c_str(), &info) == 0 || info.objProc == nullptr) {
    return false;
  }
  auto* guard = new Guard(std::move(settings));
  guard->session = &session;
  guard->original = info;
  Tcl_CmdInfo replaced = info;
  replaced.objProc = guarded_call;
  replaced.objClientData = guard;
  replaced.deleteProc = delete_guard;
  replaced.deleteData = guard;
  // A different objProc also drops the command's NRE procedure, so every
  // call (direct, compiled, through an ensemble) runs the guard.
  Tcl_SetCommandInfo(interp, name.c_str(), &replaced);
  return true;
}

// ---- pre-checks and hooks --------------------------------------------------------

// S-03: a refusal when `path` resolves under a read-only root (the PDK store,
// the install root) and under no granted root. A path outside every root is
// left to the filesystem layer, which denies it.
std::optional<Refusal> refuse_read_only(const Guard& guard, Tcl_Obj* path) {
  Tcl_Obj* absolute = as_view(path).empty() ? nullptr : Tcl_FSGetNormalizedPath(nullptr, path);
  if (absolute == nullptr) {
    return std::nullopt;
  }
  const auto decision = guard.session->filesystem().check(guard.operation, as_view(absolute), "/");
  if (!decision.allowed || decision.writable) {
    return std::nullopt;
  }
  const std::string target = decision.normalized.string();
  return Refusal{kPathDenied, guard.operation, target,
                 guard.operation + ": \"" + target + "\" is read-only in the script sandbox"};
}

// True when an `open` access argument (`r+`, `w`, `{WRONLY CREAT}`, ...) can
// change the file.
bool opens_for_writing(std::string_view access) {
  if (access.empty()) {
    return false;
  }
  if (access.front() >= 'a' && access.front() <= 'z') {
    return access.front() != 'r' || access.find('+') != std::string_view::npos;
  }
  for (const std::string_view flag : {"WRONLY", "RDWR", "CREAT", "TRUNC", "APPEND"}) {
    if (access.find(flag) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

std::optional<Refusal> check_open(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc >= 2 && as_view(objv[1]).substr(0, 1) == "|") {
    return Refusal{kProcessDenied, "open |", std::string{as_view(objv[1])},
                   "open: process execution is not permitted in the script sandbox"};
  }
  if (objc >= 3 && opens_for_writing(as_view(objv[2]))) {
    return refuse_read_only(guard, objv[1]);
  }
  return std::nullopt;
}

// Which words of a `file` subcommand name paths it changes (S-03).
enum class Changes : std::uint8_t { paths, last, set_first };

// The words of `file <sub>` after its options (`-force`, `--`).
int first_operand(int objc, Tcl_Obj* const objv[]) {
  int at = 1;
  while (at < objc && as_view(objv[at]).substr(0, 1) == "-") {
    if (as_view(objv[at++]) == "--") {
      break;
    }
  }
  return at;
}

std::optional<Refusal> check_changed_paths(Guard& guard, int objc, Tcl_Obj* const objv[], Changes changes) {
  if (changes == Changes::set_first) {
    // `file attributes name -opt value`, `file mtime name time`: a set.
    const bool sets = guard.operation == "file attributes" ? objc >= 4 : objc >= 3;
    return sets && objc >= 2 ? refuse_read_only(guard, objv[1]) : std::nullopt;
  }
  const int first = first_operand(objc, objv);
  if (changes == Changes::last) {
    return first < objc ? refuse_read_only(guard, objv[objc - 1]) : std::nullopt;
  }
  for (int i = first; i < objc; ++i) {
    if (auto refusal = refuse_read_only(guard, objv[i])) {
      return refusal;
    }
  }
  return std::nullopt;
}

std::optional<Refusal> check_write_paths(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  return check_changed_paths(guard, objc, objv, Changes::paths);
}

std::optional<Refusal> check_write_target(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  return check_changed_paths(guard, objc, objv, Changes::last);
}

std::optional<Refusal> check_write_setter(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  return check_changed_paths(guard, objc, objv, Changes::set_first);
}

// `source <path>`: a denied file is refused by name before Tcl tries it
// (Tcl's own message would be "couldn't read file").
std::optional<Refusal> check_source(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc < 2) {
    return std::nullopt;
  }
  Tcl_Obj* path = objv[objc - 1];
  if (as_view(path).empty()) {
    return std::nullopt;
  }
  Tcl_Obj* absolute = Tcl_FSGetNormalizedPath(nullptr, path);
  if (absolute != nullptr && (is_embedded_path(as_view(absolute)) ||
                              guard.session->filesystem().check("source", as_view(absolute), "/").allowed)) {
    return std::nullopt;
  }
  const PathDenial denial{"source", std::string{as_view(absolute != nullptr ? absolute : path)}};
  return path_refusal("source", denial);
}

// `file link ?-linktype? linkName ?target?`: the target of a new link must
// lie under a granted root (a relative target is relative to the link's
// directory). Following such a link is checked anyway; this names the
// refusal where the link is made.
std::optional<Refusal> check_link(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc < 3 || (objc == 3 && as_view(objv[1]).substr(0, 1) == "-")) {
    return std::nullopt;
  }
  Tcl_Obj* link = Tcl_FSGetNormalizedPath(nullptr, objv[objc - 2]);
  const platform::Path target{as_view(objv[objc - 1])};
  const platform::Path base = link != nullptr ? platform::Path{as_view(link)}.parent_path() : platform::Path{};
  const auto decision = guard.session->filesystem().check("file link", target.string(), base);
  if (link != nullptr && !base.empty() && decision.allowed) {
    return refuse_read_only(guard, objv[objc - 2]);
  }
  const PathDenial denial{"link", decision.normalized.string()};
  return path_refusal("file link", denial);
}

std::optional<Refusal> check_tempfile(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  // file tempfile ?nameVar? ?template?: without a directory in the template
  // Tcl uses the host's temporary directory (and silently does so for a
  // non-native one), so the directory must be given and granted.
  const std::string template_path = objc > 2 ? std::string{as_view(objv[2])} : std::string{};
  const auto slash = template_path.rfind('/');
  if (slash != std::string::npos) {
    Tcl_Obj* directory = Tcl_NewStringObj(template_path.c_str(), static_cast<int>(slash == 0 ? 1 : slash));
    Tcl_IncrRefCount(directory);
    Tcl_Obj* absolute = Tcl_FSGetNormalizedPath(nullptr, directory);
    const bool allowed = absolute != nullptr &&
                         guard.session->filesystem().check("file tempfile", as_view(absolute), "/").writable;
    Tcl_DecrRefCount(directory);
    if (allowed) {
      return std::nullopt;
    }
  }
  return Refusal{kPathDenied, "file tempfile", template_path,
                 "file tempfile: the template must name a directory under the sandbox's granted roots"};
}

std::optional<Refusal> refuse_hidden_access(std::string_view owner, std::string_view sub) {
  for (const std::string_view name : {"invokehidden", "expose", "marktrusted"}) {
    if (abbreviates(sub, name)) {
      const std::string operation = std::string{owner} + " " + std::string{name};
      return Refusal{kInterpDenied, operation, {}, operation + ": not permitted in the script sandbox"};
    }
  }
  return std::nullopt;
}

bool creates_safe(int objc, Tcl_Obj* const objv[]) {
  for (int i = 2; i < objc; ++i) {
    const std::string_view word = as_view(objv[i]);
    if (word == "--" || word.substr(0, 1) != "-") {
      break;
    }
    if (abbreviates(word, "-safe", 2)) {
      return true;
    }
  }
  return false;
}

// S-07: Tcl's default recursion limit; a higher one lets recursion through a
// command without NRE support (`lsort -command`) overflow the C stack.
constexpr Tcl_WideInt kMaxRecursionLimit = 1000;

// `<interp> recursionlimit ?n?` (`value` is n, or nullptr for the query).
std::optional<Refusal> refuse_recursion_limit(std::string_view owner, Tcl_Obj* value) {
  Tcl_WideInt limit = 0;
  if (value == nullptr || Tcl_GetWideIntFromObj(nullptr, value, &limit) != TCL_OK || limit <= kMaxRecursionLimit) {
    return std::nullopt;  // a query, Tcl's own error, or a limit at or below the default
  }
  const std::string operation = std::string{owner} + " recursionlimit";
  return Refusal{kInterpDenied, operation, std::string{as_view(value)},
                 operation + ": a limit above " + std::to_string(kMaxRecursionLimit) +
                     " is not permitted in the script sandbox"};
}

std::optional<Refusal> check_child(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc < 2) {
    return std::nullopt;
  }
  if (abbreviates(as_view(objv[1]), "recursionlimit")) {
    return refuse_recursion_limit(guard.operation, objc >= 3 ? objv[2] : nullptr);
  }
  return refuse_hidden_access(guard.operation, as_view(objv[1]));
}

std::optional<Refusal> check_interp(Guard& /*guard*/, int objc, Tcl_Obj* const objv[]) {
  if (objc < 2) {
    return std::nullopt;
  }
  const std::string_view sub = as_view(objv[1]);
  if (abbreviates(sub, "create") && !creates_safe(objc, objv)) {
    return Refusal{kInterpDenied, "interp create", words_after(2, objc, objv),
                   "interp create: only safe interpreters (-safe) may be created in the script sandbox"};
  }
  if (abbreviates(sub, "recursionlimit")) {
    return refuse_recursion_limit("interp", objc >= 4 ? objv[3] : nullptr);
  }
  return refuse_hidden_access("interp", sub);
}

// After `interp create -safe name`: guard the child's object command, through
// which `name invokehidden exec …` would otherwise reach its hidden commands.
// A child created deeper (`interp create -safe {a b}`) gets its command in
// the safe interpreter `a`, where Tcl already refuses hidden access.
void guard_new_child(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc < 2 || !abbreviates(as_view(objv[1]), "create")) {
    return;
  }
  Tcl_Interp* interp = guard.session->interp();
  Tcl_Obj* path = Tcl_GetObjResult(interp);
  int length = 0;
  if (Tcl_ListObjLength(nullptr, path, &length) != TCL_OK || length != 1) {
    return;
  }
  const std::string name{as_view(path)};
  Tcl_Obj* keep = Tcl_DuplicateObj(path);
  Tcl_IncrRefCount(keep);
  guard_command(*guard.session, name, Guard{nullptr, {}, name, check_child, nullptr, false});
  Tcl_SetObjResult(interp, keep);
  Tcl_DecrRefCount(keep);
}

// The packages of Tcl's embedded library (its pkgIndex.tcl files).
const std::set<std::string, std::less<>>& embedded_packages() {
  static const std::set<std::string, std::less<>> packages = [] {
    std::set<std::string, std::less<>> names;
    constexpr std::string_view kKey = "package ifneeded ";
    for (const auto& file : embedded_library()) {
      if (file.path.size() < 13 || file.path.substr(file.path.size() - 13) != "/pkgIndex.tcl") {
        continue;
      }
      for (auto at = file.content.find(kKey); at != std::string_view::npos; at = file.content.find(kKey, at + 1)) {
        const std::string_view rest = file.content.substr(at + kKey.size());
        names.emplace(rest.substr(0, rest.find_first_of(" \t\n")));
      }
    }
    return names;
  }();
  return packages;
}

std::optional<Refusal> check_package(Guard& guard, int objc, Tcl_Obj* const objv[]) {
  if (objc < 3 || !abbreviates(as_view(objv[1]), "require")) {
    return std::nullopt;
  }
  const int at = as_view(objv[2]) == "-exact" && objc > 3 ? 3 : 2;
  const std::string name{as_view(objv[at])};
  Tcl_Interp* interp = guard.session->interp();
  if (Tcl_PkgPresent(interp, name.c_str(), nullptr, 0) != nullptr ||
      embedded_packages().find(name) != embedded_packages().end()) {
    Tcl_ResetResult(interp);
    return std::nullopt;
  }
  Tcl_ResetResult(interp);
  return Refusal{kPackageDenied, "package require", name,
                 "package require: \"" + name +
                     "\" is not a package of Tcl's embedded library; the script sandbox loads no other packages"};
}

// S-08: `encoding system NAME` and `encoding dirs LIST` change process-wide
// state shared by every project's interpreter; reading them stays allowed.
std::optional<Refusal> check_encoding(Guard& /*guard*/, int objc, Tcl_Obj* const objv[]) {
  if (objc < 3) {
    return std::nullopt;
  }
  for (const std::string_view name : {"system", "dirs"}) {
    if (abbreviates(as_view(objv[1]), name)) {
      const std::string operation = "encoding " + std::string{name};
      return Refusal{kRemovedDenied, operation, std::string{as_view(objv[2])},
                     operation + ": changes process-wide state; not permitted in the script sandbox"};
    }
  }
  return std::nullopt;
}

// The `file` subcommands that read or change the filesystem.
constexpr std::array kFileAccess = {"atime",  "attributes", "copy",  "delete",   "executable", "exists",
                                    "isdirectory", "isfile", "lstat", "mtime", "mkdir",
                                    "owned",  "readable",   "readlink", "rename", "size", "stat",
                                    "type",   "writable"};

// The pre-check of the `file` subcommand `sub` that refuses changes under a
// read-only root (S-03); nullptr for the ones that only read.
PreCheck write_check(std::string_view sub) {
  if (sub == "delete" || sub == "mkdir" || sub == "rename") {
    return check_write_paths;
  }
  if (sub == "copy") {
    return check_write_target;
  }
  if (sub == "attributes" || sub == "mtime" || sub == "atime") {
    return check_write_setter;
  }
  return nullptr;
}

}  // namespace

void install_sandbox_commands(SandboxSession& session, const SandboxPolicy& policy) {
  Tcl_Interp* interp = session.interp();
  for (const auto& name : policy.removed_commands) {
    Tcl_DeleteCommand(interp, name.c_str());
    Tcl_CreateObjCommand(interp, name.c_str(), removed_command, new Removed{&session, name}, delete_removed);
  }
  guard_command(session, "open", Guard{nullptr, {}, "open", check_open, nullptr, false});
  guard_command(session, "interp", Guard{nullptr, {}, "interp", check_interp, guard_new_child, false});
  guard_command(session, "package", Guard{nullptr, {}, "package", check_package, nullptr, false});
  guard_command(session, "cd", Guard{nullptr, {}, "cd", nullptr, nullptr, false});
  guard_command(session, "glob", Guard{nullptr, {}, "glob", nullptr, nullptr, false});
  guard_command(session, "source", Guard{nullptr, {}, "source", check_source, nullptr, true});
  guard_command(session, "::tcl::file::tempfile", Guard{nullptr, {}, "file tempfile", check_tempfile, nullptr, false});
  guard_command(session, "::tcl::file::link", Guard{nullptr, {}, "file link", check_link, nullptr, false});
  guard_command(session, "encoding", Guard{nullptr, {}, "encoding", check_encoding, nullptr, false});
  for (const char* sub : kFileAccess) {
    guard_command(session, std::string{"::tcl::file::"} + sub,
                  Guard{nullptr, {}, std::string{"file "} + sub, write_check(sub), nullptr, false});
  }
}

void synthesize_environment(Tcl_Interp* interp, const SandboxPolicy& policy) {
  // Unsetting the whole array drops Tcl's traces that mirror `env` into the
  // process environment (its array-level unset trace is a no-op).
  Tcl_UnsetVar2(interp, "env", nullptr, TCL_GLOBAL_ONLY);
  Tcl_EvalEx(interp, "array set ::env {}", -1, TCL_EVAL_GLOBAL);
  for (const auto& [name, value] : policy.environment) {
    Tcl_SetVar2(interp, "env", name.c_str(), value.c_str(), TCL_GLOBAL_ONLY);
  }
  Tcl_ResetResult(interp);
}

}  // namespace hefesto::script::tcl_detail
