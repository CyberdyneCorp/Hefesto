// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFReadFile and efReadDef (see reader.hpp).
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#include "reader.hpp"

#include <cstddef>
#include <optional>
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

diag::Diagnostic cannot_read(std::string_view name) {
  return diag::make(codes::kCannotRead, "Cannot read extract file " + std::string{name} + ".ext");
}

int as_int(const std::vector<std::string>& argv, std::size_t i) {
  return i < argv.size() ? c_atoi(argv[i]) : 0;
}

// atoCap(s) * scale as Magic's float arithmetic gives it.
float scaled_cap(std::string_view text, int scale) {
  return static_cast<float>(c_atof(text)) * static_cast<float>(scale);
}

std::span<const std::string> tail(const std::vector<std::string>& argv, std::size_t from) {
  return from < argv.size() ? std::span<const std::string>{argv}.subspan(from) : std::span<const std::string>{};
}

// What reading one file produced.
struct DefRead {
  bool found = false;
  std::vector<std::uint32_t> children;  // used defs, in use-table order
  bool child_dosubckt = true;
};

struct DeferredEquiv {
  std::string name1;
  std::string name2;
};

// The records of one file and its overlay (efReadDef's switch). rscale,
// cscale and lscale are efReadDef's locals: the overlay starts with the
// `.ext` file's values.
class FileReader {
 public:
  FileReader(ReadState& st, Def& def, bool dosubckt, bool toplevel)
      : st_(st), def_(def), dosubckt_(dosubckt), toplevel_(toplevel) {}

  void read(std::string_view text);
  void apply_deferred();

 private:
  void line(const std::vector<std::string>& argv);
  void record(Key key, const std::vector<std::string>& argv);
  void connection_record(Key key, const std::vector<std::string>& argv);
  void header_record(Key key, const std::vector<std::string>& argv);
  void scale(const std::vector<std::string>& argv);
  void node(const std::vector<std::string>& argv, bool substrate);
  void rnode(const std::vector<std::string>& argv);
  void port(const std::vector<std::string>& argv);
  void equiv(const std::vector<std::string>& argv);
  void resistor(const std::vector<std::string>& argv);
  void resistclasses(const std::vector<std::string>& argv);
  void tech(const std::vector<std::string>& argv);
  void style(const std::vector<std::string>& argv);
  void version(const std::vector<std::string>& argv);
  void device(const std::vector<std::string>& argv);
  void fet(const std::vector<std::string>& argv);
  void attr(const std::vector<std::string>& argv);
  void use(const std::vector<std::string>& argv);
  void note_res_name(const std::string& name);
  db::Rect rect(const std::vector<std::string>& argv, std::size_t first) const;

  ReadState& st_;
  Def& def_;
  bool dosubckt_;
  bool toplevel_;
  int rscale_ = 1;
  int cscale_ = 1;
  std::vector<DeferredEquiv> deferred_;  // newest last
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
    // Magic's argv[128] overflows here. The line is ignored with a warning
    // every console prints, in both modes (approved 2026-11-21, M6-S8).
    st_.sink.emit(diag::make(codes::kTooManyTokens,
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
    case Key::node: node(argv, false); break;
    case Key::substrate: node(argv, true); break;
    case Key::rnode: rnode(argv); break;
    case Key::port: port(argv); break;
    case Key::subcap: adjust_subcap(st_, def_, argv[1], scaled_cap(argv[2], cscale_)); break;
    case Key::killnode: def_.kills.push_back(argv[1]); break;
    case Key::use: use(argv); break;
    case Key::device: device(argv); break;
    case Key::fet: fet(argv); break;
    case Key::attr: attr(argv); break;
    case Key::parameters: build_device_params(st_, argv[1], tail(argv, 2)); break;
    case Key::abstract_: def_.abstract = true; break;
    case Key::primitive: def_.primitive = true; break;
    default: connection_record(key, argv); break;
  }
}

void FileReader::connection_record(Key key, const std::vector<std::string>& argv) {
  switch (key) {
    case Key::equiv: equiv(argv); break;
    case Key::merge:
      build_merge(st_, def_, argv[1], argv[2], argv.size() > 3 ? scaled_cap(argv[3], cscale_) : 0.0F, tail(argv, 4));
      break;
    case Key::cap: build_cap(st_, def_, argv[1], argv[2], scaled_cap(argv[3], cscale_)); break;
    case Key::resistor: resistor(argv); break;
    default: header_record(key, argv); break;
  }
}

void FileReader::header_record(Key key, const std::vector<std::string>& argv) {
  switch (key) {
    case Key::scale: scale(argv); break;
    case Key::resistclass: resistclasses(argv); break;
    case Key::tech: tech(argv); break;
    case Key::style: style(argv); break;
    case Key::version: version(argv); break;
    case Key::timestamp:
      if (st_.options.timestamps) {
        st_.stamps.push_back(FileStamp{def_.name, c_atol(argv[1])});
      }
      break;
    default: break;  // adjust, connect, distance: nothing a netlister reads
  }
}

void FileReader::scale(const std::vector<std::string>& argv) {
  rscale_ = c_atoi(argv[1]);
  if (rscale_ == 0) {
    st_.read_error("Bad resistance scaling = 0; reset to 1.");
    rscale_ = 1;
  }
  cscale_ = c_atoi(argv[2]);
  if (cscale_ == 0) {
    st_.read_error("Bad capacitance scaling = 0; reset to 1.");
    cscale_ = 1;
  }
  auto lscale = static_cast<float>(c_atof(argv[3]));
  if (st_.units_per_lambda && lscale != *st_.units_per_lambda) {
    st_.loc_scale = lscale / *st_.units_per_lambda;
    lscale = *st_.units_per_lambda;
  }
  if (lscale == 0.0F) {
    st_.read_error("Bad linear scaling = 0; reset to 1.");
    lscale = 1.0F;
  }
  if (st_.options.noscale) {
    return;
  }
  def_.scale = lscale;
  if (st_.ef_scale != lscale) {
    if (st_.ef_scale != 0.0F) {
      st_.scale_changed = true;
      st_.ef_scale = 1.0F;
    } else {
      st_.ef_scale = lscale;
    }
  }
}

void FileReader::node(const std::vector<std::string>& argv, bool substrate) {
  // Signed, as Magic's int: a short substrate line (3 to 6 tokens) gives a
  // negative count and the message.
  std::ptrdiff_t ac = static_cast<std::ptrdiff_t>(argv.size()) - 7;
  if ((ac & 1) != 0) {
    --ac;  // the last token is an attribute list
  }
  if (ac < 2 * static_cast<std::ptrdiff_t>(st_.classes())) {
    st_.read_error("Too few area/perim values: assuming remainder are zero");
  }
  // `substrate` needs only 3 tokens but Magic reads the 7th (its layer)
  // from stale tokens: a missing layer is none here (fixed in both modes).
  std::optional<std::string_view> layer;
  if (argv.size() > 6) {
    layer = argv[6];
  }
  const std::span<const std::string> pa =
      ac > 0 ? tail(argv, 7).first(static_cast<std::size_t>(ac)) : std::span<const std::string>{};
  const float cap = argv.size() > 3 ? scaled_cap(argv[3], cscale_) : 0.0F;
  build_node(st_, def_, NodeRecord{argv[1], as_int(argv, 4), as_int(argv, 5), layer, substrate, false, toplevel_, cap, pa});
}

void FileReader::note_res_name(const std::string& name) {
  if (st_.res_names) {
    st_.res_names->insert(name);
  }
}

// A port number past the cap fails the read in both modes: the netlisters
// number ports past the highest one (Magic sizes an array by it), so a
// hostile number could exhaust memory or overflow (design D21).
void FileReader::port(const std::vector<std::string>& argv) {
  const int index = as_int(argv, 2);
  if (index > st_.options.limits.max_port_index) {
    st_.fail(codes::kLimit, st_.file + ", line " + std::to_string(st_.line) + ": port number " + std::to_string(index) +
                                " is larger than " + std::to_string(st_.options.limits.max_port_index));
    return;
  }
  def_.subcircuit = def_.subcircuit || dosubckt_;
  build_port_node(st_, def_, argv[1], index, as_int(argv, 3), as_int(argv, 4), argv[7], toplevel_);
}

void FileReader::rnode(const std::vector<std::string>& argv) {
  note_res_name(argv[1]);
  // rnode needs 5 tokens but Magic reads the 7th (its layer): a missing
  // coordinate is 0 and a missing layer is none, instead of reading stale
  // tokens.
  std::optional<std::string_view> layer;
  if (argv.size() > 6) {
    layer = argv[6];
  }
  build_node(st_, def_,
             NodeRecord{argv[1], as_int(argv, 4), as_int(argv, 5), layer, false, false, false, scaled_cap(argv[3], cscale_)});
}

void FileReader::equiv(const std::vector<std::string>& argv) {
  const bool resist = st_.options.resist_overlay;
  if (resist && !st_.reading_overlay) {
    deferred_.push_back(DeferredEquiv{argv[1], argv[2]});
  }
  build_equiv(st_, def_, argv[1], argv[2], resist);
}

void FileReader::resistor(const std::vector<std::string>& argv) {
  note_res_name(argv[1]);
  note_res_name(argv[2]);
  // Q-NETLIST-12: Magic adds 0.5 mohm, a leftover rounding term.
  const double value = static_cast<double>(rscale_) * c_atof(argv[3]);
  const auto res = static_cast<float>(st_.magic_compat() ? 0.5 + value : value);
  build_resistor(st_, def_, argv[1], argv[2], res);
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
    st_.resist_changed = true;
  }
}

void FileReader::tech(const std::vector<std::string>& argv) {
  const std::string& tech_name = st_.options.tech_name;
  if (tech_name.empty() || argv[1] == tech_name) {
    st_.tech = argv[1];
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
    st_.style = argv[1];  // the caller knows no styles: no check
    return;
  }
  if (const auto units = st_.options.style_units(argv[1])) {
    st_.units_per_lambda = *units;
    st_.selected_style = argv[1];
    st_.style = argv[1];
    return;
  }
  st_.message(codes::kUnknownStyle, "Warning:  .ext file style " + argv[1] + " is not known in this technology!");
  st_.style.reset();
}

void FileReader::version(const std::vector<std::string>& argv) {
  if (argv[1] != "8.3") {
    st_.read_error("Cell was extracted using version " + argv[1] + " of the extractor.");
    st_.read_error("   It should be re-extracted.");
  }
}

db::Rect FileReader::rect(const std::vector<std::string>& argv, std::size_t first) const {
  return db::Rect{scale_coord(st_, as_int(argv, first)), scale_coord(st_, as_int(argv, first + 1)),
                  scale_coord(st_, as_int(argv, first + 2)), scale_coord(st_, as_int(argv, first + 3))};
}

void FileReader::device(const std::vector<std::string>& argv) {
  // A `vsource` record parses with the default rule (its value, then the
  // terminals) in both modes; Magic sets no token count for the class but
  // never reads it here (Q-NETLIST-51).
  const auto cls = device_class(argv[1]);
  if (!cls) {
    st_.read_error("Unknown device class");
    return;
  }
  const db::Rect r = rect(argv, 3);
  const auto args = tail(argv, 7);
  if (argv[2] == "Short") {
    build_short(st_, def_, args);
    return;
  }
  if (!build_device(st_, def_, *cls, argv[2], r, args)) {
    st_.read_error("Incomplete terminal description for device");
  }
}

void FileReader::fet(const std::vector<std::string>& argv) {
  if (!build_device(st_, def_, kDevFet, argv[1], rect(argv, 2), tail(argv, 6))) {
    st_.read_error("Incomplete terminal description for fet");
  }
}

void FileReader::attr(const std::vector<std::string>& argv) {
  build_attr(st_, def_, argv[1], rect(argv, 2), argv[6], argv[7]);
}

void FileReader::use(const std::vector<std::string>& argv) {
  const Transform t{as_int(argv, 3), as_int(argv, 4), as_int(argv, 5),
                    as_int(argv, 6), as_int(argv, 7), as_int(argv, 8)};
  build_use(st_, def_, argv[1], argv[2], t);
}

// After the overlay: each deferred equiv, newest first, merges unless one
// of its names is on an `rnode` or `resist` record of the overlay.
void FileReader::apply_deferred() {
  for (auto it = deferred_.rbegin(); it != deferred_.rend(); ++it) {
    const bool on_network = st_.res_names && (st_.res_names->count(it->name1) != 0 || st_.res_names->count(it->name2) != 0);
    if (!on_network) {
      build_equiv(st_, def_, it->name1, it->name2, false);
    }
  }
  deferred_.clear();
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

// Reads one found file into `reader`; false (with the failure noted) when
// it cannot be read.
bool read_file(ReadState& st, cell::LookupContext& ctx, FileReader& reader, const std::string& path,
               std::string_view name) {
  const auto size = ctx.fs.size(path);
  if (size.ok() && size.value() > st.options.limits.max_file_bytes) {
    st.fail(codes::kLimit, path + ": file larger than " + std::to_string(st.options.limits.max_file_bytes) + " bytes");
    return false;
  }
  const auto text = ctx.fs.read(path);
  if (!text.ok()) {
    st.note_failure(cannot_read(name));
    return false;
  }
  st.file = path;
  reader.read(text.value());
  return true;
}

// The `.res.ext` overlay of a def that was read (efReadDef's readfile
// loop again), then the deferred equivs.
void read_overlay(ReadState& st, cell::LookupContext& ctx, FileReader& reader, const Def& def) {
  if (st.options.resist_overlay && !st.fatal) {
    if (const auto overlay = find_ext_file(st.options, ctx, def.name, ".res.ext")) {
      st.res_names.emplace();
      st.reading_overlay = true;
      (void)read_file(st, ctx, reader, *overlay, def.name);
      st.reading_overlay = false;
    }
  }
  reader.apply_deferred();
  st.res_names.reset();
}

// efReadDef for one def (without the recursion into its uses).
DefRead read_def(ReadState& st, cell::LookupContext& ctx, std::uint32_t id, bool dosubckt, bool toplevel) {
  DefRead result;
  Def& def = st.defs.at(id);
  def.available = true;
  def.graph.set_compat_attrs(st.magic_compat());
  const auto path = find_ext_file(st.options, ctx, def.name, ".ext");
  if (!path) {
    st.note_failure(cannot_read(def.name));
    return result;
  }
  FileReader reader{st, def, dosubckt, toplevel};
  if (!read_file(st, ctx, reader, *path, def.name)) {
    return result;
  }
  result.found = true;
  read_overlay(st, ctx, reader, def);
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

// efReadDef's three places: the -p path, the loaded cell's directory, and
// the cell search path when no -p path was given.
std::optional<std::string> find_ext_file(const ReadOptions& options, cell::LookupContext& ctx, std::string_view name,
                                         std::string_view suffix) {
  const std::string file = std::string{name} + std::string{suffix};
  const auto& p = options.search_path;
  if (!p.empty()) {
    if (auto found = search(ctx, file, p)) {
      return found;
    }
  }
  if (options.cell_directory) {
    if (const auto dir = options.cell_directory(name)) {
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

diag::Outcome<std::uint32_t> read_tree(ReadState& st, cell::LookupContext& ctx, std::string_view root) {
  const std::uint32_t root_id = st.defs.lookup_or_create(root);
  st.loc_scale = 1.0F;
  st.units_per_lambda = st.options.units_per_lambda;
  DefRead r = read_def(st, ctx, root_id, st.options.subcircuits, true);
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
  if (st.ef_scale == 0.0F) {
    st.ef_scale = 1.0F;
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
