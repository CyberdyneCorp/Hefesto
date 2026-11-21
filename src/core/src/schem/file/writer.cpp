// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.sch`/`.sym` writer (capability schematic-file-format "Canonical
// write order", "Version record and header text", "Writer version string",
// "Global property records", "Embedded symbols"; design D6). The output is
// built from the model only, so an unedited document writes what xschem
// writes for it.
//
// Owner: FORMAT.

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "numbers.hpp"

namespace hefesto::schem {

namespace {

using file_detail::format_g16;
using file_detail::format_int;

// Records of one kind, grouped by layer in ascending order and kept in list
// order within a layer.
template <typename Record>
std::vector<const Record*> by_layer(const std::vector<Record>& records) {
  std::vector<const Record*> sorted;
  sorted.reserve(records.size());
  for (const Record& record : records) {
    sorted.push_back(&record);
  }
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const Record* a, const Record* b) { return a->layer < b->layer; });
  return sorted;
}

class Writer {
 public:
  std::string take() { return std::move(out_); }

  void document(const Document& doc) {
    version(doc.header_text);
    globals(doc);
    for (const LineRecord* line : by_layer(doc.lines)) {
      layered('L', *line);
    }
    for (const BoxRecord* box : by_layer(doc.boxes)) {
      layered('B', *box);
    }
    for (const ArcRecord* arc : by_layer(doc.arcs)) {
      this->arc(*arc);
    }
    for (const PolygonRecord* polygon : by_layer(doc.polygons)) {
      this->polygon(*polygon);
    }
    for (const TextRecord& text : doc.texts) {
      this->text(text);
    }
    for (const WireRecord& wire : doc.wires) {
      this->wire(wire);
    }
    instances(doc);
  }

 private:
  void num(double value) {
    out_ += format_g16(value);
    out_ += ' ';
  }

  void integer(int value) {
    out_ += format_int(value);
    out_ += ' ';
  }

  void brace_line(std::string_view value) {
    out_ += brace_string(value);
    out_ += '\n';
  }

  void tagged(char tag, std::string_view value) {
    out_ += tag;
    out_ += ' ';
    brace_line(value);
  }

  void version(std::string_view header) {
    out_ += "v {xschem version=";
    out_ += kWriterVersion;
    out_ += " file_version=1.3";
    if (!header.empty()) {
      const std::string escaped = brace_string(header);
      out_ += '\n';
      out_.append(escaped, 1, escaped.size() - 2);
    }
    out_ += "}\n";
  }

  void globals(const Document& doc) {
    const GlobalProperties& g = doc.globals;
    const auto type = get_token(g.g, "type");
    if (doc.kind == FileKind::symbol && g.k.empty() && type && !type->empty()) {
      tagged('G', "");
      tagged('K', g.g);
    } else {
      tagged('G', g.g);
      tagged('K', g.k);
    }
    tagged('V', g.v);
    tagged('S', g.s);
    tagged('F', g.f);
    tagged('E', g.e);
  }

  void layered(char tag, const LineRecord& record) {
    out_ += tag;
    out_ += ' ';
    integer(record.layer);
    num(record.p1.x);
    num(record.p1.y);
    num(record.p2.x);
    num(record.p2.y);
    brace_line(record.props);
  }

  void arc(const ArcRecord& record) {
    out_ += "A ";
    integer(record.layer);
    num(record.centre.x);
    num(record.centre.y);
    num(record.radius);
    num(record.start_angle);
    num(record.sweep_angle);
    brace_line(record.props);
  }

  void polygon(const PolygonRecord& record) {
    out_ += "P ";
    integer(record.layer);
    integer(static_cast<int>(record.points.size()));
    for (const Point& point : record.points) {
      num(point.x);
      num(point.y);
    }
    brace_line(record.props);
  }

  void text(const TextRecord& record) {
    out_ += "T ";
    out_ += brace_string(record.text);
    out_ += ' ';
    num(record.origin.x);
    num(record.origin.y);
    integer(record.rotation);
    integer(record.flip);
    num(record.x_scale);
    num(record.y_scale);
    brace_line(record.props);
  }

  void wire(const WireRecord& record) {
    out_ += "N ";
    num(record.p1.x);
    num(record.p1.y);
    num(record.p2.x);
    num(record.p2.y);
    brace_line(record.props);
  }

  void instance(const InstanceRecord& record) {
    out_ += "C ";
    out_ += brace_string(record.symbol);
    out_ += ' ';
    num(record.origin.x);
    num(record.origin.y);
    integer(record.rotation);
    integer(record.flip);
    brace_line(record.props);
  }

  // Each instance, then the block of its symbol when this is the first
  // instance of that symbol with `embed=true` and the document holds it.
  void instances(const Document& doc) {
    std::map<std::string_view, const Document*> definitions;
    for (const EmbeddedSymbol& embedded : doc.embedded) {
      definitions.emplace(embedded.reference, &embedded.symbol);
    }
    std::set<std::string_view> written;
    for (const InstanceRecord& record : doc.instances) {
      instance(record);
      const auto embed = get_token(record.props, "embed");
      if (!embed || !is_true(*embed)) {
        continue;
      }
      const auto found = definitions.find(record.symbol);
      if (found != definitions.end() && written.insert(record.symbol).second) {
        embedded_block(*found->second);
      }
    }
  }

  // The block as xschem writes it in both modes (Q-SCHEM-2): no header
  // text, no `F`, `G`/`V`/`S`/`E` empty, wires as `L 1`, polygons after
  // texts, no wires or instances.
  void embedded_block(const Document& symbol) {
    out_ += "[\n";
    version("");
    tagged('K', symbol_property(symbol));
    tagged('G', "");
    tagged('V', "");
    tagged('S', "");
    tagged('E', "");
    std::vector<LineRecord> lines = symbol.lines;
    for (const WireRecord& wire : symbol.wires) {
      lines.push_back(LineRecord{1, wire.p1, wire.p2, wire.props});
    }
    for (const LineRecord* line : by_layer(lines)) {
      layered('L', *line);
    }
    for (const BoxRecord* box : by_layer(symbol.boxes)) {
      layered('B', *box);
    }
    for (const ArcRecord* arc : by_layer(symbol.arcs)) {
      this->arc(*arc);
    }
    for (const TextRecord& text : symbol.texts) {
      this->text(text);
    }
    for (const PolygonRecord* polygon : by_layer(symbol.polygons)) {
      this->polygon(*polygon);
    }
    out_ += "]\n";
  }

  // The symbol property: `K`, or a pre-1.2 symbol's `G` that has a `type`.
  static std::string_view symbol_property(const Document& symbol) {
    const auto type = get_token(symbol.globals.g, "type");
    if (symbol.globals.k.empty() && type && !type->empty()) {
      return symbol.globals.g;
    }
    return symbol.globals.k;
  }

  std::string out_;
};

}  // namespace

std::string brace_string(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out += '{';
  for (const char c : value) {
    if (c == '\\' || c == '{' || c == '}') {
      out += '\\';
    }
    out += c;
  }
  out += '}';
  return out;
}

std::string write_document(const Document& document) {
  Writer writer;
  writer.document(document);
  return writer.take();
}

}  // namespace hefesto::schem
