// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS cells in GDS output (see vendor_oasis.hpp; gds-write
// "Read-only OASIS vendor cells", Q-OASIS-35 (a)). The cell named
// OASIS_CELL is decoded from the file named by OASIS_FILE (found as
// GDS_FILE is) and each element instance is written as its GDS element:
// RECTANGLE, POLYGON, TRAPEZOID and CTRAPEZOID as BOUNDARYs of their
// outline, CIRCLE as the `oasis circlepoints`-gon of oasis-read, PATH as
// PATH, TEXT as TEXT, PLACEMENT as SREF, or as one AREF when oasis-read
// reads its repetition as one Magic array. The structure is built in
// memory first, so a cell that fails writes nothing.
//
// Expanding repetitions is bounded by kMaxInstances element instances per
// cell (the OASIS read cap of design D21; proposed for this path).
// Owner: OASW (design D2, D17).

#include "vendor_oasis.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/read/circle.hpp"
#include "hefesto/oasis/write/vendor_cells.hpp"

namespace hefesto::gds::detail {

namespace {

using oasis::Element;
using oasis::ElementKind;
using oasis::Point64;

constexpr std::uint64_t kMaxGdsNumber = 32767;
constexpr std::size_t kMaxXyPoints = 8191;
constexpr std::uint64_t kMaxInstances = std::uint64_t{1} << 28;

const std::string* string_property(const db::CellDef& def, std::string_view name) {
  const db::PropertyValue* value = def.properties().find(std::string{name});
  return value != nullptr ? std::get_if<std::string>(value) : nullptr;
}

Point64 plus(Point64 a, Point64 b) {
  return {a.x + b.x, a.y + b.y};
}

std::vector<OutputPoint> output_points(Point64 origin, std::span<const Point64> relative, bool close) {
  std::vector<OutputPoint> out;
  out.reserve(relative.size() + 1);
  for (const Point64& p : relative) {
    out.push_back(OutputPoint{origin.x + p.x, origin.y + p.y});
  }
  if (close && !out.empty()) {
    out.push_back(out.front());
  }
  return out;
}

// The quarter-turn inverse transform T^-1 of a placement (flip, then
// rotation rounded to a quarter turn; oasis-read "Placement transforms").
Point64 inverse(const Element& e, Point64 p) {
  const double turns = std::nearbyint(e.angle / 90.0);
  const auto q = std::isfinite(turns) ? ((static_cast<std::int64_t>(std::fmod(turns, 4.0)) % 4) + 4) % 4 : 0;
  constexpr std::array<std::array<std::int64_t, 4>, 4> kUnrotate = {{{1, 0, 0, 1}, {0, 1, -1, 0}, {-1, 0, 0, -1},
                                                                      {0, -1, 1, 0}}};
  const std::array<std::int64_t, 4>& m = kUnrotate[static_cast<std::size_t>(q)];
  const Point64 r{m[0] * p.x + m[1] * p.y, m[2] * p.x + m[3] * p.y};
  return e.flip ? Point64{r.x, -r.y} : r;
}

// oasis-read "Placement repetitions" rules 1 and 2.
bool reads_as_array(const Element& e) {
  const oasis::Repetition& r = *e.repetition;
  const bool grid = r.type <= 3 || r.type == 8 || r.type == 9;
  if (!grid || r.columns > kMaxGdsNumber || r.rows > kMaxGdsNumber) {
    return false;
  }
  const Point64 u = inverse(e, r.column_step);
  const Point64 v = inverse(e, r.row_step);
  const bool rule1 = (r.columns == 1 || u.y == 0) && (r.rows == 1 || v.x == 0);
  const bool rule2 = (r.columns == 1 || u.x == 0) && (r.rows == 1 || v.y == 0);
  return rule1 || rule2;
}

/// The transcoding of one cell into a structure built in memory.
class CellTranscoder {
 public:
  CellTranscoder(VendorIo& io, const db::CellDef& def, const oasis::VendorLibrary& library, ElementWriter& out)
      : io_(io), def_(def), library_(library), out_(out) {
    const editor::EditorState* editor = io.prepared.editor;
    circle_points_ = std::clamp(editor != nullptr ? editor->mask.oasis.circlepoints : 64, oasis::kMinCirclePoints,
                                oasis::kMaxCirclePoints);
    if (const std::string* file = string_property(def, "OASIS_FILE"); file != nullptr && editor != nullptr) {
      editor->cells.for_each([this, file](db::CellDef& cell) {
        const std::string* other = string_property(cell, "OASIS_FILE");
        const std::string* name = string_property(cell, "OASIS_CELL");
        if (other != nullptr && name != nullptr && *other == *file) {
          children_.emplace(*name, &cell);
        }
      });
    }
  }

  diag::Status body(const std::vector<oasis::Record>& records) {
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (!records[i].element) {
        continue;
      }
      std::size_t end = i + 1;
      while (end < records.size() && !records[end].element) {
        ++end;
      }
      if (diag::Status s = element(*records[i].element, std::span{records}.subspan(i + 1, end - i - 1)); !s.ok()) {
        return s;
      }
    }
    return diag::Done{};
  }

 private:
  diag::Diagnostic cell_error(std::string what) const {
    return diag::make(codes::kOasisElement,
                      "Calma output error:  cell " + def_.name() + " " + std::move(what) + "; the cell cannot be written.");
  }

  diag::Status check_layers(const Element& e) const {
    if (e.layer > kMaxGdsNumber || e.datatype > kMaxGdsNumber) {
      return cell_error("has an element on layer " + std::to_string(e.layer) + "/" + std::to_string(e.datatype) +
                        ", which GDS cannot hold (above 32767)");
    }
    return diag::Done{};
  }

  diag::Status count(std::uint64_t instances) {
    instances_ += instances;
    if (instances > kMaxInstances || instances_ > kMaxInstances) {
      return diag::make(codes::kOasisCap, "Calma output error:  cell " + def_.name() + " expands to more than " +
                                              std::to_string(kMaxInstances) +
                                              " element instances; the cell cannot be written.");
    }
    return diag::Done{};
  }

  diag::Status element(const Element& e, std::span<const oasis::Record> properties) {
    if (e.kind == ElementKind::xelement || e.kind == ElementKind::xgeometry) {
      warn_extensions();
      return diag::Done{};
    }
    if (e.kind == ElementKind::placement) {
      return placement(e, properties);
    }
    if (diag::Status s = check_layers(e); !s.ok()) {
      return s;
    }
    const oasis::Repetition none;
    const oasis::Repetition& r = e.repetition ? *e.repetition : none;
    const std::uint64_t n = oasis::instance_count(r);
    if (diag::Status s = count(n); !s.ok()) {
      return s;
    }
    for (std::uint64_t i = 0; i < n; ++i) {
      if (diag::Status s = instance(e, plus(e.position, oasis::offset_at(r, i))); !s.ok()) {
        return s;
      }
    }
    return diag::Done{};
  }

  diag::Status instance(const Element& e, Point64 at) {
    switch (e.kind) {
      case ElementKind::text:
        return text(e, at);
      case ElementKind::path:
        return path(e, at);
      case ElementKind::circle:
        return e.radius == 0 ? diag::Status{diag::Done{}}
                             : boundary(e, at, oasis::circle_vertices(e.radius, circle_points_));
      default:
        return boundary(e, at, oasis::outline(e));
    }
  }

  diag::Status boundary(const Element& e, Point64 at, const std::vector<Point64>& outline) {
    if (outline.empty()) {
      return diag::Done{};  // dropped by the decoder's rules
    }
    if (outline.size() + 1 > kMaxXyPoints) {
      return cell_error("has a polygon of " + std::to_string(outline.size() + 1) +
                        " XY points with the closing point, above GDS's 8191");
    }
    const mask::GdsLayer layer{static_cast<std::int32_t>(e.layer), static_cast<std::int32_t>(e.datatype)};
    return out_.boundary(layer, output_points(at, outline, true));
  }

  diag::Status path(const Element& e, Point64 at) {
    std::vector<Point64> centre{Point64{}};
    centre.insert(centre.end(), e.points.begin(), e.points.end());
    if (centre.size() > kMaxXyPoints) {
      return cell_error("has a path of " + std::to_string(centre.size()) + " points, above GDS's 8191");
    }
    const std::int64_t hw = e.half_width;
    const bool flush = e.start_extension == 0 && e.end_extension == 0;
    const bool half = hw > 0 && e.start_extension == hw && e.end_extension == hw;
    const std::int32_t type = flush ? 0 : (half ? 2 : 4);
    const mask::GdsLayer layer{static_cast<std::int32_t>(e.layer), static_cast<std::int32_t>(e.datatype)};
    diag::Outcome<std::int32_t> width = out_.checked_i4(2 * hw);
    diag::Outcome<std::int32_t> begin = out_.checked_i4(e.start_extension);
    diag::Outcome<std::int32_t> end = out_.checked_i4(e.end_extension);
    for (const auto* v : {&width, &begin, &end}) {
      if (!v->ok()) {
        return v->error();
      }
    }
    const std::vector<OutputPoint> xy = output_points(at, centre, false);
    for (const OutputPoint& p : xy) {
      if (diag::Outcome<std::int32_t> x = out_.checked_i4(p.x); !x.ok()) {
        return x.error();
      }
      if (diag::Outcome<std::int32_t> y = out_.checked_i4(p.y); !y.ok()) {
        return y.error();
      }
    }
    RecordWriter& records = out_.records();
    diag::Status s = out_.empty(RecordType::path);
    s = s.ok() ? out_.layer_records(RecordType::datatype, layer) : s;
    s = s.ok() ? out_.int2(RecordType::pathtype, type) : s;
    s = s.ok() ? records.write_int4(RecordType::width, std::array<std::int32_t, 1>{width.value()}) : s;
    if (type == 4) {
      s = s.ok() ? records.write_int4(RecordType::bgnextn, std::array<std::int32_t, 1>{begin.value()}) : s;
      s = s.ok() ? records.write_int4(RecordType::endextn, std::array<std::int32_t, 1>{end.value()}) : s;
    }
    s = s.ok() ? out_.xy(xy) : s;
    return s.ok() ? out_.empty(RecordType::endel) : s;
  }

  diag::Status text(const Element& e, Point64 at) {
    const std::string string = oasis::resolved_name(library_.names.textstrings, e);
    const std::array<OutputPoint, 1> xy{OutputPoint{at.x, at.y}};
    const mask::GdsLayer layer{static_cast<std::int32_t>(e.layer), static_cast<std::int32_t>(e.datatype)};
    if (diag::Outcome<std::int32_t> x = out_.checked_i4(at.x); !x.ok()) {
      return x.error();
    }
    if (diag::Outcome<std::int32_t> y = out_.checked_i4(at.y); !y.ok()) {
      return y.error();
    }
    diag::Status s = out_.empty(RecordType::text);
    s = s.ok() ? out_.layer_records(RecordType::texttype, layer) : s;
    s = s.ok() ? out_.xy(xy) : s;
    s = s.ok() ? out_.string_record(RecordType::string, string) : s;
    return s.ok() ? out_.empty(RecordType::endel) : s;
  }

  // The SNAME of the referenced cell: the output name of the database
  // cell that holds it (same OASIS_FILE, OASIS_CELL = the name), else the
  // name itself.
  diag::Status sname(const Element& e) {
    const std::string name = oasis::resolved_name(library_.names.cellnames, e);
    const auto child = children_.find(name);
    if (child != children_.end()) {
      return io_.names.write(out_, RecordType::sname, *child->second, io_.id_of(*child->second));
    }
    return out_.string_record(RecordType::sname, name);
  }

  diag::Status transform(const Element& e) {
    diag::Status s = diag::Done{};
    if (e.flip || e.magnification != 1.0 || e.angle != 0.0) {
      s = out_.bits(RecordType::strans, e.flip ? 0x8000 : 0);
    }
    if (s.ok() && e.magnification != 1.0) {
      s = out_.real8(RecordType::mag, e.magnification);
    }
    if (s.ok() && e.angle != 0.0) {
      s = out_.real8(RecordType::angle, e.angle);
    }
    return s;
  }

  diag::Status reference_properties(std::span<const oasis::Record> properties) {
    for (const oasis::Record& r : properties) {
      const oasis::Property p = oasis::resolved_property(*r.property, library_.names);
      if (p.name != "S_GDS_PROPERTY" || p.values.size() != 2) {
        continue;
      }
      const oasis::PropertyValue& a = p.values[0];
      const auto attribute = static_cast<std::int32_t>(a.kind == oasis::PropertyValue::Kind::signed_integer
                                                          ? a.signed_value
                                                          : static_cast<std::int64_t>(a.unsigned_value));
      diag::Status s = out_.int2(RecordType::propattr, std::min<std::int32_t>(attribute, 32767));
      s = s.ok() ? out_.string_record(RecordType::propvalue, p.values[1].string) : s;
      if (!s.ok()) {
        return s;
      }
    }
    return diag::Done{};
  }

  diag::Status reference(const Element& e, Point64 at, std::span<const oasis::Record> properties) {
    const std::array<OutputPoint, 1> xy{OutputPoint{at.x, at.y}};
    diag::Status s = out_.empty(RecordType::sref);
    s = s.ok() ? sname(e) : s;
    s = s.ok() ? transform(e) : s;
    s = s.ok() ? out_.xy(xy) : s;
    s = s.ok() ? reference_properties(properties) : s;
    return s.ok() ? out_.empty(RecordType::endel) : s;
  }

  diag::Status array(const Element& e, std::span<const oasis::Record> properties) {
    const oasis::Repetition& r = *e.repetition;
    const auto na = static_cast<std::int64_t>(r.columns);
    const auto nb = static_cast<std::int64_t>(r.rows);
    const std::array<OutputPoint, 3> xy{
        OutputPoint{e.position.x, e.position.y},
        OutputPoint{e.position.x + na * r.column_step.x, e.position.y + na * r.column_step.y},
        OutputPoint{e.position.x + nb * r.row_step.x, e.position.y + nb * r.row_step.y}};
    for (const OutputPoint& p : xy) {
      if (!out_.checked_i4(p.x).ok() || !out_.checked_i4(p.y).ok()) {
        return out_.xy(xy);  // the GDS-WRITE-004 error
      }
    }
    diag::Status s = out_.empty(RecordType::aref);
    s = s.ok() ? sname(e) : s;
    s = s.ok() ? transform(e) : s;
    const std::array<std::int16_t, 2> colrow{static_cast<std::int16_t>(na), static_cast<std::int16_t>(nb)};
    s = s.ok() ? out_.records().write_int2(RecordType::colrow, colrow) : s;
    s = s.ok() ? out_.xy(xy) : s;
    s = s.ok() ? reference_properties(properties) : s;
    return s.ok() ? out_.empty(RecordType::endel) : s;
  }

  diag::Status placement(const Element& e, std::span<const oasis::Record> properties) {
    if (e.repetition && reads_as_array(e)) {
      return array(e, properties);
    }
    const oasis::Repetition none;
    const oasis::Repetition& r = e.repetition ? *e.repetition : none;
    const std::uint64_t n = oasis::instance_count(r);
    if (diag::Status s = count(n); !s.ok()) {
      return s;
    }
    for (std::uint64_t i = 0; i < n; ++i) {
      if (diag::Status s = reference(e, plus(e.position, oasis::offset_at(r, i)), properties); !s.ok()) {
        return s;
      }
    }
    return diag::Done{};
  }

  void warn_extensions() {
    if (!extensions_warned_) {
      extensions_warned_ = true;
      io_.report_native(codes::kOasisExtensions, "Warning: cell " + def_.name() +
                                                     " has XELEMENT or XGEOMETRY records, which GDS cannot hold; "
                                                     "they are not written.");
    }
  }

  VendorIo& io_;
  const db::CellDef& def_;
  const oasis::VendorLibrary& library_;
  ElementWriter& out_;
  std::int32_t circle_points_ = 64;
  std::map<std::string, const db::CellDef*> children_;
  std::uint64_t instances_ = 0;
  bool extensions_warned_ = false;
};

// The structure of `def` from `library` into `io`, built in memory first.
OasisVendorResult write_structure(VendorIo& io, const db::CellDef& def, const oasis::VendorLibrary& library,
                                  const std::vector<oasis::Record>* records) {
  MemorySink buffer;
  RecordWriter memory(buffer);
  ElementWriter out(memory, io.context(def), &io.sink);
  const GdsSettings& settings = io.prepared.options.settings;
  const std::array<std::int16_t, 12> dates = structure_dates(settings.datestamp.value_or(def.timestamp()), io.now,
                                                             io.prepared.compat, io.prepared.options.time_zone);
  diag::Status s = memory.write_int2(RecordType::bgnstr, dates);
  s = s.ok() ? io.names.write(out, RecordType::strname, def, io.id_of(def)) : s;
  if (s.ok() && records != nullptr) {
    CellTranscoder transcoder(io, def, library, out);
    s = transcoder.body(*records);
  }
  s = s.ok() ? memory.write_empty(RecordType::endstr) : s;
  s = s.ok() ? memory.flush() : s;
  if (!s.ok()) {
    io.sink.emit(s.error());
    return OasisVendorResult::failed;
  }
  diag::Status copied = io.records.flush();
  copied = copied.ok() ? io.raw.write(buffer.bytes()) : copied;
  if (!copied.ok()) {
    io.error = copied;
    return OasisVendorResult::failed;
  }
  return OasisVendorResult::written;
}

// The rules of an unopenable vendor file (gds-write "Verbatim copy of
// read-only cells" rule 2, with OASIS_FILE).
OasisVendorResult missing(VendorIo& io, const db::CellDef& def, const std::string& file) {
  const std::vector<db::CellUse*>& parents = def.instances();
  const db::CellDef* parent = !parents.empty() && parents.front()->parent != nullptr ? parents.front()->parent : &def;
  if (parent != &def && (parent->properties().find("GDS_FILE") != nullptr || oasis_vendor_cell(*parent))) {
    return OasisVendorResult::skipped;  // inside another vendor cell: not written
  }
  io.report(codes::kVendorMissing, "Calma output error:  Can't find OASIS file \"" + file + "\" for vendor cell \"" +
                                       def.name() + "\".  It will not be output.");
  return io.prepared.options.settings.allow_undefined ? OasisVendorResult::skipped : OasisVendorResult::failed;
}

OasisVendorResult placeholder(VendorIo& io, const db::CellDef& def, oasis::VendorCells& cells, ByteSource& source,
                              const std::string& path, const std::string& file) {
  switch (cells.prepare(def, source, path)) {
    case oasis::VendorCells::Check::ready:
      return write_structure(io, def, oasis::VendorLibrary{}, nullptr);
    case oasis::VendorCells::Check::missing:
      return missing(io, def, file);
    default:
      return OasisVendorResult::fallback;
  }
}

double output_unit(const VendorIo& io) {
  return io.prepared.style->has_option(mask::style_option::angstroms) ? 10000.0 : 1000.0;
}

}  // namespace

bool oasis_vendor_cell(const db::CellDef& def) {
  return oasis::is_oasis_vendor_cell(def);
}

OasisVendorResult transcode_oasis_cell(VendorIo& io, const db::CellDef& def) {
  const std::string& file = *string_property(def, "OASIS_FILE");
  const std::string& cell = *string_property(def, "OASIS_CELL");
  io.report(codes::kVendorCopy, "   Copying output for cell " + def.name() + " from " + file);
  std::optional<VendorFile> opened = open_vendor_gds(io, def, file);
  if (!opened) {
    return missing(io, def, file);
  }
  const std::string path = opened->path.generic_string();
  if (oasis::VendorCells* cells = oasis::VendorCells::active()) {
    return placeholder(io, def, *cells, *opened->source, path, file);
  }
  diag::Outcome<oasis::VendorLibrary> library = oasis::decode_vendor_library(*opened->source, {cell});
  if (!library.ok()) {
    io.report(codes::kOasisSource, "Calma output error:  the OASIS file " + path + " of cell " + def.name() +
                                       " cannot be decoded (" + library.error().text +
                                       "); the cell is generated from its layout.");
    return OasisVendorResult::fallback;
  }
  if (library.value().unit != output_unit(io)) {
    io.report(codes::kOasisSource, "Calma output error:  the OASIS file " + path + " of cell " + def.name() +
                                       " does not have the output style's unit; the cell is generated from its "
                                       "layout.");
    return OasisVendorResult::fallback;
  }
  const auto found = library.value().cells.find(cell);
  if (found == library.value().cells.end()) {
    return missing(io, def, file);
  }
  return write_structure(io, def, library.value(), &found->second);
}

}  // namespace hefesto::gds::detail
