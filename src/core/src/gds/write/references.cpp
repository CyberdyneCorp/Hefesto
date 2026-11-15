// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell reference elements and array flattening on output (gds-write "Cell
// reference elements", "Array flattening on output"). Ports Magic 8.3.694
// (fc5daa9e) calma/CalmaWrite.c calmaWriteUseFunc, Copyright (C) 1985,
// 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Positions, pitches and products are 64-bit and
// range-checked (Q-MASK-222); a COLROW count outside I2 fails the element
// the same way (Magic writes its low 16 bits).
// Owner: GDSW.

#include <array>
#include <cstddef>
#include <limits>

#include "codes.hpp"
#include "writer.hpp"

namespace hefesto::gds::detail {

namespace {

constexpr std::uint16_t kStransMirror = 0x8000;  // CALMA_STRANS_UPSIDEDOWN
constexpr std::int32_t kPropUseName = 61;       // CALMA_PROP_USENAME_STD
constexpr std::int32_t kPropArrayLimits = 99;   // CALMA_PROP_ARRAY_LIMITS
// Cancellation is polled once per this many flattened array elements.
constexpr std::int64_t kCancelPoll = 4096;

using Angle = std::array<std::byte, 8>;
constexpr Angle kR90{std::byte{0x42}, std::byte{0x5a}, std::byte{0}, std::byte{0},
                     std::byte{0},    std::byte{0},    std::byte{0}, std::byte{0}};
constexpr Angle kR180{std::byte{0x42}, std::byte{0xb4}, std::byte{0}, std::byte{0},
                      std::byte{0},    std::byte{0},    std::byte{0}, std::byte{0}};
constexpr Angle kR270{std::byte{0x43}, std::byte{0x10}, std::byte{0xe0}, std::byte{0},
                      std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0}};

struct Orientation {
  std::uint16_t strans = 0;
  const Angle* angle = nullptr;
};

// The STRANS flags and ANGLE of a Manhattan transform.
Orientation reference_orientation(const db::Transform& t) {
  Orientation o;
  o.angle = t.a == -1 ? &kR180 : nullptr;
  if (t.a != t.e || (t.a == 0 && t.b == t.d)) {
    o.strans = kStransMirror;
    if (t.a == 0) {
      o.angle = t.b == 1 ? &kR90 : &kR270;
    }
  } else if (t.a == 0) {
    o.angle = t.b == -1 ? &kR90 : &kR270;
  }
  return o;
}

// SREF/AREF, SNAME, STRANS and ANGLE (when any).
diag::Status reference_head(ElementWriter& out, StructureNames& names, RecordType type, const db::CellUse& use,
                            std::int64_t id, const Orientation& o) {
  if (diag::Status s = out.empty(type); !s.ok()) {
    return s;
  }
  if (diag::Status s = names.write(out, RecordType::sname, *use.def, id); !s.ok()) {
    return s;
  }
  if (diag::Status s = out.bits(RecordType::strans, o.strans); !s.ok()) {
    return s;
  }
  if (o.angle == nullptr) {
    return diag::Done{};
  }
  return out.records().write(RecordType::angle, DataType::real8, *o.angle);
}

std::int64_t span_of(std::int32_t lo, std::int32_t hi) {
  const std::int64_t d = std::int64_t{hi} - lo;
  return d < 0 ? -d : d;
}

std::int64_t sat_mul(std::int64_t a, std::int64_t b) {
  return saturating_mul(a, b);
}

std::int64_t sat_add(std::int64_t a, std::int64_t b) {
  return saturating_add(a, b);
}

// a*x + b*y + c, then times `scale`, saturating.
std::int64_t affine(std::int64_t a, std::int64_t x, std::int64_t b, std::int64_t y, std::int64_t c,
                    std::int64_t scale) {
  return sat_mul(sat_add(sat_add(sat_mul(a, x), sat_mul(b, y)), c), scale);
}

// GeoTransPoint, then times `scale`.
OutputPoint transform_point(const db::Transform& t, std::int64_t x, std::int64_t y, std::int64_t scale) {
  return OutputPoint{affine(t.a, x, t.b, y, t.c, scale), affine(t.d, x, t.e, y, t.f, scale)};
}

// One SREF of a flattened array at output point `p`.
diag::Status flattened_element(ElementWriter& out, StructureNames& names, const db::CellUse& use, std::int64_t id,
                               Orientation o, const OutputPoint& p) {
  if (diag::Status s = reference_head(out, names, RecordType::sref, use, id, o); !s.ok()) {
    return s;
  }
  if (diag::Status s = out.xy(std::span<const OutputPoint>{&p, 1}); !s.ok()) {
    return s;
  }
  return out.empty(RecordType::endel);
}

// The SREFs of a flattened array (`gds arrays yes`), x outer, y inner.
diag::Status flattened_array(ElementWriter& out, StructureNames& names, const db::CellUse& use, std::int64_t id,
                             const mask::GenerationContext& ctx) {
  const db::Transform& t = use.transform;
  const Orientation o = reference_orientation(t);
  const std::int64_t xsep = use.array.xsep;
  const std::int64_t ysep = use.array.ysep;
  const std::int64_t topx = span_of(use.array.xlo, use.array.xhi);
  const std::int64_t topy = span_of(use.array.ylo, use.array.yhi);
  std::int64_t written = 0;
  for (std::int64_t x = 0; x <= topx; ++x) {
    for (std::int64_t y = 0; y <= topy; ++y) {
      if (++written % kCancelPoll == 0 && ctx.cancelled()) {
        return diag::make(codes::kCancelled, "GDS write cancelled.");
      }
      // xxlate = c + a*xsep*x + b*ysep*y (Magic's order of terms).
      const OutputPoint p = transform_point(t, sat_mul(xsep, x), sat_mul(ysep, y), out.scale().write);
      if (diag::Status s = flattened_element(out, names, use, id, o, p); !s.ok()) {
        return s;
      }
    }
  }
  return diag::Done{};
}

diag::Status colrow(ElementWriter& out, std::int64_t cols, std::int64_t rows) {
  for (const std::int64_t v : {cols, rows}) {
    if (v > std::numeric_limits<std::int16_t>::max()) {
      return diag::make(codes::kValueRange, "Calma output error:  array dimension " + std::to_string(v) +
                                                " in cell " + out.cell().name() +
                                                " does not fit in a GDS 2-byte integer; the cell cannot be written.");
    }
  }
  const std::array<std::int16_t, 2> values{static_cast<std::int16_t>(cols), static_cast<std::int16_t>(rows)};
  return out.records().write_int2(RecordType::colrow, values);
}

// PROPATTR 61 with the instance name, then PROPATTR 99 with the array
// limits when the array does not start at index 0.
diag::Status reference_properties(ElementWriter& out, const db::CellUse& use) {
  if (diag::Status s = out.int2(RecordType::propattr, kPropUseName); !s.ok()) {
    return s;
  }
  if (diag::Status s = out.string_record(RecordType::propvalue, use.id); !s.ok()) {
    return s;
  }
  if (use.array.xlo == 0 && use.array.ylo == 0) {
    return diag::Done{};
  }
  if (diag::Status s = out.int2(RecordType::propattr, kPropArrayLimits); !s.ok()) {
    return s;
  }
  const std::string limits = std::to_string(use.array.xlo) + "_" + std::to_string(use.array.xhi) + "_" +
                             std::to_string(use.array.ylo) + "_" + std::to_string(use.array.yhi);
  return out.string_record(RecordType::propvalue, limits);
}

// One SREF or AREF for the whole use (`gds arrays no`).
diag::Status single_reference(ElementWriter& out, StructureNames& names, const db::CellUse& use, std::int64_t id) {
  const db::Transform& t = use.transform;
  const std::int64_t cols = span_of(use.array.xlo, use.array.xhi) + 1;
  const std::int64_t rows = span_of(use.array.ylo, use.array.yhi) + 1;
  const bool array = cols > 1 || rows > 1;
  if (diag::Status s = reference_head(out, names, array ? RecordType::aref : RecordType::sref, use, id,
                                      reference_orientation(t));
      !s.ok()) {
    return s;
  }
  if (array) {
    if (diag::Status s = colrow(out, cols, rows); !s.ok()) {
      return s;
    }
  }
  const std::int64_t scale = out.scale().write;
  std::array<OutputPoint, 3> points{OutputPoint{sat_mul(t.c, scale), sat_mul(t.f, scale)}, {}, {}};
  std::size_t count = 1;
  if (array) {
    points[1] = transform_point(t, sat_mul(use.array.xsep, cols), 0, scale);
    points[2] = transform_point(t, 0, sat_mul(use.array.ysep, rows), scale);
    count = 3;
  }
  if (diag::Status s = out.xy(std::span<const OutputPoint>{points.data(), count}); !s.ok()) {
    return s;
  }
  if (diag::Status s = reference_properties(out, use); !s.ok()) {
    return s;
  }
  return out.empty(RecordType::endel);
}

}  // namespace

diag::Status write_reference(ElementWriter& out, StructureNames& names, const db::CellUse& use, std::int64_t id,
                             const mask::GenerationContext& ctx) {
  if (use.def == nullptr) {
    return diag::Done{};
  }
  if (out.settings().arrays) {
    return flattened_array(out, names, use, id, ctx);
  }
  return single_reference(out, names, use, id);
}

}  // namespace hefesto::gds::detail
