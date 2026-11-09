// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Use blocks of the .mag reader (cell-file-format "Use block parsing";
// cell-library-resolution "Reading instance paths", "Duplicate cell
// conflicts on read"; cell-timestamps "Timestamps on instance read").
// Ported from Magic 8.3.694 database/DBio.c (dbReadUse, dbReadUseBody,
// dbStatCellFile, DBSameCellFile); Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).

#include <cstdlib>
#include <memory>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/cell/paths.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "mag_reader.hpp"

namespace hefesto::cell::detail {

namespace {

constexpr std::size_t kMaxPath = 1024;

std::string text_of(std::string_view line) {
  return std::string{chomp(line)};
}

struct UseBlock {
  std::string name;
  std::string id;
  std::string path;
  bool locked = false;
  db::ArrayInfo array;
  std::int32_t stamp = 0;
  db::Transform t;
  db::Rect box;
};

bool manhattan(const db::Transform& t) {
  if (t.a == 0) {
    return t.e == 0 && std::abs(t.b) == 1 && std::abs(t.d) == 1;
  }
  return t.b == 0 && t.d == 0 && std::abs(t.a) == 1 && std::abs(t.e) == 1;
}

bool read_array_line(ReadState& st, UseBlock& u) {
  if (!st.line.starts_with("array")) {
    return true;
  }
  Scan s{st.line};
  s.lit("array");
  std::int32_t v[6] = {};
  for (auto& x : v) {
    if (!s.integer(x)) {
      st.emit(codes::kFormatError, "Malformed \"array\" line: " + text_of(st.line));
      return false;
    }
  }
  u.array = db::ArrayInfo{v[0], v[1], v[3], v[4], st.scale(v[2]), st.scale(v[5])};
  return st.in.fgets(st.line);
}

bool read_stamp_line(ReadState& st, UseBlock& u) {
  if (!st.line.starts_with("timestamp")) {
    return true;
  }
  Scan s{st.line};
  s.lit("timestamp");
  if (!s.integer(u.stamp)) {
    st.emit(codes::kFormatError, "Malformed \"timestamp\" line: " + text_of(st.line));
    return false;
  }
  return st.in.fgets(st.line);
}

bool read_transform_line(ReadState& st, UseBlock& u) {
  Scan s{st.line};
  std::int32_t v[6] = {};
  bool ok = s.lit("transform");
  for (auto& x : v) {
    ok = ok && s.integer(x);
  }
  u.t = db::Transform{v[0], v[1], st.scale(v[2]), v[3], v[4], st.scale(v[5])};
  if (!ok || !manhattan(u.t)) {
    st.emit(codes::kFormatError, "Malformed or illegal \"transform\" line: " + text_of(st.line));
    return false;
  }
  return st.in.fgets(st.line);
}

bool read_box_line(ReadState& st, UseBlock& u) {
  Scan s{st.line};
  std::int32_t v[4] = {};
  bool ok = s.lit("box");
  for (auto& x : v) {
    ok = ok && s.integer(x);
  }
  if (!ok) {
    st.emit(codes::kFormatError, "Malformed \"box\" line: " + text_of(st.line));
    return false;
  }
  u.box = db::Rect{st.scale(v[0]), st.scale(v[1]), st.scale(v[2]), st.scale(v[3])};
  return true;
}

bool is_ancestor(const db::CellDef& a, const db::CellDef& b) {
  if (&a == &b) {
    return true;
  }
  for (const db::CellUse* u : b.instances()) {
    if (u->parent != nullptr && is_ancestor(a, *u->parent)) {
      return true;
    }
  }
  return false;
}

db::CellDef* create_placeholder(ReadState& st, const UseBlock& u, bool first_use) {
  auto created = st.ctx.library.create(u.name, st.tech().plane_count());
  if (!created.ok()) {
    st.ctx.sink.emit(created.error());
    return nullptr;
  }
  db::CellDef* sub = created.value();
  sub->set_timestamp(u.stamp);
  db::Rect r = u.box;
  if (r.x0 >= r.x1 || r.y0 >= r.y1) {
    const std::string before = std::to_string(r.x0) + " " + std::to_string(r.y0) + " " + std::to_string(r.x1) +
                               " " + std::to_string(r.y1);
    r.x1 = r.x1 <= r.x0 ? r.x0 + 1 : r.x1;
    r.y1 = r.y1 <= r.y0 ? r.y0 + 1 : r.y1;
    if (first_use) {
      st.emit(codes::kUseProblem, "Subcell has degenerate bounding box: " + before);
      st.emit(codes::kUseProblem, "Adjusting bounding box of subcell " + u.name + " of " + st.def.name() + " to " +
                                      std::to_string(r.x0) + " " + std::to_string(r.y0) + " " +
                                      std::to_string(r.x1) + " " + std::to_string(r.y1));
    }
  }
  sub->set_bboxes(r, r);
  return sub;
}

// --- path of the use line ----------------------------------------------------

/// Step 2 of "Reading instance paths"; false for an undefined variable.
bool expand_use_variable(ReadState& st, std::string& p) {
  if (p.empty() || p.front() != '$') {
    return true;
  }
  std::size_t start = 1;
  if (start < p.size() && p[start] == '{') {
    ++start;
  }
  std::size_t end = start + 1;
  while (end < p.size() && p[end] != '}' && p[end] != '/' && p[end] != '\n' && p[end] != ' ') {
    ++end;
  }
  end = std::min(end, p.size());
  const std::string name = p.substr(start, end - start);
  const std::size_t after = end < p.size() && p[end] == '}' ? end + 1 : end;
  const auto value = st.ctx.vars.get(name);
  if (!value) {
    st.emit(codes::kUndefinedVariable, "undefined variable " + name + " in the path of cell " + st.def.name() +
                                           "'s use line; the search paths are used");
    return false;  // Q-CELL-19
  }
  p = *value + p.substr(after);
  return true;
}

std::string parent_file(const ReadState& st) {
  return st.def.file_path().generic_string();
}

void join_parent_dir(ReadState& st, std::string& p, bool first_use) {
  if (!first_use || (!p.empty() && (p.front() == '/' || p.front() == '~'))) {
    return;
  }
  const std::string pf = parent_file(st);
  const std::size_t slash = pf.rfind('/');
  if (slash == std::string::npos) {
    return;
  }
  const std::string pd = pf.substr(0, slash);
  p = p.empty() ? pd : collapse_path(pd + "/" + p, st.ctx.host);
}

void remap_home(ReadState& st, std::string& p) {
  const std::string pf = parent_file(st);
  if (!p.starts_with("~/") || pf.empty() || pf.front() != '/') {
    return;
  }
  const auto home = st.ctx.vars.home();
  if (!home || (pf.starts_with(*home) && pf.size() > home->size() && pf[home->size()] == '/')) {
    return;
  }
  const std::size_t root_end = home->rfind('/');
  if (root_end == std::string::npos || !pf.starts_with(home->substr(0, root_end + 1))) {
    return;
  }
  const std::size_t user_end = pf.find('/', root_end + 1);
  if (user_end != std::string::npos) {
    p = pf.substr(0, user_end) + p.substr(1);
  }
}

// --- duplicate conflicts ----------------------------------------------------

struct Conflict {
  ReadState& st;
  UseBlock& u;
  db::CellDef*& sub;
  const std::string& dir;  // D
  bool first_use;
};

bool cell_present_in(ReadState& st, const std::string& dir, const std::string& name) {
  LookupContext lc{st.ctx.fs, st.ctx.paths, st.ctx.vars, st.ctx.sink};
  return search_file(lc, name + ".mag", {dir}, nullptr).has_value();
}

std::optional<platform::Path> stat_cell_file(ReadState& st, const std::string& path) {
  for (const std::string& candidate : {path + ".mag", path, path + ".mag.gz"}) {
    if (st.ctx.fs.exists(candidate) && !st.ctx.host.is_directory(candidate)) {
      return platform::Path{candidate};
    }
  }
  return std::nullopt;
}

bool same_cell_file(ReadState& st, const std::string& a, const std::string& b) {
  const auto pa = stat_cell_file(st, a);
  const auto pb = stat_cell_file(st, b);
  if (!pa || !pb) {
    return false;
  }
  return st.ctx.host.same_file(*pa, *pb).value_or(false);
}

bool tilde_equals(ReadState& st, const std::string& d, const std::string& e) {
  const auto home = st.ctx.vars.home();
  return home && e.starts_with(*home) && e.substr(home->size()) == d.substr(1);
}

void rename_duplicate(Conflict& c, const std::string& previous) {
  ReadState& st = c.st;
  st.emit(codes::kUsePath, "Duplicate cell in " + st.def.name() + ":  Instance of cell " + c.u.name +
                               " is from path " + c.dir + " but cell was previously read from " + previous + ".");
  std::string renamed;
  for (int i = 0;; ++i) {
    renamed = c.u.name + "__" + std::to_string(i);
    if (st.ctx.library.find(renamed) == nullptr) {
      break;
    }
  }
  st.emit(codes::kUsePath, "Cell name conflict:  Renaming original cell to " + renamed + ".");
  if (auto done = st.ctx.library.rename(*c.sub, renamed, true); !done.ok()) {
    st.ctx.sink.emit(done.error());
    return;
  }
  if (st.ctx.renamed_duplicates != nullptr) {
    st.ctx.renamed_duplicates->insert(c.sub);
  }
  UseBlock fresh = c.u;
  c.sub = create_placeholder(st, fresh, false);
  if (c.sub != nullptr) {
    c.sub->set_file_path(c.dir + "/" + c.u.name + ".mag");
  }
}

void report_bad_path(Conflict& c, const std::string& previous) {
  const bool is_dir = c.st.ctx.host.is_directory(c.dir);
  c.st.emit(codes::kUsePath, "Bad cell path in " + c.st.def.name() + ":  Instance of cell " + c.u.name +
                                 " specifies path " + c.dir +
                                 (is_dir ? ", which does not contain the cell." : ", which does not exist.") +
                                 "  Cell previously read from " + previous + " will be used instead.");
}

bool git_equivalent(Conflict& c, const std::string& e) {
  const auto h1 = c.st.ctx.host.git_head(e);
  const auto h2 = c.st.ctx.host.git_head(c.dir);
  if (!h1 || !h2 || *h1 != *h2) {
    return false;
  }
  c.st.emit(codes::kReadNote, "Cells " + c.u.name + " in " + e + " and " + c.dir +
                                  " have matching git repository commits and can be considered equivalent.");
  return true;
}

/// Case A: the existing recorded path has a directory E.
void conflict_with_dir(Conflict& c, const std::string& e) {
  ReadState& st = c.st;
  const std::string cwd = st.ctx.host.current_directory().generic_string();
  bool ok = c.dir.front() == '~' ? tilde_equals(st, c.dir, e) : c.dir == cwd;
  const bool check = e != c.dir && !st.options.dereference && c.first_use;
  if (ok || !check) {
    return;
  }
  ok = st.ctx.host.same_file(e, c.dir).value_or(false);
  if (ok) {
    return;
  }
  const bool missing = !cell_present_in(st, c.dir, c.u.name);
  if (!missing && git_equivalent(c, e)) {
    return;
  }
  if (missing) {
    report_bad_path(c, e);
  } else {
    rename_duplicate(c, e);
  }
}

/// Case B: the existing recorded path has no directory (Q-CELL-39: the
/// process working directory stands for PWD).
void conflict_in_cwd(Conflict& c, const std::string& recorded) {
  ReadState& st = c.st;
  const std::string cwd = st.ctx.host.current_directory().generic_string();
  bool ok = c.dir.front() == '~' ? tilde_equals(st, c.dir, cwd) : c.dir == cwd;
  if (ok || c.dir == cwd) {
    return;
  }
  ok = same_cell_file(st, recorded, c.dir + "/" + c.u.name);
  if (ok || st.options.dereference || !c.first_use) {
    return;
  }
  if (!cell_present_in(st, c.dir, c.u.name)) {
    report_bad_path(c, "the current directory");
  } else {
    rename_duplicate(c, "the current directory");
  }
}

void resolve_location(Conflict& c) {
  const std::string recorded = c.sub->file_path().generic_string();
  if (recorded.empty()) {
    c.sub->set_file_path(c.dir + "/" + c.u.name + ".mag");  // case C
    return;
  }
  const std::size_t slash = recorded.rfind('/');
  if (slash != std::string::npos) {
    conflict_with_dir(c, recorded.substr(0, slash));
  } else {
    conflict_in_cwd(c, recorded);
  }
}

void handle_path(ReadState& st, UseBlock& u, db::CellDef*& sub, bool first_use) {
  std::string p = u.path;
  if (!expand_use_variable(st, p)) {
    return;
  }
  join_parent_dir(st, p, first_use);
  remap_home(st, p);
  if (p.empty()) {
    return;
  }
  Conflict c{st, u, sub, p, first_use};
  resolve_location(c);
}

void place_use(ReadState& st, const UseBlock& u, db::CellDef& sub) {
  auto use = std::make_unique<db::CellUse>();
  use->id = u.locked ? u.id.substr(1) : u.id;
  use->def = &sub;
  use->parent = &st.def;
  use->transform = u.t;
  use->array = u.array;
  use->locked = u.locked;
  db::compute_use_bbox(*use);
  db::CellUse* raw = use.get();
  st.def.uses().push_back(std::move(use));
  sub.instances().insert(sub.instances().begin(), raw);
  if (u.stamp != sub.timestamp() || u.stamp == 0) {
    st.ctx.stamps.record(sub, u.box);
    if (!sub.flags().available) {
      sub.set_timestamp(0);
    } else {
      st.ctx.stamps.record(sub, sub.bbox());
    }
  }
}

bool read_block_lines(ReadState& st, UseBlock& u) {
  if (!st.in.fgets(st.line)) {
    return false;
  }
  return read_array_line(st, u) && read_stamp_line(st, u) && read_transform_line(st, u) && read_box_line(st, u);
}

bool read_use_body(ReadState& st, UseBlock& u) {
  if (u.path.size() >= kMaxPath) {
    st.emit(codes::kUsePath, "Path for cell " + u.name + " in \"use\" line is too long and will be ignored.");
    u.path.clear();
  }
  u.locked = !u.id.empty() && u.id.front() == '*';
  if (!read_block_lines(st, u)) {
    return false;
  }
  const bool first_use = st.seen_uses.insert(u.name).second;
  db::CellDef* sub = st.ctx.library.find(u.name);
  if (sub == nullptr) {
    sub = create_placeholder(st, u, first_use);
  } else if (is_ancestor(*sub, st.def)) {
    if (first_use) {
      st.emit(codes::kUseProblem, "Subcells are used circularly!");
      st.emit(codes::kUseProblem, "Ignoring subcell " + u.name + " of " + st.def.name() + ".");
    }
    return st.in.fgets(st.line);
  }
  if (sub != nullptr) {
    handle_path(st, u, sub, first_use);
  }
  if (sub != nullptr) {
    place_use(st, u, *sub);
  }
  return st.in.fgets(st.line);
}

}  // namespace

bool read_use(ReadState& st) {
  if (!st.line.starts_with("use")) {
    st.emit(codes::kFormatError, "Expected \"use\" line but saw: " + text_of(st.line));
    return false;
  }
  UseBlock u;
  Scan s{st.line};
  s.lit("use");
  if (!s.str(u.name, std::string::npos)) {
    st.emit(codes::kFormatError, "Malformed \"use\" line: " + text_of(st.line));
    return false;
  }
  if (s.str(u.id, std::string::npos)) {
    (void)s.str(u.path, std::string::npos);
  }
  return read_use_body(st, u);
}

}  // namespace hefesto::cell::detail
