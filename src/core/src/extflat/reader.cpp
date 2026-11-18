// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFReadFile and efReadDef (see reader.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "reader.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "build.hpp"
#include "text.hpp"

namespace hefesto::extflat::detail {

void ReadState::read_error(std::string_view text) {
  sink.emit(diag::make(codes::kReadError, file + ", line " + std::to_string(line) + ": " + std::string{text},
                       diag::SourceLocation{file, line}));
}

void ReadState::message(std::string_view code, std::string text) {
  sink.emit(diag::make(code, std::move(text)));
}

void ReadState::fail(std::string_view code, std::string text) {
  if (!fatal) {
    fatal = diag::make_native(code, std::move(text));
    sink.emit(*fatal);
  }
}

namespace {

bool opens_directly(std::string_view file) noexcept {
  return file.starts_with("/") || file.starts_with("./") || file.starts_with("../");
}

// PaOpen without the library path: `file` itself when it is a path, else
// the first `<dir>/<file>` of `path` that exists.
std::optional<std::string> search(cell::LookupContext& ctx, const std::string& file,
                                  const std::vector<std::string>& path) {
  if (opens_directly(file)) {
    return ctx.fs.exists(file) ? std::optional{file} : std::nullopt;
  }
  if (file.starts_with("~") || file.starts_with("$")) {
    const auto expanded = cell::expand_entry(file, ctx.vars);
    return expanded && ctx.fs.exists(*expanded) ? expanded : std::nullopt;
  }
  for (const std::string& entry : path) {
    const auto dir = cell::expand_entry(entry, ctx.vars);
    if (!dir) {
      continue;
    }
    std::string candidate = dir->empty() ? file : *dir + "/" + file;
    if (ctx.fs.exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

// efReadDef's three places: the -p path, the loaded cell's directory, and
// the cell search path when no -p path was given.
std::optional<std::string> find_ext_file(const ReadState& st, cell::LookupContext& ctx, std::string_view name) {
  const std::string file = std::string{name} + ".ext";
  const auto& p = st.options.search_path;
  if (!p.empty()) {
    if (auto found = search(ctx, file, p)) {
      return found;
    }
  }
  if (st.options.cell_directory) {
    if (const auto dir = st.options.cell_directory(name)) {
      if (auto found = search(ctx, file, {*dir})) {
        return found;
      }
    }
  }
  if (p.empty()) {
    return search(ctx, file, ctx.paths.path);
  }
  return std::nullopt;
}

diag::Diagnostic cannot_read(std::string_view name) {
  return diag::make(codes::kCannotRead, "Cannot read extract file " + std::string{name} + ".ext");
}

int as_int(const std::vector<std::string>& argv, std::size_t i) {
  return i < argv.size() ? c_atoi(argv[i]) : 0;
}

// What reading one file produced.
struct DefRead {
  bool found = false;
  std::vector<std::uint32_t> children;  // used defs, in use-table order
  bool child_dosubckt = true;
};

// The records of one file (efReadDef's switch).
class FileReader {
 public:
  FileReader(ReadState& st, Def& def, bool dosubckt, bool toplevel)
      : st_(st), def_(def), dosubckt_(dosubckt), toplevel_(toplevel) {}

  void read(std::string_view text);

 private:
  void line(const std::vector<std::string>& argv);
  void record(Key key, const std::vector<std::string>& argv);
  void scale(const std::vector<std::string>& argv);
  void node(const std::vector<std::string>& argv, bool substrate);
  void rnode(const std::vector<std::string>& argv);
  void resistclasses(const std::vector<std::string>& argv);
  void tech(const std::vector<std::string>& argv);
  void style(const std::vector<std::string>& argv);
  void version(const std::vector<std::string>& argv);
  void device(const std::vector<std::string>& argv);
  void fet(const std::vector<std::string>& argv);
  void attr(const std::vector<std::string>& argv);
  void use(const std::vector<std::string>& argv);
  db::Point corner(const std::vector<std::string>& argv, std::size_t first) const;

  ReadState& st_;
  Def& def_;
  bool dosubckt_;
  bool toplevel_;
};

void FileReader::read(std::string_view text) {
  LineReader reader{text, st_.options.limits.max_line_bytes};
  st_.line = 0;
  while (const auto l = reader.next()) {
    st_.line = l->number;
    if (l->too_long) {
      st_.fail(codes::kLimit, st_.file + ", line " + std::to_string(l->number) + ": line longer than " +
                                  std::to_string(st_.options.limits.max_line_bytes) + " bytes");
      return;
    }
    line(l->tokens);
    if (st_.fatal) {
      return;
    }
  }
}

void FileReader::line(const std::vector<std::string>& argv) {
  if (argv.size() > kMaxTokens) {
    // Magic's argv[128] overflows here.
    st_.sink.emit(diag::make_native(codes::kTooManyTokens,
                                    st_.file + ", line " + std::to_string(st_.line) + ": more than " +
                                        std::to_string(kMaxTokens) + " tokens; line ignored",
                                    diag::SourceLocation{st_.file, st_.line}));
    return;
  }
  const auto key = lookup_key(argv[0]);
  if (!key) {
    st_.read_error("Unrecognized token \"" + argv[0] + "\" (ignored)");
    return;
  }
  if (argv.size() < key->min_tokens) {
    st_.read_error("Not enough tokens for " + argv[0] + " line");
    return;
  }
  record(key->key, argv);
}

void FileReader::record(Key key, const std::vector<std::string>& argv) {
  switch (key) {
    case Key::scale: scale(argv); break;
    case Key::node: node(argv, false); break;
    case Key::substrate: node(argv, true); break;
    case Key::rnode: rnode(argv); break;
    case Key::port:
      def_.subcircuit = def_.subcircuit || dosubckt_;
      build_port_node(st_, def_, argv[1], as_int(argv, 2), as_int(argv, 3), as_int(argv, 4), argv[7], toplevel_);
      break;
    case Key::equiv: build_equiv(st_, def_, argv[1], argv[2]); break;
    case Key::merge: build_merge(st_, def_, argv[1], argv[2]); break;
    case Key::cap: check_connection(st_, argv[1], argv[2]); break;
    case Key::resistor: check_connection(st_, argv[1], argv[2]); break;
    case Key::killnode: def_.kills.push_back(argv[1]); break;
    case Key::use: use(argv); break;
    case Key::device: device(argv); break;
    case Key::fet: fet(argv); break;
    case Key::attr: attr(argv); break;
    case Key::parameters:
      build_device_params(st_, def_, argv[1], std::span<const std::string>{argv}.subspan(2));
      break;
    case Key::resistclass: resistclasses(argv); break;
    case Key::tech: tech(argv); break;
    case Key::style: style(argv); break;
    case Key::version: version(argv); break;
    case Key::timestamp:
      if (st_.options.timestamps) {
        st_.stamps.push_back(FileStamp{def_.name, c_atol(argv[1])});
      }
      break;
    default: break;  // abstract, primitive, adjust, connect, distance, subcap: nothing the subset keeps
  }
}

void FileReader::scale(const std::vector<std::string>& argv) {
  if (c_atoi(argv[1]) == 0) {
    st_.read_error("Bad resistance scaling = 0; reset to 1.");
  }
  if (c_atoi(argv[2]) == 0) {
    st_.read_error("Bad capacitance scaling = 0; reset to 1.");
  }
  auto lscale = static_cast<float>(c_atof(argv[3]));
  if (st_.units_per_lambda && lscale != *st_.units_per_lambda) {
    st_.loc_scale = lscale / *st_.units_per_lambda;
    lscale = *st_.units_per_lambda;
  }
  if (lscale == 0.0F) {
    st_.read_error("Bad linear scaling = 0; reset to 1.");
  }
}

void FileReader::node(const std::vector<std::string>& argv, bool substrate) {
  std::size_t ac = argv.size() - 7;
  if ((ac & 1U) != 0) {
    --ac;  // the last token is an attribute list
  }
  if (ac < 2 * st_.resist_classes.value_or(0)) {
    st_.read_error("Too few area/perim values: assuming remainder are zero");
  }
  build_node(st_, def_,
             NodeRecord{argv[1], as_int(argv, 4), as_int(argv, 5), std::string_view{argv[6]}, substrate, false,
                        toplevel_});
}

void FileReader::rnode(const std::vector<std::string>& argv) {
  // rnode needs 5 tokens but Magic reads the 7th (its layer): a missing
  // coordinate is 0 and a missing layer is none, instead of reading stale
  // tokens.
  std::optional<std::string_view> layer;
  if (argv.size() > 6) {
    layer = argv[6];
  }
  build_node(st_, def_, NodeRecord{argv[1], as_int(argv, 4), as_int(argv, 5), layer, false, false, false});
}

void FileReader::resistclasses(const std::vector<std::string>& argv) {
  const std::size_t count = argv.size() - 1;
  if (!st_.resist_classes) {
    st_.resist_classes = count;
    for (std::size_t n = 0; n < count; ++n) {
      st_.resists.push_back(c_atoi(argv[n + 1]));
    }
    return;
  }
  bool mismatch = false;
  if (*st_.resist_classes != count) {
    st_.read_error("Number of resistance classes doesn't match:");
    mismatch = true;
  }
  for (std::size_t n = 0; !mismatch && n < count; ++n) {
    if (st_.resists[n] != c_atoi(argv[n + 1])) {
      st_.read_error("Resistance class values don't match:");
      mismatch = true;
    }
  }
  if (mismatch) {
    st_.read_error("Re-extract the entire tree with the same technology file");
  }
}

void FileReader::tech(const std::vector<std::string>& argv) {
  const std::string& tech_name = st_.options.tech_name;
  if (tech_name.empty() || argv[1] == tech_name) {
    return;
  }
  // Layout is loaded, so Magic refuses to switch technology.
  const std::string text = "Error: .ext file has different technology " + argv[1];
  st_.message(codes::kTechMismatch, text);
  st_.message(codes::kTechMismatch, "Load this technology and repeat.");
  st_.note_failure(diag::make(codes::kTechMismatch, text));
}

void FileReader::style(const std::vector<std::string>& argv) {
  if (!st_.options.style_units) {
    return;  // the caller knows no styles: no check
  }
  if (const auto units = st_.options.style_units(argv[1])) {
    st_.units_per_lambda = *units;
    st_.selected_style = argv[1];
    return;
  }
  st_.message(codes::kUnknownStyle, "Warning:  .ext file style " + argv[1] + " is not known in this technology!");
}

void FileReader::version(const std::vector<std::string>& argv) {
  if (argv[1] != "8.3") {
    st_.read_error("Cell was extracted using version " + argv[1] + " of the extractor.");
    st_.read_error("   It should be re-extracted.");
  }
}

db::Point FileReader::corner(const std::vector<std::string>& argv, std::size_t first) const {
  return db::Point{scale_coord(st_, as_int(argv, first)), scale_coord(st_, as_int(argv, first + 1))};
}

void FileReader::device(const std::vector<std::string>& argv) {
  const auto cls = device_class(argv[1]);
  if (!cls) {
    st_.read_error("Unknown device class");
    return;
  }
  const db::Point ll = corner(argv, 3);
  const auto args = std::span<const std::string>{argv}.subspan(7);
  if (argv[2] == "Short") {
    build_short(st_, def_, args);
    return;
  }
  if (!build_device(st_, def_, *cls, argv[2], ll, args)) {
    st_.read_error("Incomplete terminal description for device");
  }
}

void FileReader::fet(const std::vector<std::string>& argv) {
  const db::Point ll = corner(argv, 2);
  if (!build_device(st_, def_, kDevFet, argv[1], ll, std::span<const std::string>{argv}.subspan(6))) {
    st_.read_error("Incomplete terminal description for fet");
  }
}

void FileReader::attr(const std::vector<std::string>& argv) {
  if (def_.names.find(argv[1]) == def_.names.end()) {
    st_.read_error("Attribute for nonexistent node " + argv[1] + " ignored");
  }
}

void FileReader::use(const std::vector<std::string>& argv) {
  const Transform t{as_int(argv, 3), as_int(argv, 4), as_int(argv, 5),
                    as_int(argv, 6), as_int(argv, 7), as_int(argv, 8)};
  build_use(st_, def_, argv[1], argv[2], t);
}

// The used defs of `def` in use-table order.
std::vector<std::uint32_t> children_of(const Def& def) {
  std::vector<std::uint32_t> out;
  def.use_table.for_each([&](const auto& entry) {
    if (entry.value) {
      out.push_back(def.uses[*entry.value].def);
    }
  });
  return out;
}

// efReadDef for one def (without the recursion into its uses).
DefRead read_def(ReadState& st, cell::LookupContext& ctx, std::uint32_t id, bool dosubckt, bool toplevel) {
  DefRead result;
  Def& def = st.defs.at(id);
  def.available = true;
  const auto path = find_ext_file(st, ctx, def.name);
  if (!path) {
    st.note_failure(cannot_read(def.name));
    return result;
  }
  const auto size = ctx.fs.size(*path);
  if (size.ok() && size.value() > st.options.limits.max_file_bytes) {
    st.fail(codes::kLimit, *path + ": file larger than " + std::to_string(st.options.limits.max_file_bytes) + " bytes");
    return result;
  }
  const auto text = ctx.fs.read(*path);
  if (!text.ok()) {
    st.note_failure(cannot_read(def.name));
    return result;
  }
  result.found = true;
  st.file = *path;
  FileReader reader{st, def, dosubckt, toplevel};
  reader.read(text.value());
  result.children = children_of(def);
  result.child_dosubckt = dosubckt && !(def.subcircuit && !toplevel);
  return result;
}

struct Frame {
  std::vector<std::uint32_t> children;
  std::size_t next = 0;
  bool dosubckt = true;
};

// One step of the depth-first read: the next unread child of the top
// frame, read and pushed. Returns false when the frame is exhausted.
bool read_next_child(ReadState& st, cell::LookupContext& ctx, std::vector<Frame>& stack, std::size_t& files) {
  Frame& top = stack.back();
  if (top.next == top.children.size()) {
    return false;
  }
  const std::uint32_t child = top.children[top.next++];
  if (st.defs.at(child).available) {
    return true;
  }
  if (stack.size() > st.options.limits.max_depth || ++files > st.options.limits.max_cells) {
    st.fail(codes::kLimit, "the .ext hierarchy is deeper or larger than the read allows");
    return true;
  }
  DefRead r = read_def(st, ctx, child, top.dosubckt, false);
  if (r.found) {
    stack.push_back(Frame{std::move(r.children), 0, r.child_dosubckt});
  }
  return true;
}

}  // namespace

diag::Outcome<std::uint32_t> read_tree(ReadState& st, cell::LookupContext& ctx, std::string_view root) {
  const std::uint32_t root_id = st.defs.lookup_or_create(root);
  st.loc_scale = 1.0F;
  st.units_per_lambda = st.options.units_per_lambda;
  DefRead r = read_def(st, ctx, root_id, true, true);
  if (st.fatal) {
    return *st.fatal;
  }
  if (!r.found) {
    return *st.failure;
  }
  std::vector<Frame> stack;
  stack.push_back(Frame{std::move(r.children), 0, r.child_dosubckt});
  std::size_t files = 1;
  while (!stack.empty() && !st.fatal) {
    if (!read_next_child(st, ctx, stack, files)) {
      stack.pop_back();
    }
  }
  if (st.fatal) {
    return *st.fatal;
  }
  if (st.failure) {
    return *st.failure;
  }
  return root_id;
}

}  // namespace hefesto::extflat::detail
