// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SREF and AREF elements (capability gds-read "Structure reference
// transforms", "Instance placement scaling", "Array references", "Instance
// properties", "Use placement rules", "Magnified instances", "Post-order
// reading and file repositioning" (the look-ahead trigger), "Flattening
// small cells on input" (the copies into the parent)). Ported from Magic
// 8.3.694 calma/CalmaRdcl.c (calmaElementSref, calmaMagnifiedCell,
// gdsCopyPaintFunc), calma/CalmaRdio.c (calmaReadTransform) and
// database/DBcell.c (DBCellFindDup); Magic is (c) the Regents of the
// University of California, distributed under its permissive license
// (THIRD_PARTY_NOTICES.md).
//
// Compat decisions: Q-MASK-124 (a row spacing that the row count does not
// divide is truncated ON, rounded OFF), Q-MASK-125 (an unknown instance
// property ends the property list ON, is skipped OFF).
//
// Deviations (X-2 tier 1, malformed input): a non-finite MAG is reported
// and read as 1; magnified copies are nested at most kMaxMagnifyDepth deep
// and only while the read scale stays within kMaxScale (GDS-READ-011, the
// instance is then placed without magnification); one AREF makes at most
// kMaxArrayCopies flatten copies; coordinates are clamped to the usable
// plane.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numeric>

#include "gds/read/reader.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/values.hpp"

namespace hefesto::gds::read_detail {

namespace {

constexpr std::string_view kCapReached = "GDS-READ-011";
constexpr std::int32_t kPropUseName = 61;     // CALMA_PROP_USENAME_STD
constexpr std::int32_t kPropUseNameOld = 98;  // CALMA_PROP_USENAME
constexpr std::int32_t kPropArrayLimits = 99;  // CALMA_PROP_ARRAY_LIMITS

std::uint8_t type_of(RecordType t) {
  return static_cast<std::uint8_t>(t);
}

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
}

bool matches_any(const std::vector<std::string>& patterns, std::string_view name) {
  return std::any_of(patterns.begin(), patterns.end(),
                     [&](const std::string& pattern) { return db::glob_match(pattern, name); });
}

/// DBCellFindDup's comparison of two placements of the same definition.
bool same_placement(const db::CellUse& a, const db::CellUse& b) {
  if (a.def != b.def || !(a.transform == b.transform)) {
    return false;
  }
  const bool not_x = a.array.xhi == a.array.xlo && b.array.xhi == b.array.xlo;
  const bool not_y = a.array.yhi == a.array.ylo && b.array.yhi == b.array.ylo;
  const bool x_match = a.array.xhi - a.array.xlo == b.array.xhi - b.array.xlo && a.array.xsep == b.array.xsep;
  const bool y_match = a.array.yhi - a.array.ylo == b.array.yhi - b.array.ylo && a.array.ysep == b.array.ysep;
  if (not_x && not_y) {
    return true;
  }
  if (!not_x && not_y) {
    return x_match;
  }
  if (not_x) {
    return y_match;
  }
  return x_match && y_match;
}

/// sscanf("%d_%d_%d_%d").
bool parse_limits(const std::string& text, std::array<std::int32_t, 4>& out) {
  int consumed = 0;
  return std::sscanf(text.c_str(), "%d_%d_%d_%d%n", &out[0], &out[1], &out[2], &out[3], &consumed) == 4;
}

/// A plane of a flatten-marked cell: GDSR's saved planes or the engine's
/// copyup planes.
const mask::MaskPlane* source_plane(const PlaneSet* mine, const std::vector<mask::MaskPlane*>* engine,
                                    std::size_t layer) {
  if (mine != nullptr) {
    return layer < mine->size() ? (*mine)[layer].get() : nullptr;
  }
  return engine != nullptr && layer < engine->size() ? (*engine)[layer] : nullptr;
}

void copy_tiles(const mask::MaskPlane& from, mask::MaskPlane& to, const db::Transform& t, bool magic_compat) {
  for (const db::TileView& tile : from.solid_tiles()) {
    const db::Point p0 = db::apply(t, db::Point{tile.area.x0, tile.area.y0});
    const db::Point p1 = db::apply(t, db::Point{tile.area.x1, tile.area.y1});
    const db::Rect area = db::canonical(db::Rect{p0.x, p0.y, p1.x, p1.y});
    if (tile.split) {
      const db::TriangleSide side = db::transform_triangle(t, tile.diagonal, tile.right_side);
      (void)to.paint_triangle(area, side.diagonal, side.right_side, magic_compat);
    } else {
      (void)to.paint(area);
    }
  }
}

}  // namespace

/// An SREF/AREF being read (calmaElementSref's locals).
struct Reference {
  std::string sname;
  db::CellDef* def = nullptr;
  db::Transform transform;
  bool array = false;
  std::int32_t cols = 0;
  std::int32_t rows = 0;
  std::int32_t xlo = 0;
  std::int32_t xhi = 0;
  std::int32_t ylo = 0;
  std::int32_t yhi = 0;
  std::array<std::array<std::int64_t, 2>, 3> points{};    // reference points, internal units
  std::array<std::array<std::int64_t, 2>, 3> unscaled{};  // read units (flatten copies)
  std::string useid;
};

// --- transform ------------------------------------------------------------------

Reader::Strans Reader::read_transform(const std::string& name) {
  Strans out;
  std::optional<Record> rec = next();
  if (!rec) {
    out.ok = false;
    return out;
  }
  if (rec->type != type_of(RecordType::strans)) {
    unread(std::move(*rec));
    return out;
  }
  if (rec->data.size() != 2) {
    out.ok = false;
    return out;
  }
  const auto flags = static_cast<std::uint16_t>(*i2_at(rec->data, 0));
  if (!read_magnification(name, out) || !read_angle(out)) {
    out.ok = false;
    return out;
  }
  const ManhattanAngle angle = manhattan_angle(out.angle);
  if (angle.rounded) {
    error("Non-Manhattan angle (" + std::to_string(angle.truncated) + ") in transform");
    error("    Rounding to " + std::to_string(angle.degrees) + " degrees.");
  }
  out.transform = strans_transform(flags, angle.degrees);
  return out;
}

bool Reader::read_magnification(const std::string& name, Strans& out) {
  std::optional<Record> rec = next();
  if (!rec) {
    return false;
  }
  if (rec->type != type_of(RecordType::mag)) {
    unread(std::move(*rec));
    return true;
  }
  if (rec->data.size() != 8) {
    return false;
  }
  double mag = *r8_at(rec->data, 0);
  if (!(mag > 0.0) || !std::isfinite(mag)) {
    char text[128];
    std::snprintf(text, sizeof text, "Invalid magnification (%g) in instance of ", mag);
    error(text + name + " (ignored)");
    mag = 1.0;
  }
  out.mag = mag;
  return true;
}

bool Reader::read_angle(Strans& out) {
  std::optional<Record> rec = next();
  if (!rec) {
    return false;
  }
  if (rec->type != type_of(RecordType::angle)) {
    unread(std::move(*rec));
    return true;
  }
  if (rec->data.size() != 8) {
    return false;
  }
  out.angle = *r8_at(rec->data, 0);
  return true;
}

// --- magnified copies -------------------------------------------------------------

db::CellDef* Reader::magnified_cell(const std::string& sname, double mag) {
  const MagFraction fraction = mag_fraction(mag);
  if (!fraction.exact) {
    char text[64];
    std::snprintf(text, sizeof text, "Magnification %.15g of cell ", mag);
    error(text + sname + " approximated as " + std::to_string(fraction.num) + "/" + std::to_string(fraction.den));
  }
  std::int64_t num = mag_num_ * fraction.num;
  std::int64_t den = mag_den_ * fraction.den;
  const std::int64_t g = std::gcd(num, den);
  num /= g;
  den /= g;
  if (num == den) {
    return nullptr;
  }
  const std::string name = magnified_name(sname, num, den);
  if (db::CellDef* made = look_cell(name)) {
    return made;
  }
  const ReadScale s = scale_->scale();
  if (mag_depth_ >= kMaxMagnifyDepth || nesting_ >= kMaxNesting || s.num > kMaxScale / fraction.num ||
      s.den > kMaxScale / fraction.den || num > kMaxScale || den > kMaxScale) {
    counted(diag::make(kCapReached, "magnified copy " + name + " not made: nesting or scale limit reached"));
    return nullptr;
  }
  const std::uint64_t resume = records_.position();
  db::CellDef* def = nullptr;
  Position position;
  if (records_.seek(0).ok()) {
    position = set_position(sname);
  }
  // Magic tests FEOF: a search ended by block padding counts as found, and
  // the structure read that follows finds nothing.
  if (position.found || position.padding) {
    char text[64];
    std::snprintf(text, sizeof text, "%g", static_cast<double>(num) / static_cast<double>(den));
    info("Cell " + sname + " is magnified by " + text + ":  reading magnified copy " + name + ".");
    read_magnified(name, num, den, fraction);
    def = look_cell(name);
    if (def != nullptr) {
      db::set_property(*def, "GDS_MAG_SOURCE", db::PropertyValue{sname});
    }
  } else {
    error("Cannot magnify cell " + sname + ", which is not defined in this file;  placing it without magnification.");
  }
  (void)records_.seek(resume);
  return def;
}

void Reader::read_magnified(const std::string& name, std::int64_t num, std::int64_t den, const MagFraction& fraction) {
  db::CellDef* reading = current_;
  PlaneSet saved = save_planes(true);
  saved_sets_.push_back(&saved);
  const auto saved_errors = layer_errors_;
  const std::int64_t saved_num = mag_num_;
  const std::int64_t saved_den = mag_den_;
  mag_num_ = num;
  mag_den_ = den;
  mag_name_ = name;
  const ReadScale s = scale_->scale();
  scale_->set_scale(ReadScale{s.num * fraction.num, s.den * fraction.den});
  ++nesting_;
  ++mag_depth_;
  (void)parse_structure();
  --mag_depth_;
  --nesting_;
  const ReadScale after = scale_->scale();
  scale_->set_scale(ReadScale{after.num / fraction.num, after.den / fraction.den});
  mag_num_ = saved_num;
  mag_den_ = saved_den;
  mag_name_.reset();
  saved_sets_.pop_back();
  current_ = reading;
  restore_planes(saved);
  layer_errors_ = saved_errors;
}

// --- reference points ----------------------------------------------------------------

bool Reader::read_reference_points(Reference& ref) {
  std::optional<Record> xy = next();
  if (!xy) {
    return false;
  }
  if (xy->type != type_of(RecordType::xy)) {
    unexpected(RecordType::xy, xy->type);
    return false;
  }
  std::size_t count = xy->data.size() / 8;
  if (count > 3) {
    error("Too many points (" + std::to_string(count) + ") in SREF/AREF");
    count = 3;
  } else if (count < 1) {
    error("Missing reference points in SREF/AREF (using 0,0)");
  }
  const bool rotated = ref.transform.a == 0;
  for (std::size_t n = 0; n < count; ++n) {
    read_reference_point(ref, *xy, n, rotated);
  }
  ref.unscaled = ref.points;
  scale_reference_points(ref, count);
  return true;
}

void Reader::read_reference_point(Reference& ref, const Record& xy, std::size_t n, bool rotated) {
  const std::int64_t before = scale_->scale().num;
  auto& p = ref.points[n];
  const std::int32_t vx = *i4_at(xy.data, 2 * n);
  const std::int32_t vy = *i4_at(xy.data, 2 * n + 1);
  std::int64_t r = 1;
  const bool only_x = n > 0 && ((!rotated && ref.rows == 1) || (rotated && ref.cols == 1));
  const bool only_y = n > 0 && ((!rotated && ref.cols == 1) || (rotated && ref.rows == 1));
  if (only_x) {
    p[0] = convert(vx, 1, r);
    p[1] = ref.points[0][1];  // copied before the rescale below, as Magic does
  } else if (only_y) {
    p[1] = convert(vy, 1, r);
    p[0] = ref.points[0][0];
  } else {
    p[0] = convert(vx, 1, r);
    std::int64_t r2 = 1;
    p[1] = convert(vy, 1, r2);
    p[0] *= r2;
  }
  const std::int64_t after = scale_->scale().num;
  if (after != before && before > 0) {
    const std::int64_t factor = after / before;
    for (std::size_t i = 0; i < n; ++i) {
      ref.points[i][0] *= factor;
      ref.points[i][1] *= factor;
    }
  }
}

void Reader::scale_reference_points(Reference& ref, std::size_t count) {
  // CIFScaleCoord per coordinate; a grid subdivision multiplies the values
  // already converted (x of this point included when y subdivides).
  for (std::size_t n = 0; n < count; ++n) {
    std::int64_t save = scale_factor();
    ref.points[n][0] = engine_->scale_coord(ref.points[n][0], mask::Snap::exact);
    if (save != scale_factor()) {
      const std::int64_t factor = save / scale_factor();
      for (std::size_t i = 0; i < n; ++i) {
        ref.points[i][0] *= factor;
        ref.points[i][1] *= factor;
      }
      save = scale_factor();
    }
    ref.points[n][1] = engine_->scale_coord(ref.points[n][1], mask::Snap::exact);
    if (save != scale_factor()) {
      const std::int64_t factor = save / scale_factor();
      for (std::size_t i = 0; i < n; ++i) {
        ref.points[i][0] *= factor;
        ref.points[i][1] *= factor;
      }
      ref.points[n][0] *= factor;
    }
  }
}

// --- properties -----------------------------------------------------------------------

bool Reader::read_instance_properties(Reference& ref) {
  for (;;) {
    std::optional<Record> rec = next();
    if (!rec) {
      return false;
    }
    if (rec->type != type_of(RecordType::propattr)) {
      unread(std::move(*rec));
      return true;
    }
    const std::int32_t attr = i2_at(rec->data, 0).value_or(0);
    if (attr == kPropUseName || attr == kPropUseNameOld) {
      std::optional<std::string> id = read_string(RecordType::propvalue);
      if (!id) {
        return false;
      }
      ref.useid = instance_name(std::move(*id));
    } else if (attr == kPropArrayLimits) {
      const std::optional<std::string> limits = read_string(RecordType::propvalue);
      if (!limits) {
        return false;
      }
      array_limits(ref, *limits);
    } else if (!compat_.magic_compat) {
      skip_property_value();  // Q-MASK-125 OFF: only this pair is skipped
    }
  }
}

void Reader::skip_property_value() {
  std::optional<Record> rec = next();
  if (rec && rec->type != type_of(RecordType::propvalue)) {
    unread(std::move(*rec));
  }
}

std::string Reader::instance_name(std::string id) {
  for (char& c : id) {
    if (c != '/' && c != ',') {
      continue;
    }
    if (name_conversions_ < 100) {
      info(std::string{"\""} + c + "\" character cannot be used in instance name; converting to underscore");
    } else if (name_conversions_ == 100) {
      info("More than 100 character changes; not reporting further errors.");
    }
    c = '_';
    ++name_conversions_;
  }
  return id;
}

void Reader::array_limits(Reference& ref, const std::string& text) {
  std::array<std::int32_t, 4> v{};
  if (!parse_limits(text, v)) {
    messages_.print(diag::make("GDS-READ-006", "Malformed \"array\" property ignored: " + text));
    return;
  }
  ref.xlo = v[0];
  ref.xhi = v[1];
  ref.ylo = v[2];
  ref.yhi = v[3];
}

// --- placement ------------------------------------------------------------------------

db::CellUse* Reader::place_use(db::CellDef& parent, db::CellDef& child, const db::Transform& transform,
                               const db::ArrayInfo& array, const std::string& id, bool check_duplicate) {
  if (check_duplicate) {
    db::CellUse probe;
    probe.def = &child;
    probe.transform = transform;
    probe.array = array;
    db::compute_use_bbox(probe);
    bool duplicate = false;
    (void)parent.use_index().find_equal(probe.bbox, [&](db::CellUse& other) {
      duplicate = same_placement(other, probe);
      return !duplicate;
    });
    if (duplicate) {
      error("Warning: cell \"" + child.name() + "\" placed on top of itself.  Ignoring the extra one.");
      return nullptr;
    }
  }
  db::PlaceOptions options;
  options.transform = transform;
  options.array = array;
  auto placed = db::place(parent, child, options);
  if (!placed.ok()) {
    counted(placed.error());
    return nullptr;
  }
  // DBCellNewUse leaves the id as read (or unset) until DBGenerateUniqueIds
  // at the end of the structure, which resolves duplicates in Magic's order.
  db::CellUse* use = placed.value();
  parent.unlink_use_id(use->id);
  use->id = id;
  return use;
}

void Reader::array_spacing(const Reference& ref, const db::Transform& trans, db::ArrayInfo& array) {
  const db::Transform inverse = db::invert(trans);
  const auto local = [&](std::size_t n) {
    const db::Point p{clamp_coord(ref.points[n][0]), clamp_coord(ref.points[n][1])};
    return db::apply(inverse, p);
  };
  array.xsep = 0;
  array.ysep = 0;
  if (ref.cols != 0) {
    const std::int64_t px = local(1).x;
    if (px % ref.cols != 0) {
      const std::int64_t rounded = (px + (ref.cols + 1) / 2) / ref.cols;
      error("# cols doesn't divide displacement ref pt");
      error("    " + std::to_string(px) + " / " + std::to_string(ref.cols) + " -> " + std::to_string(rounded));
      array.xsep = clamp_coord(rounded);
    } else {
      array.xsep = clamp_coord(px / ref.cols);
    }
  }
  if (ref.rows != 0) {
    const std::int64_t py = local(2).y;
    if (py % ref.rows != 0) {
      const std::int64_t rounded = (py + (ref.rows + 1) / 2) / ref.rows;
      error("# rows doesn't divide displacement ref pt");
      error("    " + std::to_string(py) + " / " + std::to_string(ref.rows) + " -> " + std::to_string(rounded));
      array.ysep = clamp_coord(compat_.magic_compat ? py / ref.rows : rounded);  // Q-MASK-124
    } else {
      array.ysep = clamp_coord(py / ref.rows);
    }
  }
}

bool Reader::is_flat(const db::CellDef& def) const {
  return flat_planes_.contains(&def) || engine_->saved_planes(def) != nullptr;
}

void Reader::copy_flattened(const Reference& ref, const db::Transform& trans, const db::ArrayInfo& array) {
  const auto mine = flat_planes_.find(ref.def);
  const PlaneSet* own = mine != flat_planes_.end() ? &mine->second : nullptr;
  const std::vector<mask::MaskPlane*>* copyup = own == nullptr ? engine_->saved_planes(*ref.def) : nullptr;
  const std::int64_t cols = ref.array ? std::max(ref.cols, 0) : 1;
  const std::int64_t rows = ref.array ? std::max(ref.rows, 0) : 1;
  if (cols * rows > kMaxArrayCopies) {
    counted(diag::make(kCapReached, "flattened cell " + ref.def->name() + " not copied into a " +
                                        std::to_string(cols) + " x " + std::to_string(rows) + " array"));
    return;
  }
  state(*ref.def).flattened = true;
  std::vector<mask::MaskPlane*>& target = planes();
  for (std::size_t layer = 0; layer < target.size(); ++layer) {
    const mask::MaskPlane* source = source_plane(own, copyup, layer);
    if (source == nullptr || target[layer] == nullptr) {
      continue;
    }
    for (std::int64_t x = 0; x < cols; ++x) {
      for (std::int64_t y = 0; y < rows; ++y) {
        // GeoTransTranslate: the element offset is added after the transform.
        const db::Transform t = db::compose(
            db::translation(clamp_coord(x * array.xsep), clamp_coord(y * array.ysep)), trans);
        copy_tiles(*source, *target[layer], t, compat_.magic_compat);
      }
    }
  }
}

bool Reader::place_reference(const Reference& ref, bool unscaled, std::int64_t& made) {
  const auto& pts = unscaled ? ref.unscaled : ref.points;
  db::Transform trans = ref.transform;
  trans.c = clamp_coord(pts[0][0]);
  trans.f = clamp_coord(pts[0][1]);
  db::ArrayInfo array;
  if (ref.array) {
    Reference spacing = ref;
    spacing.points = pts;
    array_spacing(spacing, trans, array);
    array.xlo = ref.xlo;
    array.xhi = ref.xhi;
    array.ylo = ref.ylo;
    array.yhi = ref.yhi;
  }
  if (!is_flat(*ref.def)) {
    (void)place_use(*current_, *ref.def, trans, array, ref.useid, true);
    return false;  // no second pass
  }
  if (unscaled) {
    copy_flattened(ref, trans, array);
    return false;
  }
  if (!ref.def->flags().vendor_gds && !has_contents(*ref.def)) {
    made = 1;  // the copy stands for the use
    return true;
  }
  if (place_use(*current_, *ref.def, trans, array, ref.useid, true) != nullptr) {
    made = 1;
  }
  return true;
}

// --- SREF / AREF ---------------------------------------------------------------------

db::CellDef* Reader::reference_cell(const std::string& sname) {
  db::CellDef* def = look_cell(sname);
  const bool look_ahead = settings().ordering || settings().flatten || matches_any(settings().flatglob, sname);
  if (def == nullptr && look_ahead) {
    def = read_ahead(sname);
  }
  if (def == nullptr) {
    def = find_cell(sname, nullptr, nullptr);
  }
  return def;
}

bool Reader::ancestor_refused(const db::CellDef& def) {
  if (current_ == nullptr || !is_ancestor(def, *current_)) {
    return false;
  }
  error("Cell " + def.name() + " is an ancestor of " + current_->name(), true);
  error(" and can't be used as a subcell.");
  error("(Use skipped)");
  return true;
}

bool Reader::read_colrow(Reference& ref) {
  std::optional<Record> rec = next();
  if (!rec) {
    return false;
  }
  if (rec->type != type_of(RecordType::colrow)) {
    unread(std::move(*rec));
    return true;
  }
  const auto cols = i2_at(rec->data, 0);
  const auto rows = i2_at(rec->data, 1);
  if (!cols || !rows) {
    return false;
  }
  ref.array = true;
  ref.cols = *cols;
  ref.rows = *rows;
  ref.xlo = 0;
  ref.xhi = ref.cols - 1;
  ref.ylo = 0;
  ref.yhi = ref.rows - 1;
  return true;
}

std::int64_t Reader::element_sref() {
  skip_set({RecordType::elflags, RecordType::plex});
  Reference ref;
  const std::optional<std::string> sname = read_string(RecordType::sname);
  if (!sname || current_ == nullptr) {
    return -1;
  }
  ref.sname = *sname;
  ref.def = reference_cell(ref.sname);
  if (ref.def == nullptr || ancestor_refused(*ref.def)) {
    return -1;
  }
  const Strans strans = read_transform(ref.sname);
  if (!strans.ok) {
    info("Couldn't read transform.");
    return -1;
  }
  ref.transform = strans.transform;
  if (strans.mag != 1.0 || mag_num_ != mag_den_) {
    if (db::CellDef* magnified = magnified_cell(ref.sname, strans.mag)) {
      if (ancestor_refused(*magnified)) {
        return -1;
      }
      ref.def = magnified;
    }
  }
  if (!read_colrow(ref) || !read_reference_points(ref) || !read_instance_properties(ref)) {
    return -1;
  }
  std::int64_t made = 0;
  if (place_reference(ref, false, made)) {
    (void)place_reference(ref, true, made);
  }
  return is_flat(*ref.def) ? made : 1;
}

}  // namespace hefesto::gds::read_detail
