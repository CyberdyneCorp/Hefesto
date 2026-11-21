// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.sch`/`.sym` reader (capability schematic-file-format; design D6).
// Input is hostile: every step is bounded by the bytes, nothing is
// allocated from a count read in the file, and embedded blocks nested in
// blocks are kept on an explicit stack of frames, never by recursion.
//
// Owner: FORMAT.

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "codes.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "lexer.hpp"

namespace hefesto::schem {

namespace {

using file_detail::Lexer;

// One document being filled: the file itself, or an embedded block.
struct Frame {
  Document doc;
  // The block's entry in the outermost document's `embedded` list; nullopt
  // for the file itself and for a discarded block.
  std::optional<std::size_t> slot;
  // nullopt while the version is unset ("Leading instance record", Q-SCHEM-3).
  std::optional<std::string> version;
  bool first_record = true;
  bool k_read = false;
  bool last_was_instance = false;
};

// The Q-SCHEM-3 branch point ("Version record and header text"): with
// xschem-compat ON the version is unset until the first record ends; OFF,
// a file with no `v` record is version 1.0 from its first record.
std::optional<std::string> initial_version(bool xschem_compat) {
  if (xschem_compat) {
    return std::nullopt;
  }
  return std::string{"1.0"};
}

void order_endpoints(Point& p1, Point& p2) noexcept {
  if (p2.x < p1.x || (p1.x == p2.x && p2.y < p1.y)) {
    std::swap(p1, p2);
  }
}

void order_corners(Point& p1, Point& p2) noexcept {
  if (p2.x < p1.x) {
    std::swap(p1.x, p2.x);
  }
  if (p2.y < p1.y) {
    std::swap(p1.y, p2.y);
  }
}

// File version 1.0 references name a symbol without extension: `.sym`
// replaces any extension of the last path element.
std::string with_sym_extension(std::string reference) {
  const auto slash = reference.find_last_of('/');
  const auto dot = reference.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
    reference.erase(dot);
  }
  return reference + ".sym";
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
    frames_.push_back(new_frame(options_.kind));
    while (true) {
      lex_.skip_whitespace();
      if (lex_.at_end()) {
        break;
      }
      read_record();
    }
    while (frames_.size() > 1) {
      warn(file_codes::kUnclosedEmbeddedBlock, "EOF reached inside an embedded symbol block, missing ]");
      close_block();
    }
    finish(frames_.front());
    return std::move(frames_.front().doc);
  }

 private:
  [[nodiscard]] Frame new_frame(FileKind kind) const {
    Frame frame;
    frame.doc.kind = kind;
    frame.version = initial_version(options_.xschem_compat);
    return frame;
  }

  Frame& frame() { return frames_.back(); }

  void read_record() {
    const char tag = lex_.take();
    const std::size_t depth = frames_.size();
    const bool after_instance = std::exchange(frame().last_was_instance, false);
    dispatch(tag, after_instance);
    // The record belongs to the frame it started in: a `[` opened a frame
    // above it, a `]` closed the frame it ended.
    end_record(frames_[std::min(depth, frames_.size()) - 1]);
    lex_.skip_to_line_end();
  }

  static void end_record(Frame& frame) {
    if (frame.first_record) {
      frame.first_record = false;
      if (!frame.version) {
        frame.version = "1.0";
      }
    }
  }

  void dispatch(char tag, bool after_instance) {
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
        open_block(after_instance);
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

  void read_version() {
    const std::string v = brace();
    auto version = get_token(v, "file_version");
    frame().version = (version && !version->empty()) ? std::move(*version) : std::string{"1.0"};
    frame().doc.header_text = header_text(v);
  }

  void read_global(char tag) {
    GlobalProperties& globals = frame().doc.globals;
    std::string value = brace();
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
      order_endpoints(record.p1, record.p2);
      frame().doc.lines.push_back(std::move(record));
    } else {
      order_corners(record.p1, record.p2);
      frame().doc.boxes.push_back(std::move(record));
    }
  }

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
    frame().doc.arcs.push_back(ArcRecord{*layer, {v[0], v[1]}, v[2], v[3], v[4], brace()});
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
    frame().doc.polygons.push_back(std::move(record));
  }

  void read_text() {
    TextRecord record;
    record.text = brace();
    std::array<double, 2> origin{};
    std::array<double, 2> scale{};
    std::optional<int> rotation;
    std::optional<int> flip;
    const bool ok = read_numbers(origin) && (rotation = lex_.read_int()) && (flip = lex_.read_int()) &&
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
    frame().doc.texts.push_back(std::move(record));
  }

  void read_wire() {
    std::array<double, 4> v{};
    if (!read_numbers(v)) {
      missing("WIRE");
      return;
    }
    WireRecord record{{v[0], v[1]}, {v[2], v[3]}, brace()};
    order_endpoints(record.p1, record.p2);
    frame().doc.wires.push_back(std::move(record));
  }

  void read_instance() {
    InstanceRecord record;
    record.symbol = brace();
    std::array<double, 2> origin{};
    std::optional<int> rotation;
    std::optional<int> flip;
    if (!(read_numbers(origin) && (rotation = lex_.read_int()) && (flip = lex_.read_int()))) {
      missing("INSTANCE");
      return;
    }
    record.origin = {origin[0], origin[1]};
    record.rotation = *rotation;
    record.flip = *flip;
    record.props = brace();
    const auto& version = frame().version;
    if (version && *version == "1.0") {
      record.symbol = with_sym_extension(std::move(record.symbol));
    }
    frame().doc.instances.push_back(std::move(record));
    frame().last_was_instance = true;
  }

  void open_block(bool after_instance) {
    Frame block = new_frame(FileKind::symbol);
    if (!after_instance) {
      warn(file_codes::kStrayEmbeddedBlock, "embedded symbol block [ without a preceding instance, skipped");
      frames_.push_back(std::move(block));
      return;
    }
    InstanceRecord& instance = frame().doc.instances.back();
    instance.props = set_token(instance.props, "embed", "true");
    if (references_.insert(instance.symbol).second) {
      auto& embedded = frames_.front().doc.embedded;
      block.slot = embedded.size();
      embedded.push_back(EmbeddedSymbol{instance.symbol, {}});
    }
    frames_.push_back(std::move(block));
  }

  // A `]` ends the innermost block; outside any block it is an unknown record.
  void close_block_or_skip() {
    if (frames_.size() > 1) {
      close_block();
    } else {
      skip_unknown();
    }
  }

  void close_block() {
    Frame block = std::move(frames_.back());
    frames_.pop_back();
    finish(block);
    if (block.slot) {
      frames_.front().doc.embedded[*block.slot].symbol = std::move(block.doc);
    }
  }

  // "Global property records": a `.sym` with no `K` record whose `G` has a
  // non-empty `type` uses `G` as its symbol property.
  static void finish(Frame& frame) {
    Document& doc = frame.doc;
    doc.file_version = frame.version.value_or("1.0");
    if (doc.kind != FileKind::symbol || frame.k_read) {
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
  std::vector<Frame> frames_;
  std::set<std::string> references_;
};

}  // namespace

diag::Outcome<Document> read_document(std::string_view bytes, const ReadOptions& options) {
  return Reader{bytes, options}.run();
}

}  // namespace hefesto::schem
