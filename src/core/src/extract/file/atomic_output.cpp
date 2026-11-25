// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// AtomicOutput (Q-EXTRACT-27, fix in both modes; capability
// extract-commands "Output path and file location"; openspec/changes/
// m7-extraction design.md D12). The target is resolved as ExtFileOpen
// does: ExtLocalPath, else the cell's file without its extension, else the
// cell name; a relative name goes through the cell search path as PaOpen
// does for writing; when no candidate can be written, the retry with the
// path stripped in the current directory (unless a `.mag` of that name is
// there). A symbolic link is followed and the file it points to replaced;
// a directory where no temporary file can be created is written in place.
// (Magic ref: extract/ExtCell.c:153-251, utils/path.c:647-790)
//
// Owner: EXTNODE (design D3).

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/extract/file/atomic_output.hpp"

namespace hefesto::extract {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kCodeOpen = "EXTRACT-FILE-003";
constexpr std::string_view kCodePath = "EXTRACT-FILE-004";
constexpr int kMaxLinkHops = 40;

bool is_local(const platform::FileSystem& files) {
  return dynamic_cast<const platform::LocalFileSystem*>(&files) != nullptr;
}

/// The file a path names after following symbolic links (local only).
std::string follow_links(const platform::FileSystem& files, const std::string& path) {
  if (!is_local(files)) {
    return path;
  }
  fs::path p{path};
  for (int hop = 0; hop < kMaxLinkHops; ++hop) {
    std::error_code ec;
    if (!fs::is_symlink(p, ec)) {
      break;
    }
    const fs::path link = fs::read_symlink(p, ec);
    if (ec) {
      break;
    }
    p = link.is_absolute() ? link : p.parent_path() / link;
  }
  return p.string();
}

std::string temporary_name(const std::string& destination) {
  const fs::path p{destination};
  return (p.parent_path() / ("." + p.filename().string() + ".hefesto-tmp")).string();
}

/// True when the target can be opened for writing in place (fopen "w"
/// would succeed): an existing writable file, or a new file in a writable
/// directory.
bool writable_in_place(platform::FileSystem& files, const std::string& destination) {
  if (!is_local(files)) {
    return false;
  }
  std::ofstream probe(destination, std::ios::app | std::ios::binary);
  return probe.is_open();
}

struct Prepared {
  std::string destination;
  std::optional<std::string> temporary;
  bool in_place = false;
};

/// The temporary file next to `candidate`'s destination, or the in-place
/// fallback; nullopt when neither works.
std::optional<Prepared> prepare(platform::FileSystem& files, const std::string& candidate) {
  Prepared out;
  out.destination = follow_links(files, candidate);
  const std::string temp = temporary_name(out.destination);
  if (files.write_atomic(temp, "").ok()) {
    out.temporary = temp;
    return out;
  }
  if (writable_in_place(files, out.destination)) {
    out.in_place = true;
    return out;
  }
  return std::nullopt;
}

std::string join(const std::string& directory, const std::string& name) {
  if (directory.empty() || directory == ".") {
    return name;
  }
  return (fs::path{directory} / name).string();
}

std::string current_dir(const OutputLocation& where) {
  return where.current_directory.empty() ? std::string{"."} : where.current_directory;
}

/// ExtFileOpen's name before the suffix: ExtLocalPath, the cell file
/// without its extension, the cell name.
std::string base_name(cell::LookupContext& ctx, const db::CellDef& cell, const OutputLocation& where) {
  if (where.local_path) {
    const std::string& dir = *where.local_path;
    if (dir == ".") {
      return join(current_dir(where), cell.name());
    }
    if (!ctx.fs.exists(dir)) {
      ctx.sink.emit(diag::make(kCodePath, "Path \"" + dir + "\" does not exist;  attempting to create it."));
      if (!ctx.fs.create_directories(dir).ok() || !ctx.fs.exists(dir)) {
        ctx.sink.emit(diag::make(kCodePath, "Path \"" + dir + "\" does not exist;  saving locally."));
        return join(current_dir(where), cell.name());
      }
    }
    return join(dir, cell.name());
  }
  const fs::path file = cell.file_path();
  if (!file.empty()) {
    return file.has_extension() ? (file.parent_path() / file.stem()).string() : file.string();
  }
  return cell.name();
}

bool is_direct(const std::string& name) {
  return name.starts_with('/') || name == "." || name == ".." || name.starts_with("./") ||
         name.starts_with("../");
}

/// `file` in each expanded entry of a search path (`.` is the current
/// directory).
void append_entries(std::vector<std::string>& out, cell::LookupContext& ctx, const std::vector<std::string>& entries,
                    const std::string& file, const OutputLocation& where) {
  for (const std::string& entry : entries) {
    const std::optional<std::string> dir = cell::expand_entry(entry, ctx.vars);
    if (dir && !dir->empty()) {
      out.push_back(join(*dir == "." ? current_dir(where) : *dir, file));
    }
  }
}

/// The places PaOpen tries for writing `file`, in order: a rooted or
/// explicitly relative name as given, a `~`/`$` name expanded, otherwise
/// each search-path entry, then the library path.
std::vector<std::string> candidates(cell::LookupContext& ctx, const std::string& file, const OutputLocation& where) {
  if (file.starts_with('~') || file.starts_with('$')) {
    const std::optional<std::string> expanded = cell::expand_entry(file, ctx.vars);
    return expanded ? std::vector<std::string>{*expanded} : std::vector<std::string>{};
  }
  if (file.starts_with('/')) {
    return {file};
  }
  std::vector<std::string> out;
  if (is_direct(file)) {
    out.push_back(join(current_dir(where), file));
  }
  append_entries(out, ctx, ctx.paths.path, file, where);
  append_entries(out, ctx, ctx.paths.library, file, where);
  return out;
}

std::optional<std::pair<std::string, Prepared>> search(cell::LookupContext& ctx, const std::string& file,
                                                       const OutputLocation& where) {
  for (const std::string& candidate : candidates(ctx, file, where)) {
    if (std::optional<Prepared> ready = prepare(ctx.fs, candidate)) {
      return std::pair{candidate, std::move(*ready)};
    }
    // PaOpen stops at an error other than "no such file": the directory
    // exists but cannot be written.
    const fs::path parent = fs::path{candidate}.parent_path();
    if (ctx.fs.exists(parent.empty() ? fs::path{"."} : parent)) {
      break;
    }
  }
  return std::nullopt;
}

/// ExtFileOpen's retry: the cell name without its path, in the current
/// directory, unless a `.mag` file named after the cell's file is there.
std::optional<std::pair<std::string, Prepared>> retry_stripped(cell::LookupContext& ctx, const db::CellDef& cell,
                                                               std::string_view suffix, const OutputLocation& where) {
  const std::string& name = cell.name();
  const std::size_t slash = name.rfind('/');
  const std::string stripped = slash == std::string::npos ? name : name.substr(slash + 1);
  const fs::path file = cell.file_path();
  if (file.has_parent_path() && ctx.fs.exists(join(current_dir(where), file.filename().string() + ".mag"))) {
    return std::nullopt;
  }
  const std::string candidate = join(current_dir(where), stripped + std::string{suffix});
  if (std::optional<Prepared> ready = prepare(ctx.fs, candidate)) {
    return std::pair{candidate, std::move(*ready)};
  }
  return std::nullopt;
}

}  // namespace

AtomicOutput::AtomicOutput(AtomicOutput&& other) noexcept
    : target_(std::move(other.target_)),
      temporary_(std::exchange(other.temporary_, std::nullopt)),
      destination_(std::move(other.destination_)),
      fs_(std::exchange(other.fs_, nullptr)),
      in_place_(other.in_place_),
      truncated_(other.truncated_) {}

AtomicOutput& AtomicOutput::operator=(AtomicOutput&& other) noexcept {
  if (this != &other) {
    abandon();
    target_ = std::move(other.target_);
    temporary_ = std::exchange(other.temporary_, std::nullopt);
    destination_ = std::move(other.destination_);
    fs_ = std::exchange(other.fs_, nullptr);
    in_place_ = other.in_place_;
    truncated_ = other.truncated_;
  }
  return *this;
}

AtomicOutput::~AtomicOutput() {
  abandon();
}

diag::Outcome<AtomicOutput> AtomicOutput::open(const cell::LookupContext& ctx, const db::CellDef& cell,
                                               std::string_view suffix) {
  return open(ctx, cell, suffix, OutputLocation{});
}

diag::Outcome<AtomicOutput> AtomicOutput::open(const cell::LookupContext& ctx_in, const db::CellDef& cell,
                                               std::string_view suffix, const OutputLocation& where) {
  cell::LookupContext ctx = ctx_in;
  const std::string file = base_name(ctx, cell, where) + std::string{suffix};
  auto found = search(ctx, file, where);
  if (!found) {
    found = retry_stripped(ctx, cell, suffix, where);
  }
  if (!found) {
    return diag::make(kCodeOpen, "Cannot open output file " + file + ".");
  }
  AtomicOutput out;
  out.target_ = found->first;
  out.destination_ = found->second.destination;
  out.temporary_ = found->second.temporary;
  out.in_place_ = found->second.in_place;
  out.fs_ = &ctx.fs;
  return out;
}

diag::Status AtomicOutput::write(std::string_view text) {
  if (fs_ == nullptr) {
    return diag::make(kCodeOpen, "The output file " + target_ + " is not open.");
  }
  if (temporary_) {
    if (!fs_->append(*temporary_, text).ok()) {
      return diag::make(kCodeOpen, "Cannot write output file " + target_ + ".");
    }
    return diag::Done{};
  }
  if (!truncated_) {
    // In place, as Magic's fopen(name, "w"): the first write truncates.
    truncated_ = true;
    std::ofstream out(destination_, std::ios::trunc | std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good() ? diag::Status{diag::Done{}}
                      : diag::Status{diag::make(kCodeOpen, "Cannot write output file " + target_ + ".")};
  }
  if (!fs_->append(destination_, text).ok()) {
    return diag::make(kCodeOpen, "Cannot write output file " + target_ + ".");
  }
  return diag::Done{};
}

diag::Status AtomicOutput::commit() {
  if (fs_ == nullptr) {
    return diag::make(kCodeOpen, "The output file " + target_ + " is not open.");
  }
  if (in_place_) {
    if (!truncated_) {
      diag::Status empty = write({});
      if (!empty.ok()) {
        return empty;
      }
    }
    fs_ = nullptr;
    return diag::Done{};
  }
  const std::string temp = *temporary_;
  if (!fs_->rename(temp, destination_).ok()) {
    abandon();
    return diag::make(kCodeOpen, "Cannot replace output file " + target_ + ".");
  }
  temporary_.reset();
  fs_ = nullptr;
  return diag::Done{};
}

void AtomicOutput::abandon() noexcept {
  if (fs_ != nullptr && temporary_) {
    (void)fs_->remove(*temporary_);
  }
  temporary_.reset();
  fs_ = nullptr;
}

}  // namespace hefesto::extract
