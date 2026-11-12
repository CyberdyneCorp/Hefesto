// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sandbox's Tcl_Filesystem (design D5) and the per-thread current
// session (sandbox_runtime.hpp).
//
// The layer is registered once per process in front of Tcl's native
// filesystem. Tcl asks every registered filesystem, newest first, whether it
// handles a path (pathInFilesystemProc). This one claims
//   - the embedded library tree (/__hefesto_embedded__/...), served
//     read-only from memory, and
//   - every path outside the granted roots of the thread's current session
//     (all paths when there is none): each access fails with EACCES and is
//     recorded as a denial;
// and leaves the paths under a granted root to the native filesystem. The
// directories above a granted root are claimed too, but their stat and
// access are answered from the host (Tcl walks them for `file mkdir`). Tcl
// caches the claim in the path object; switching the current session bumps
// Tcl's filesystem epoch so no cached claim crosses interpreters.

#include <tcl.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/script/tcl/embedded_library.hpp"
#include "script/tcl/sandbox_runtime.hpp"

namespace hefesto::script::tcl_detail {

namespace {

constexpr std::string_view kEmbeddedTop = "/__hefesto_embedded__";

// access(2) mode bits as Tcl passes them (POSIX values; unistd.h is not
// included in the core).
constexpr int kWriteAccess = 2;
constexpr int kExecuteAccess = 1;

thread_local SandboxSession* t_active = nullptr;
thread_local const SandboxSession* t_last = nullptr;

// ---- the embedded library tree -------------------------------------------------

struct EmbeddedNode {
  bool directory = false;
  std::string_view content;
};

// The path of `path` relative to the library root ("" for the root), or
// nullopt when it is not under it.
std::optional<std::string_view> library_relative(std::string_view path) {
  if (path == kVirtualLibraryRoot) {
    return std::string_view{};
  }
  if (path.size() > kVirtualLibraryRoot.size() && path.substr(0, kVirtualLibraryRoot.size()) == kVirtualLibraryRoot &&
      path[kVirtualLibraryRoot.size()] == '/') {
    return path.substr(kVirtualLibraryRoot.size() + 1);
  }
  return std::nullopt;
}

bool library_has_directory(std::string_view relative) {
  const std::string prefix = std::string{relative} + "/";
  const auto files = embedded_library();
  const auto it = std::lower_bound(files.begin(), files.end(), prefix,
                                   [](const EmbeddedFile& file, const std::string& key) { return file.path < key; });
  return it != files.end() && it->path.substr(0, prefix.size()) == prefix;
}

std::optional<EmbeddedNode> embedded_node(std::string_view path) {
  if (path == kEmbeddedTop) {
    return EmbeddedNode{true, {}};
  }
  const auto relative = library_relative(path);
  if (!relative) {
    return std::nullopt;
  }
  if (relative->empty() || library_has_directory(*relative)) {
    return EmbeddedNode{true, {}};
  }
  if (const auto content = find_embedded_file(*relative)) {
    return EmbeddedNode{false, *content};
  }
  return std::nullopt;
}

// The entries of the embedded directory `path`: name and whether it is a
// directory.
std::vector<std::pair<std::string, bool>> embedded_children(std::string_view path) {
  std::vector<std::pair<std::string, bool>> children;
  if (path == kEmbeddedTop) {
    children.emplace_back(std::string{kVirtualLibraryRoot.substr(kEmbeddedTop.size() + 1)}, true);
    return children;
  }
  const auto relative = library_relative(path);
  if (!relative) {
    return children;
  }
  const std::string prefix = relative->empty() ? std::string{} : std::string{*relative} + "/";
  for (const auto& file : embedded_library()) {
    if (!file.path.starts_with(prefix)) {
      continue;
    }
    // remove_prefix, not substr: GCC 11 cannot bound substr's length here and
    // rejects the inlined memchr of find() (-Wstringop-overread).
    std::string_view rest = file.path;
    rest.remove_prefix(prefix.size());
    const auto slash = rest.find('/');
    std::pair<std::string, bool> entry{std::string{rest.substr(0, slash)}, slash != std::string_view::npos};
    if (children.empty() || children.back() != entry) {
      children.push_back(std::move(entry));
    }
  }
  return children;
}

// ---- a read-only channel over an embedded file ------------------------------------

struct MemoryChannel {
  std::string_view content;
  std::size_t position = 0;
};

int memory_close(ClientData data, Tcl_Interp* /*interp*/) {
  delete static_cast<MemoryChannel*>(data);
  return 0;
}

int memory_input(ClientData data, char* buffer, int size, int* /*error*/) {
  auto& channel = *static_cast<MemoryChannel*>(data);
  const std::size_t left = channel.content.size() - channel.position;
  const std::size_t count = std::min(left, static_cast<std::size_t>(std::max(size, 0)));
  std::memcpy(buffer, channel.content.data() + channel.position, count);
  channel.position += count;
  return static_cast<int>(count);
}

int memory_output(ClientData /*data*/, const char* /*buffer*/, int /*size*/, int* error) {
  *error = EINVAL;
  return -1;
}

Tcl_WideInt memory_wide_seek(ClientData data, Tcl_WideInt offset, int mode, int* error) {
  auto& channel = *static_cast<MemoryChannel*>(data);
  Tcl_WideInt base = 0;
  if (mode == SEEK_CUR) {
    base = static_cast<Tcl_WideInt>(channel.position);
  } else if (mode == SEEK_END) {
    base = static_cast<Tcl_WideInt>(channel.content.size());
  }
  const Tcl_WideInt target = base + offset;
  if (target < 0 || target > static_cast<Tcl_WideInt>(channel.content.size())) {
    *error = EINVAL;
    return -1;
  }
  channel.position = static_cast<std::size_t>(target);
  return target;
}

int memory_seek(ClientData data, long offset, int mode, int* error) {
  return static_cast<int>(memory_wide_seek(data, offset, mode, error));
}

void memory_watch(ClientData /*data*/, int /*mask*/) {}

int memory_handle(ClientData /*data*/, int /*direction*/, ClientData* /*handle*/) {
  return TCL_ERROR;
}

const Tcl_ChannelType kMemoryChannelType = {
    "hefesto-embedded", TCL_CHANNEL_VERSION_5, memory_close, memory_input, memory_output, memory_seek,
    nullptr,           nullptr,              memory_watch, memory_handle, nullptr,       nullptr,
    nullptr,           nullptr,              memory_wide_seek, nullptr,   nullptr};

Tcl_Channel open_memory_channel(std::string_view content) {
  static std::atomic<unsigned long> counter{0};
  const std::string name = "embedded" + std::to_string(counter.fetch_add(1));
  return Tcl_CreateChannel(&kMemoryChannelType, name.c_str(), new MemoryChannel{content, 0}, TCL_READABLE);
}

// ---- helpers of the filesystem procedures ------------------------------------------

void audit(std::string_view path, FilesystemAccess::Verdict verdict) {
  if (const auto& observer = filesystem_audit()) {
    observer(FilesystemAccess{std::string{path}, verdict});
  }
}

std::string normalized(Tcl_Obj* path) {
  Tcl_Obj* norm = Tcl_FSGetNormalizedPath(nullptr, path);
  return std::string{as_view(norm != nullptr ? norm : path)};
}

// Records a denial of `access` on `path` and fails with EACCES.
int deny(std::string path, const char* access) {
  if (SandboxSession* session = active_session()) {
    session->record(PathDenial{access, std::move(path)});
  }
  Tcl_SetErrno(EACCES);
  return -1;
}

void set_open_error(Tcl_Interp* interp, const std::string& path) {
  if (interp != nullptr) {
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("couldn't open \"%s\": %s", path.c_str(), Tcl_PosixError(interp)));
  }
}

bool type_matches(const EmbeddedNode& node, const Tcl_GlobTypeData* types) {
  if (types == nullptr || types->type == 0) {
    return types == nullptr || (types->perm & TCL_GLOB_PERM_W) == 0;
  }
  return (node.directory && (types->type & TCL_GLOB_TYPE_DIR) != 0) ||
         (!node.directory && (types->type & TCL_GLOB_TYPE_FILE) != 0);
}

// ---- the Tcl_Filesystem procedures ----------------------------------------------

int fs_path_in_filesystem(Tcl_Obj* path, ClientData* client) {
  *client = nullptr;
  if (as_view(path).empty()) {
    return -1;  // Tcl's "" is no file of any filesystem
  }
  Tcl_Obj* norm = Tcl_FSGetNormalizedPath(nullptr, path);
  const std::string_view text = as_view(norm != nullptr ? norm : path);
  if (norm != nullptr && is_embedded_path(text)) {
    audit(text, FilesystemAccess::Verdict::embedded);
    return TCL_OK;
  }
  const SandboxSession* session = active_session();
  if (norm != nullptr && session != nullptr && session->filesystem().check("", text, "/").allowed) {
    audit(text, FilesystemAccess::Verdict::allowed);
    return -1;
  }
  audit(text, FilesystemAccess::Verdict::denied);
  return TCL_OK;
}

Tcl_Obj* fs_separator(Tcl_Obj* /*path*/) {
  return Tcl_NewStringObj("/", 1);
}

// True when `name` is a directory above a granted root of the current
// session (e.g. "/" or "/home" for the root /home/u/proj): Tcl stats these
// while creating or copying under the root (`file mkdir -p` walks from "/").
// Their metadata is revealed, nothing else.
bool is_root_ancestor(const std::string& name) {
  const SandboxSession* session = active_session();
  return session != nullptr && session->filesystem().is_root_ancestor(name);
}

int native_stat(const std::string& name, Tcl_StatBuf* buffer, bool follow) {
  const int code = follow ? ::stat(name.c_str(), buffer) : ::lstat(name.c_str(), buffer);
  if (code != 0) {
    Tcl_SetErrno(errno);
  }
  return code;
}

int fs_stat(Tcl_Obj* path, Tcl_StatBuf* buffer) {
  std::string name = normalized(path);
  if (!is_embedded_path(name)) {
    return is_root_ancestor(name) ? native_stat(name, buffer, true) : deny(std::move(name), "stat");
  }
  const auto node = embedded_node(name);
  if (!node) {
    Tcl_SetErrno(ENOENT);
    return -1;
  }
  std::memset(buffer, 0, sizeof(*buffer));
  buffer->st_mode = node->directory ? (S_IFDIR | 0555) : (S_IFREG | 0444);
  buffer->st_nlink = 1;
  buffer->st_size = static_cast<off_t>(node->content.size());
  return 0;
}

int fs_lstat(Tcl_Obj* path, Tcl_StatBuf* buffer) {
  std::string name = normalized(path);
  if (!is_embedded_path(name) && is_root_ancestor(name)) {
    return native_stat(name, buffer, false);
  }
  return fs_stat(path, buffer);
}

int fs_access(Tcl_Obj* path, int mode) {
  std::string name = normalized(path);
  if (!is_embedded_path(name) && is_root_ancestor(name) && (mode & kWriteAccess) == 0) {
    // A directory above a granted root: it exists and can be searched,
    // since the root below it is reachable.
    Tcl_StatBuf buffer;
    return native_stat(name, &buffer, true);
  }
  if (!is_embedded_path(name)) {
    return deny(std::move(name), "access");
  }
  const auto node = embedded_node(name);
  if (!node) {
    Tcl_SetErrno(ENOENT);
    return -1;
  }
  const bool executable_file = (mode & kExecuteAccess) != 0 && !node->directory;
  if ((mode & kWriteAccess) != 0 || executable_file) {
    Tcl_SetErrno(EACCES);
    return -1;
  }
  return 0;
}

Tcl_Channel fs_open(Tcl_Interp* interp, Tcl_Obj* path, int mode, int /*permissions*/) {
  std::string name = normalized(path);
  const bool read_only = (mode & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) == 0;
  const auto node = is_embedded_path(name) ? embedded_node(name) : std::nullopt;
  if (!read_only || !is_embedded_path(name)) {
    deny(name, "open");
    set_open_error(interp, name);
    return nullptr;
  }
  if (!node || node->directory) {
    Tcl_SetErrno(node ? EISDIR : ENOENT);
    set_open_error(interp, name);
    return nullptr;
  }
  return open_memory_channel(node->content);
}

int fs_match_in_directory(Tcl_Interp* /*interp*/, Tcl_Obj* result, Tcl_Obj* path, const char* pattern,
                          Tcl_GlobTypeData* types) {
  if (types != nullptr && (types->type & TCL_GLOB_TYPE_MOUNT) != 0) {
    return TCL_OK;  // Tcl asks every filesystem for its mount points in a directory: none
  }
  std::string name = normalized(path);
  if (!is_embedded_path(name)) {
    deny(std::move(name), "glob");
    return TCL_OK;  // no match; the guarded command reports the denial
  }
  if (pattern == nullptr) {
    const auto node = embedded_node(name);
    if (node && type_matches(*node, types)) {
      Tcl_ListObjAppendElement(nullptr, result, path);
    }
    return TCL_OK;
  }
  for (const auto& [child, directory] : embedded_children(name)) {
    const EmbeddedNode node{directory, {}};
    if (Tcl_StringCaseMatch(child.c_str(), pattern, 0) != 0 && type_matches(node, types)) {
      Tcl_Obj* tail = Tcl_NewStringObj(child.c_str(), tcl_length(child));
      Tcl_IncrRefCount(tail);
      Tcl_ListObjAppendElement(nullptr, result, Tcl_FSJoinToPath(path, 1, &tail));
      Tcl_DecrRefCount(tail);
    }
  }
  return TCL_OK;
}

int fs_utime(Tcl_Obj* path, struct utimbuf* /*times*/) {
  return deny(normalized(path), "utime");
}

Tcl_Obj* fs_link(Tcl_Obj* path, Tcl_Obj* /*to*/, int /*type*/) {
  deny(normalized(path), "link");
  return nullptr;
}

const char* const* fs_attribute_names(Tcl_Obj* path, Tcl_Obj** names) {
  static const char* const kNone[] = {nullptr};
  *names = nullptr;
  std::string name = normalized(path);
  if (!is_embedded_path(name)) {
    deny(std::move(name), "attributes");
  }
  return kNone;
}

int fs_attribute_get(Tcl_Interp* /*interp*/, int /*index*/, Tcl_Obj* path, Tcl_Obj** /*value*/) {
  deny(normalized(path), "attributes");
  return TCL_ERROR;
}

int fs_attribute_set(Tcl_Interp* /*interp*/, int /*index*/, Tcl_Obj* path, Tcl_Obj* /*value*/) {
  deny(normalized(path), "attributes");
  return TCL_ERROR;
}

int fs_create_directory(Tcl_Obj* path) {
  return deny(normalized(path), "mkdir");
}

int fs_remove_directory(Tcl_Obj* path, int /*recursive*/, Tcl_Obj** error) {
  *error = path;
  Tcl_IncrRefCount(*error);
  return deny(normalized(path), "rmdir");
}

int fs_delete_file(Tcl_Obj* path) {
  return deny(normalized(path), "delete");
}

int fs_copy_file(Tcl_Obj* source, Tcl_Obj* /*target*/) {
  return deny(normalized(source), "copy");
}

int fs_rename_file(Tcl_Obj* source, Tcl_Obj* /*target*/) {
  return deny(normalized(source), "rename");
}

int fs_copy_directory(Tcl_Obj* source, Tcl_Obj* /*target*/, Tcl_Obj** error) {
  *error = source;
  Tcl_IncrRefCount(*error);
  return deny(normalized(source), "copy");
}

int fs_load_file(Tcl_Interp* /*interp*/, Tcl_Obj* path, Tcl_LoadHandle* /*handle*/,
                 Tcl_FSUnloadFileProc** /*unload*/) {
  deny(normalized(path), "load");
  return TCL_ERROR;
}

int fs_chdir(Tcl_Obj* path) {
  return deny(normalized(path), "chdir");
}

const Tcl_Filesystem kSandboxFilesystem = {
    "hefesto-sandbox",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    fs_path_in_filesystem,
    nullptr,  // dupInternalRepProc: no internal representation
    nullptr,  // freeInternalRepProc
    nullptr,  // internalToNormalizedProc
    nullptr,  // createInternalRepProc
    nullptr,  // normalizePathProc: Tcl's native normalization applies
    nullptr,  // filesystemPathTypeProc
    fs_separator,
    fs_stat,
    fs_access,
    fs_open,
    fs_match_in_directory,
    fs_utime,
    fs_link,
    nullptr,  // listVolumesProc
    fs_attribute_names,
    fs_attribute_get,
    fs_attribute_set,
    fs_create_directory,
    fs_remove_directory,
    fs_delete_file,
    fs_copy_file,
    fs_rename_file,
    fs_copy_directory,
    fs_lstat,
    fs_load_file,
    nullptr,  // getCwdProc: the working directory is always native
    fs_chdir,
};

void make_current(SandboxSession* session) {
  t_active = session;
  if (session != nullptr && session != t_last) {
    // Path objects cache which filesystem claimed them; a claim made for
    // another interpreter's roots must not be reused.
    t_last = session;
    Tcl_FSMountsChanged(&kSandboxFilesystem);
  }
}

}  // namespace

int tcl_length(std::string_view text) noexcept {
  return text.size() > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(text.size());
}

std::string_view as_view(Tcl_Obj* object) {
  int length = 0;
  const char* text = Tcl_GetStringFromObj(object, &length);
  return std::string_view{text, static_cast<std::size_t>(length)};
}

std::function<void(const FilesystemAccess&)>& filesystem_audit() {
  static std::function<void(const FilesystemAccess&)> observer;
  return observer;
}

void register_filesystem() {
  Tcl_FSRegister(nullptr, &kSandboxFilesystem);
}

SandboxSession* active_session() noexcept {
  return t_active;
}

ActiveSession::ActiveSession(SandboxSession& session) : previous_(t_active) {
  make_current(&session);
}

ActiveSession::~ActiveSession() {
  make_current(previous_);
}

// ---- SandboxSession -------------------------------------------------------------------

SandboxSession::SandboxSession(Tcl_Interp* interp, const SandboxPolicy& policy, ScriptEvents* events)
    : interp_(interp), filesystem_(policy), events_(events) {}

void SandboxSession::record(PathDenial denial) {
  pending_.push_back(std::move(denial));
}

std::vector<PathDenial> SandboxSession::claim(std::size_t mark) {
  std::vector<PathDenial> claimed;
  if (mark < pending_.size()) {
    claimed.assign(std::make_move_iterator(pending_.begin() + static_cast<std::ptrdiff_t>(mark)),
                   std::make_move_iterator(pending_.end()));
    pending_.resize(mark);
  }
  return claimed;
}

int SandboxSession::refuse(const Refusal& refusal) {
  publish(refusal, current_location(interp_), true);
  Tcl_SetObjResult(interp_, Tcl_NewStringObj(refusal.message.data(), tcl_length(refusal.message)));
  const std::string code{refusal.code};
  Tcl_SetErrorCode(interp_, "HEFESTO", "SANDBOX", code.c_str(), nullptr);
  return TCL_ERROR;
}

void SandboxSession::report_unclaimed(std::string_view operation, std::string_view file, std::size_t mark) {
  const diag::SourceLocation location{std::string{file}, 0};
  for (auto& denial : claim(mark)) {
    const std::string name = operation.empty() ? denial.access : std::string{operation};
    publish(Refusal{"SCRIPT-SANDBOX-004", name, denial.path,
                    name + ": \"" + denial.path + "\" is outside the sandbox's granted roots"},
            location, false);
  }
}

std::vector<diag::Diagnostic> SandboxSession::diagnostics_since(std::size_t mark) const {
  if (mark >= diagnostics_.size()) {
    return {};
  }
  return {diagnostics_.begin() + static_cast<std::ptrdiff_t>(mark), diagnostics_.end()};
}

void SandboxSession::reset() noexcept {
  pending_.clear();
  diagnostics_.clear();
  held_.clear();
}

void SandboxSession::publish(const Refusal& refusal, const diag::SourceLocation& location, bool raised) {
  diagnostics_.push_back(diag::make(refusal.code, refusal.message, location));
  DeniedOperation event{refusal.operation, refusal.target, location};
  if (deferring_) {
    held_.push_back(HeldDenial{std::move(event), diagnostics_.size() - 1, refusal.message, raised});
  } else if (events_ != nullptr) {
    events_->publish_denied(event);
  }
}

void SandboxSession::end_deferral(const std::optional<std::string>& uncaught_message) {
  if (!deferring_) {
    return;
  }
  deferring_ = false;
  // The evaluation ended with the error of the newest denial raising that
  // message; every other raised denial was absorbed by the script.
  std::size_t uncaught = held_.size();
  for (std::size_t i = held_.size(); uncaught_message && i > 0; --i) {
    if (held_[i - 1].raised && held_[i - 1].message == *uncaught_message) {
      uncaught = i - 1;
      break;
    }
  }
  std::vector<HeldDenial> held = std::move(held_);
  held_.clear();
  for (std::size_t i = 0; i < held.size(); ++i) {
    HeldDenial& denial = held[i];
    denial.event.caught = denial.raised && i != uncaught;
    if (denial.event.caught && denial.diagnostic < diagnostics_.size()) {
      diagnostics_[denial.diagnostic].text += kCaughtSuffix;
    }
    if (events_ != nullptr) {
      events_->publish_denied(denial.event);
    }
  }
}

// ---- script location ----------------------------------------------------------------

namespace {

std::optional<std::string> dict_text(Tcl_Interp* interp, Tcl_Obj* dict, const char* key) {
  Tcl_Obj* name = Tcl_NewStringObj(key, -1);
  Tcl_IncrRefCount(name);
  Tcl_Obj* value = nullptr;
  const bool found = Tcl_DictObjGet(interp, dict, name, &value) == TCL_OK && value != nullptr;
  Tcl_DecrRefCount(name);
  return found ? std::optional<std::string>{std::string{as_view(value)}} : std::nullopt;
}

// Evaluates one command of fixed words directly (no new command frame).
std::optional<std::string> invoke_words(Tcl_Interp* interp, std::initializer_list<const char*> words) {
  std::vector<Tcl_Obj*> objv;
  for (const char* word : words) {
    objv.push_back(Tcl_NewStringObj(word, -1));
    Tcl_IncrRefCount(objv.back());
  }
  const int code = Tcl_EvalObjv(interp, static_cast<int>(objv.size()), objv.data(), 0);
  for (Tcl_Obj* object : objv) {
    Tcl_DecrRefCount(object);
  }
  if (code != TCL_OK) {
    return std::nullopt;
  }
  return std::string{as_view(Tcl_GetObjResult(interp))};
}

}  // namespace

diag::SourceLocation current_location(Tcl_Interp* interp) {
  Tcl_InterpState saved = Tcl_SaveInterpState(interp, TCL_OK);
  diag::SourceLocation location;
  if (invoke_words(interp, {"::tcl::info::frame", "0"})) {
    Tcl_Obj* frame = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    Tcl_IncrRefCount(frame);
    long line = 0;
    if (const auto text = dict_text(interp, frame, "line")) {
      line = std::strtol(text->c_str(), nullptr, 10);
    }
    location.line = line > 0 ? static_cast<std::uint32_t>(line) : 0U;
    if (auto file = dict_text(interp, frame, "file")) {
      location.file = std::move(*file);
    }
    Tcl_DecrRefCount(frame);
  }
  if (location.file.empty()) {
    location.file = invoke_words(interp, {"::tcl::info::script"}).value_or(std::string{});
  }
  static_cast<void>(Tcl_RestoreInterpState(interp, saved));
  return location;
}

}  // namespace hefesto::script::tcl_detail
