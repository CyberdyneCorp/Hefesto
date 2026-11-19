// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchical circuit model (see model.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "model.hpp"

#include <algorithm>
#include <limits>

namespace hefesto::extflat::detail {

namespace {

// Magic computes these in int and overflows on hostile transforms; the
// subset saturates instead.
std::int32_t sat32(std::int64_t v) noexcept {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(v, std::numeric_limits<std::int32_t>::min(),
                                                            std::numeric_limits<std::int32_t>::max()));
}

std::int64_t lin(std::int64_t p, std::int64_t q, std::int64_t r, std::int64_t s) noexcept {
  return p * q + r * s;
}

}  // namespace

Transform compose(const Transform& first, const Transform& second) noexcept {
  Transform net;
  net.a = sat32(lin(first.a, second.a, first.d, second.b));
  net.b = sat32(lin(first.b, second.a, first.e, second.b));
  net.c = sat32(lin(first.c, second.a, first.f, second.b) + second.c);
  net.d = sat32(lin(first.a, second.d, first.d, second.e));
  net.e = sat32(lin(first.b, second.d, first.e, second.e));
  net.f = sat32(lin(first.c, second.d, first.f, second.e) + second.f);
  return net;
}

Transform translate_then(std::int64_t x, std::int64_t y, const Transform& t) noexcept {
  Transform out = t;
  // x and y are 32-bit values times a 32-bit separation: clamp before the
  // products so that nothing overflows 64 bits.
  const std::int64_t cx = std::clamp<std::int64_t>(x, std::numeric_limits<std::int32_t>::min(),
                                                   std::numeric_limits<std::int32_t>::max());
  const std::int64_t cy = std::clamp<std::int64_t>(y, std::numeric_limits<std::int32_t>::min(),
                                                   std::numeric_limits<std::int32_t>::max());
  out.c = sat32(lin(cx, t.a, cy, t.b) + t.c);
  out.f = sat32(lin(cx, t.d, cy, t.e) + t.f);
  return out;
}

db::Point transform_unit_rect(const Transform& t, db::Point ll) noexcept {
  const std::int64_t x0 = ll.x;
  const std::int64_t y0 = ll.y;
  const std::int64_t x1 = x0 + 1;
  const std::int64_t y1 = y0 + 1;
  const std::int64_t px0 = lin(x0, t.a, y0, t.b) + t.c;
  const std::int64_t py0 = lin(x0, t.d, y0, t.e) + t.f;
  const std::int64_t px1 = lin(x1, t.a, y1, t.b) + t.c;
  const std::int64_t py1 = lin(x1, t.d, y1, t.e) + t.f;
  return db::Point{sat32(std::min(px0, px1)), sat32(std::min(py0, py1))};
}

std::string ConnName::element(std::int64_t i, std::int64_t j) const {
  std::string out = before;
  out.push_back('[');
  out += std::to_string(i);
  if (nsubs == 2) {
    out.push_back(',');
    out += std::to_string(j);
  }
  out.push_back(']');
  out += after;
  return out;
}

std::uint32_t LayerNames::add(std::string_view name) {
  const auto it = index_.find(std::string{name});
  if (it != index_.end()) {
    return it->second;
  }
  names_.emplace_back(name);
  const auto id = static_cast<std::uint32_t>(names_.size() - 1);
  index_.emplace(std::string{name}, id);
  return id;
}

std::uint32_t DefTable::lookup_or_create(std::string_view name) {
  if (const auto found = lookup(name)) {
    return *found;
  }
  defs_.push_back(std::make_unique<Def>());
  defs_.back()->name = std::string{name};
  const auto id = static_cast<std::uint32_t>(defs_.size() - 1);
  index_.emplace(std::string{name}, id);
  return id;
}

std::optional<std::uint32_t> DefTable::lookup(std::string_view name) const {
  const auto it = index_.find(std::string{name});
  if (it == index_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace hefesto::extflat::detail
