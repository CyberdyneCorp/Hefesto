// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.sch`/`.sym` reader (capability schematic-file-format; design D6).
// Input is hostile: every step is bounded by the bytes, nothing is
// allocated from a count read in the file, and embedded blocks nested in
// blocks are kept on an explicit stack of frames, at most
// kMaxEmbeddedBlockDepth deep, never by recursion.
//
// Owner: FORMAT.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "block_symbol.hpp"
#include "codes.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "lexer.hpp"

namespace hefesto::schem {

namespace {

using file_detail::Lexer;

// Where the records of a frame go: the file's document, the entry of a
// top-level block in the file's `embedded` list, the symbol a nested block
// defines in place (merged into the frame below when it closes), or a
// scratch document shared by every discarded block.
enum class Sink : std::uint8_t { file, block, nested, discard };

// Indices into Reader::versions_: unset ("Leading instance record",
// Q-SCHEM-3) and "1.0".
constexpr std::size_t kVersionUnset = 0;
constexpr std::size_t kVersion10 = 1;

// The range of a rotation or flip: xschem stores them as 16-bit integers.
constexpr int kMinOrientation = -32768;
constexpr int kMaxOrientation = 32767;

// The instance a `[` inside a block may define in place: the `C` record
// directly before it.
struct PendingInstance {
  std::string reference;
  Placement placement;
  bool ignored = false;  // symbol_ignore=true: the instance is not drawn
};

// The reading state of one document being filled.
struct Frame {
  Sink sink = Sink::file;
  std::size_t index = 0;  // Sink::block: entry in `embedded`; Sink::nested: entry in Reader::nested_
  std::size_t version = kVersionUnset;
  int level = 0;          // symbol level inside the top-level block (that block is 0)
  Placement placement;    // Sink::nested: where the symbol is placed in the level below
  bool first_record = true;
  bool k_read = false;
  std::optional<PendingInstance> pending;  // in a block: the record just read was a `C`
};

// The Q-SCHEM-3 branch point ("Version record and header text"): with
// xschem-compat ON the version is unset until the first record ends; OFF,
// a file with no `v` record is version 1.0 from its first record.
std::size_t initial_version(bool xschem_compat) {
  return xschem_compat ? kVersionUnset : kVersion10;
}

// "Version record and header text": the keys `xschem` (near the start),
// `version` and `file_version` are removed by the property update rules
// (each with one following space or LF, "Property strings" rule 5), then
// leading blanks and one LF. The PDK round trips of SCHEM-RT-02 (gf180
// headers that start with a blank line) confirm the separator goes with
// the key.
std::string header_text(const std::string& v) {
  std::string header = v;
  const auto xschem = header.find("xschem");
  if (xschem != std::string::npos && xschem < 50) {
    header = set_token(header, "xschem", std::nullopt);
  }
  header = set_token(header, "version", std::nullopt);
  header = set_token(header, "file_version", std::nullopt);
  const auto first = header.find_first_not_of(" \t");
  header.erase(0, first == std::string::npos ? header.size() : first);
  if (!header.empty() && header.front() == '\n') {
    header.erase(0, 1);
  }
  return header;
}

class Reader {
 public:
  Reader(std::string_view bytes, const ReadOptions& options) : lex_(bytes), options_(options) {}

  Document run() {
    result_.kind = options_.kind;
    frames_.push_back(new_frame(Sink::file, 0));
    while (true) {
      lex_.skip_whitespace();
      if (lex_.at_end()) {
        break;
      }
      read_record();
    }
    if (in_block()) {
      warn(file_codes::kUnclosedEmbeddedBlock, "EOF reached inside an embedded symbol block, missing ]");
    }
    while (in_block()) {
      close_block();
    }
    finish(frames_.front());
    return std::move(result_);
  }

 private:
  [[nodiscard]] Frame new_frame(Sink sink, std::size_t index) const {
    Frame frame;
    frame.sink = sink;
    frame.index = index;
    frame.version = initial_version(options_.xschem_compat);
    return frame;
  }

  Frame& frame() { return frames_.back(); }
  [[nodiscard]] bool in_block() const { return frames_.size() > 1; }
  [[nodiscard]] bool at_block_level() const { return frames_.size() == 2; }

  Document& doc_of(const Frame& frame) {
    switch (frame.sink) {
      case Sink::file:
        return result_;
      case Sink::block:
        return result_.embedded[frame.index].symbol;
      case Sink::nested:
        return nested_[frame.index];
      default:
        return scratch_;
    }
  }

  // The document the current record goes to. Once a block's symbol hit the
  // 40-level limit, nothing more of that block is kept, at any level.
  Document& doc() { return truncated_ && in_block() ? scratch_ : doc_of(frames_.back()); }

  void read_record() {
    const char tag = lex_.take();
    const std::size_t depth = frames_.size();
    std::optional<PendingInstance> pending = std::move(frame().pending);
    frame().pending.reset();
    dispatch(tag, std::move(pending));
    // The record belongs to the frame it started in: a `[` opened a frame
    // above it, a `]` closed the frame it ended.
    end_record(frames_[std::min(depth, frames_.size()) - 1]);
    lex_.skip_to_line_end();
  }

  static void end_record(Frame& frame) {
    if (frame.first_record) {
      frame.first_record = false;
      if (frame.version == kVersionUnset) {
        frame.version = kVersion10;
      }
    }
  }

  void dispatch(char tag, std::optional<PendingInstance> pending) {
    switch (tag) {
      case 'v':
        read_version();
        break;
      case 'K':
      case 'G':
      case 'V':
      case 'S':
      case 'F':
      case 'E':
        read_global(tag);
        break;
      case 'L':
      case 'B':
        read_line_or_box(tag);
        break;
      case 'A':
        read_arc();
        break;
      case 'P':
        read_polygon();
        break;
      case 'T':
        read_text();
        break;
      case 'N':
        read_wire();
        break;
      case 'C':
        read_instance();
        break;
      case '[':
        open_block(std::move(pending));
        break;
      case ']':
        close_block_or_skip();
        break;
      case '#':
        break;  // the rest of the line is discarded by read_record
      default:
        skip_unknown();
        break;
    }
  }

  void warn(std::string_view code, std::string text) {
    if (options_.warnings != nullptr) {
      options_.warnings->emit(diag::make(code, std::move(text), diag::SourceLocation{options_.source_name, lex_.line()}));
    }
  }

  std::string brace() {
    auto read = lex_.read_brace_string();
    if (read.unterminated) {
      warn(file_codes::kUnterminatedString, "EOF reached, malformed {...} string input, missing close brace");
    }
    return std::move(read.value);
  }

  // A skipped record ("Malformed input"): the rest of the line goes.
  void missing(std::string_view kind) {
    warn(file_codes::kMissingFields, "missing fields for " + std::string{kind} + " object, ignoring");
    lex_.skip_to_line_end();
  }

  template <std::size_t N>
  bool read_numbers(std::array<double, N>& out) {
    for (double& value : out) {
      const auto read = lex_.read_double();
      if (!read) {
        return false;
      }
      value = *read;
    }
    return true;
  }

  std::optional<int> read_layer(std::string_view kind) {
    const auto layer = lex_.read_int();
    if (!layer || *layer < 0 || *layer >= kLayerCount) {
      warn(file_codes::kBadLayer, "wrong or missing layer number for " + std::string{kind} + " object, ignoring");
      lex_.skip_to_line_end();
      return std::nullopt;
    }
    return layer;
  }

  // A rotation or flip of a `T` or `C` record: an integer field that also
  // fits xschem's 16-bit storage. A value outside -32768..32767 (or one that
  // does not fit 32 bits) reads as a missing field, in both modes ("Record
  // grammar", Q-SCHEM-34 approved 2026-11-25).
  std::optional<int> read_orientation() {
    const auto value = lex_.read_int();
    if (!value || *value < kMinOrientation || *value > kMaxOrientation) {
      return std::nullopt;
    }
    return value;
  }

  // A `v` record sets the frame's file version; inside a block it is
  // otherwise ignored for the symbol ("Embedded symbols").
  void read_version() {
    const std::string v = brace();
    auto version = get_token(v, "file_version");
    if (version && !version->empty() && *version != "1.0") {
      frame().version = versions_.size();
      versions_.push_back(std::move(*version));
    } else {
      frame().version = kVersion10;
    }
    if (!in_block()) {
      doc().header_text = header_text(v);
    }
  }

  void read_global(char tag) {
    std::string value = brace();
    if (in_block()) {
      block_global(tag, std::move(value));
      return;
    }
    GlobalProperties& globals = doc().globals;
    switch (tag) {
      case 'K':
        globals.k = std::move(value);
        frame().k_read = true;
        break;
      case 'G':
        globals.g = std::move(value);
        break;
      case 'V':
        globals.v = std::move(value);
        break;
      case 'S':
        globals.s = std::move(value);
        break;
      case 'F':
        globals.f = std::move(value);
        break;
      default:
        globals.e = std::move(value);
        break;
    }
  }

  // "Embedded symbols": `K`, or `G` when no `K` was read, is the symbol
  // property at the block's own level; every other global record is ignored.
  void block_global(char tag, std::string value) {
    if (!at_block_level() || (tag != 'K' && tag != 'G') || (tag == 'G' && frame().k_read)) {
      return;
    }
    doc().globals.k = std::move(value);
    frame().k_read = frame().k_read || tag == 'K';
  }

  void read_line_or_box(char tag) {
    const std::string_view kind = tag == 'L' ? "LINE" : "xRECT";
    const auto layer = read_layer(kind);
    if (!layer) {
      return;
    }
    std::array<double, 4> v{};
    if (!read_numbers(v)) {
      missing(kind);
      return;
    }
    LineRecord record{*layer, {v[0], v[1]}, {v[2], v[3]}, brace()};
    if (tag == 'L') {
      file_detail::order_endpoints(record.p1, record.p2);
      add_line(std::move(record));
    } else {
      file_detail::order_corners(record.p1, record.p2);
      if (!in_block() || !file_detail::graph_box(record)) {
        add_kept(doc().boxes, std::move(record));
      }
    }
  }

  // Inside a block, objects with symbol_ignore=true are dropped.
  template <typename Record>
  void add_kept(std::vector<Record>& to, Record record) {
    if (!in_block() || !file_detail::symbol_ignored(record.props)) {
      to.push_back(std::move(record));
    }
  }

  void add_line(LineRecord record) { add_kept(doc().lines, std::move(record)); }

  void read_arc() {
    const auto layer = read_layer("ARC");
    if (!layer) {
      return;
    }
    std::array<double, 5> v{};
    if (!read_numbers(v)) {
      missing("ARC");
      return;
    }
    add_kept(doc().arcs, ArcRecord{*layer, {v[0], v[1]}, v[2], v[3], v[4], brace()});
  }

  void read_polygon() {
    const auto layer = read_layer("POLYGON");
    if (!layer) {
      return;
    }
    const auto count = lex_.read_int();
    if (!count || *count < 0) {
      missing("POLYGON");
      return;
    }
    PolygonRecord record{*layer, {}, {}};
    std::array<double, 2> xy{};
    for (int i = 0; i < *count; ++i) {  // no reserve: the count is untrusted
      if (!read_numbers(xy)) {
        missing("POLYGON");
        return;
      }
      record.points.push_back({xy[0], xy[1]});
    }
    record.props = brace();
    add_kept(doc().polygons, std::move(record));
  }

  void read_text() {
    TextRecord record;
    record.text = brace();
    std::array<double, 2> origin{};
    std::array<double, 2> scale{};
    std::optional<int> rotation;
    std::optional<int> flip;
    const bool ok = read_numbers(origin) && (rotation = read_orientation()) && (flip = read_orientation()) &&
                    read_numbers(scale);
    if (!ok) {
      missing("TEXT");
      return;
    }
    record.origin = {origin[0], origin[1]};
    record.rotation = *rotation;
    record.flip = *flip;
    record.x_scale = scale[0];
    record.y_scale = scale[1];
    record.props = brace();
    if (!in_block() || !file_detail::floater_text(record)) {
      add_kept(doc().texts, std::move(record));
    }
  }

  // Inside a block a wire is a line on layer 1, kept in read order with the
  // `L 1` lines ("Embedded symbols").
  void read_wire() {
    std::array<double, 4> v{};
    if (!read_numbers(v)) {
      missing("WIRE");
      return;
    }
    WireRecord record{{v[0], v[1]}, {v[2], v[3]}, brace()};
    file_detail::order_endpoints(record.p1, record.p2);
    if (in_block()) {
      add_line(LineRecord{file_detail::kWireLineLayer, record.p1, record.p2, std::move(record.props)});
    } else {
      doc().wires.push_back(std::move(record));
    }
  }

  void read_instance() {
    InstanceRecord record;
    record.symbol = brace();
    std::array<double, 2> origin{};
    std::optional<int> rotation;
    std::optional<int> flip;
    if (!(read_numbers(origin) && (rotation = read_orientation()) && (flip = read_orientation()))) {
      missing("INSTANCE");
      return;
    }
    record.origin = {origin[0], origin[1]};
    record.rotation = *rotation;
    record.flip = *flip;
    record.props = brace();
    if (frame().version == kVersion10) {
      record.symbol = with_sym_extension(std::move(record.symbol));
    }
    if (!in_block()) {
      last_instance_ = result_.instances.size();
      result_.instances.push_back(std::move(record));
      return;
    }
    const bool ignored = file_detail::symbol_ignored(record.props);
    frame().pending = PendingInstance{record.symbol, {record.origin, record.rotation, record.flip}, ignored};
    if (!ignored) {
      doc().instances.push_back(std::move(record));
    }
  }

  void open_block(std::optional<PendingInstance> pending) {
    if (!in_block()) {
      open_top_block();
    } else if (!pending) {
      lex_.skip_past_closing_line();  // any other `[` inside a block
    } else if (frames_.size() > kMaxEmbeddedBlockDepth) {
      warn(file_codes::kBlockTooDeep, "embedded symbol blocks nested deeper than 64 levels; block skipped");
      lex_.skip_past_closing_line();
    } else {
      open_nested_block(*pending);
    }
  }

  // A `[` of the file's own level belongs to the last instance read before
  // it, whatever lies between ("Embedded symbols").
  void open_top_block() {
    if (!last_instance_) {
      warn(file_codes::kStrayEmbeddedBlock, "embedded symbol block [ without a preceding instance, skipped");
      lex_.skip_past_closing_line();
      return;
    }
    InstanceRecord& instance = result_.instances[*last_instance_];
    instance.props = set_token(instance.props, "embed", "true");
    if (instance.symbol.empty() || !references_.insert(instance.symbol).second) {
      lex_.skip_past_closing_line();
      return;
    }
    result_.embedded.push_back(EmbeddedSymbol{instance.symbol, {}});
    result_.embedded.back().symbol.kind = FileKind::symbol;
    frames_.push_back(new_frame(Sink::block, result_.embedded.size() - 1));
  }

  // A `[` right after a `C` inside a block defines that instance's symbol
  // in place, unless the symbol is found as a file.
  void open_nested_block(const PendingInstance& pending) {
    const bool kept = !truncated_ && !pending.ignored && frame().sink != Sink::discard;
    if (kept && options_.symbol_file_exists && options_.symbol_file_exists(pending.reference)) {
      lex_.skip_past_closing_line();
      return;
    }
    const int level = frame().level + 1;
    if (kept && level >= kMaxBlockSymbolLevel) {
      warn(file_codes::kBlockSymbolTooDeep, "Symbol recursively instantiating symbol: max depth reached, skipping");
      truncated_ = true;
    }
    if (!kept || truncated_) {
      push_discarded_block();
      return;
    }
    doc().instances.pop_back();  // the instance is drawn by merging its block
    nested_.emplace_back().kind = FileKind::symbol;
    Frame nested = new_frame(Sink::nested, nested_.size() - 1);
    nested.level = level;
    nested.placement = pending.placement;
    frames_.push_back(std::move(nested));
  }

  void push_discarded_block() {
    ++discarded_blocks_;
    Frame discarded = new_frame(Sink::discard, 0);
    discarded.level = frame().level + 1;
    frames_.push_back(std::move(discarded));
  }

  // A `]` ends the innermost block; outside any block it is an unknown record.
  void close_block_or_skip() {
    if (in_block()) {
      close_block();
    } else {
      skip_unknown();
    }
  }

  void close_block() {
    const Frame block = std::move(frames_.back());
    frames_.pop_back();
    finish(block);
    if (block.sink == Sink::nested) {
      file_detail::merge_nested(nested_.back(), block.placement, doc_of(frames_.back()));
      nested_.pop_back();
    } else if (block.sink == Sink::discard && --discarded_blocks_ == 0) {
      scratch_ = Document{};  // nothing reads a discarded block
    }
    if (!in_block()) {
      truncated_ = false;
    }
  }

  // "Global property records": a `.sym` with no `K` record whose `G` has a
  // non-empty `type` uses `G` as its symbol property. Blocks set `K` while
  // reading (block_global).
  void finish(const Frame& frame) {
    Document& doc = doc_of(frame);
    doc.file_version = frame.version == kVersionUnset ? "1.0" : versions_[frame.version];
    if (doc.kind != FileKind::symbol || frame.k_read || frame.sink != Sink::file) {
      return;
    }
    const auto type = get_token(doc.globals.g, "type");
    if (type && !type->empty()) {
      doc.globals.k = std::move(doc.globals.g);
      doc.globals.g.clear();
    }
  }

  // "Record grammar": an unknown record runs to the next LF outside braces.
  void skip_unknown() {
    while (!lex_.at_end() && lex_.peek() != '\n') {
      if (lex_.take() == '{' && lex_.read_brace_body().unterminated) {
        warn(file_codes::kUnterminatedString, "EOF reached, malformed {...} string input, missing close brace");
      }
    }
  }

  Lexer lex_;
  const ReadOptions& options_;
  Document result_;
  Document scratch_;              // the records of discarded blocks
  std::vector<Document> nested_;  // symbols of open nested blocks, innermost last
  std::size_t discarded_blocks_ = 0;
  bool truncated_ = false;  // the open top-level block hit the 40-level limit
  std::optional<std::size_t> last_instance_;  // in result_.instances
  std::vector<std::string> versions_{"", "1.0"};  // see kVersionUnset, kVersion10
  std::vector<Frame> frames_;
  std::set<std::string> references_;
};

}  // namespace

std::string with_sym_extension(std::string reference) {
  const auto slash = reference.find_last_of('/');
  const auto dot = reference.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
    reference.erase(dot);
  }
  return reference + ".sym";
}

std::string instance_symbol_reference(const Document& document, const InstanceRecord& instance) {
  return document.file_version == "1.0" ? with_sym_extension(instance.symbol) : instance.symbol;
}

diag::Outcome<Document> read_document(std::string_view bytes, const ReadOptions& options) {
  return Reader{bytes, options}.run();
}

}  // namespace hefesto::schem
