// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bins of the cell-use index (gds-write "Structure ordering",
// Q-MASK-204). Ported from Magic VLSI 8.3.694 (fc5daa9e) bplane/bpBins.c
// (bpBinArrayNew, bpBinAdd, bpBinArrayUnbuild, bpListExceedsQ,
// bpBinArraySizeIt, bpBinArrayBuild1, bpBinArrayBuild, bpBinsUpdate),
// bplane/bpEnum.h (bpBinArea) and utils/ihash.c (the chain order of equal
// keys), Copyright (c) 1995-2002 Juniper Networks, Inc. and the Regents of
// the University of California (permissive licenses,
// THIRD_PARTY_NOTICES.md). Magic's int and double arithmetic is kept, so
// bin sizes and populations are Magic's. Owner: INDEX.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

#include "use_index_impl.hpp"

namespace hefesto::db::use_index_detail {

namespace {

int width(const Rect& r) noexcept { return r.x1 - r.x0; }
int height(const Rect& r) noexcept { return r.y1 - r.y0; }

// bpBins.c roundUp: rounds a non-negative `i` up to a multiple of `res`.
int round_up(int i, int res) noexcept {
  int r = i % res;
  if (r > 0) {
    r -= res;
  }
  return i - r;
}

// bpBinArrayNew: the array covering `bbox` with bins of dx x dy.
std::unique_ptr<BinArray> bin_array_new(int dx, int dy, const Rect& bbox) {
  auto ba = std::make_unique<BinArray>();
  const int dim_x = round_up(width(bbox), dx) / dx;
  const int dim_y = round_up(height(bbox), dy) / dy;
  ba->bbox = bbox;
  ba->dx = dx;
  ba->dy = dy;
  ba->dim_x = dim_x;
  ba->num_bins = dim_x * dim_y;
  ba->bins.resize(static_cast<std::size_t>(ba->num_bins) + 1);
  // Pulled back by one on the top and right edges (bpEnumPush's indices).
  --ba->bbox.x1;
  --ba->bbox.y1;
  return ba;
}

// The regular bin of a non-oversized element. Magic indexes without a
// range check; an element outside the array (only possible for the
// oversized bin's on-the-fly sub-array, whose bpBinArea lies above the
// array) is held to the nearest bin instead of writing out of bounds.
int regular_bin(const BinArray& ba, const Rect& r) noexcept {
  const int dim_y = ba.num_bins / ba.dim_x;
  const int xi = std::clamp((r.x0 - ba.bbox.x0) / ba.dx, 0, ba.dim_x - 1);
  const int yi = std::clamp((r.y0 - ba.bbox.y0) / ba.dy, 0, dim_y - 1);
  return xi + yi * ba.dim_x;
}

struct Extent {
  int max_ex = 0;  // the widest element
  int max_ey = 0;  // the tallest element
  int count = 0;
};

Extent extent_of(const Element* elements) noexcept {
  Extent e;
  for (; elements != nullptr; elements = elements->link) {
    e.max_ex = std::max(e.max_ex, width(elements->rect));
    e.max_ey = std::max(e.max_ey, height(elements->rect));
    ++e.count;
  }
  return e;
}

struct Sizing {
  int w = 0;
  int h = 0;
  int dx = 0;
  int dy = 0;
  int max_dx = 0;
  int max_dy = 0;
  int count = 0;
};

// bpBinArraySizeIt, the case where both element dimensions exceed half
// the array: split in two along the axis that leaves fewer oversized
// elements. False when no split makes progress.
bool split_oversized(Sizing& s, const Element* elements) noexcept {
  int x_over = 0;
  int y_over = 0;
  for (const Element* e = elements; e != nullptr; e = e->link) {
    x_over += width(e->rect) >= s.max_dx ? 1 : 0;
    y_over += height(e->rect) >= s.max_dy ? 1 : 0;
  }
  if (x_over < y_over) {
    s.dx = s.max_dx;
    s.dy = s.h + 1;
    return true;
  }
  if (y_over == s.count) {
    return false;
  }
  s.dx = s.w + 1;
  s.dy = s.max_dy;
  return true;
}

// bpBinArraySizeIt: the tentative bin size, fitting every element.
bool tentative_size(Sizing& s, const Element* elements) noexcept {
  const bool x_fits = s.dx <= s.max_dx;
  const bool y_fits = s.dy <= s.max_dy;
  if (x_fits && !y_fits) {
    s.dy = s.h + 1;
  } else if (!x_fits && y_fits) {
    s.dx = s.w + 1;
  } else if (!x_fits && !y_fits) {
    return split_oversized(s, elements);
  }
  return true;
}

// bpBinArraySizeIt, too many bins with neither axis fixed: square bins of
// the target size, unless one axis cannot take it.
void square_bins(Sizing& s, int x_dim, int y_dim, int max_bins) noexcept {
  const double area = s.h * (s.w + 0.0);
  const int d = static_cast<int>(std::max(std::sqrt(area / max_bins), 1.0));
  if (d < s.dx) {
    s.dy = std::min((s.h + 1) / std::max(max_bins / x_dim, 1), s.max_dy);
  } else if (d < s.dy) {
    s.dx = std::min((s.w + 1) / std::max(max_bins / y_dim, 1), s.max_dx);
  } else if (d > s.max_dx) {
    s.dx = s.w + 1;
    s.dy = std::min(std::max((s.h + 1) / max_bins, s.dy), s.max_dy);
  } else if (d > s.max_dy) {
    s.dy = s.h + 1;
    s.dx = std::min(std::max((s.w + 1) / max_bins, s.dx), s.max_dx);
  } else {
    s.dx = d;
    s.dy = d;
  }
}

// bpBinArraySizeIt: grows the bins when there are more than
// count / bpMinAvgBinPop of them (never shrinks them).
void limit_bin_count(Sizing& s) noexcept {
  const int x_dim = round_up(s.w, s.dx) / s.dx;
  const int y_dim = round_up(s.h, s.dy) / s.dy;
  const double num_bins = x_dim * static_cast<double>(y_dim);
  const auto max_bins = static_cast<int>(std::max(s.count / kMinAverageBinPopulation, 1.0));
  if (num_bins <= max_bins) {
    return;
  }
  if (s.dx == s.w + 1) {
    s.dy = std::min((s.h + 1) / std::max(max_bins / x_dim, 1), s.max_dy);
  } else if (s.dy == s.h + 1) {
    s.dx = std::min((s.w + 1) / std::max(max_bins / y_dim, 1), s.max_dx);
  } else {
    square_bins(s, x_dim, y_dim, max_bins);
  }
}

// bpBinArraySizeIt: the bin size of a new array, or nullopt when there are
// too few elements, the area is too small or no binning makes progress.
std::optional<Sizing> size_it(const Rect& bbox, const Element* elements) noexcept {
  const Extent ext = extent_of(elements);
  Sizing s;
  s.h = height(bbox);
  s.w = width(bbox);
  s.count = ext.count;
  if (s.count < kMinBinArrayPopulation || s.h < 2 || s.w < 2) {
    return std::nullopt;
  }
  s.dx = ext.max_ex + 1;
  s.dy = ext.max_ey + 1;
  s.max_dx = (s.w + 1) / 2;  // at least two bins
  s.max_dy = (s.h + 1) / 2;
  if (!tentative_size(s, elements)) {
    return std::nullopt;
  }
  limit_bin_count(s);
  return s;
}

// bpBinArrayBuild1: an array with the given bin size holding `elements`.
std::unique_ptr<BinArray> bin_array_build1(const Rect& bbox, Element* elements, int dx, int dy) {
  auto ba = bin_array_new(dx, dy, bbox);
  while (elements != nullptr) {
    Element* e = elements;
    elements = e->link;
    bin_add(*ba, *e);
  }
  return ba;
}

void set_sub(Bin& bin, std::unique_ptr<BinArray> sub) noexcept {
  if (sub != nullptr) {
    bin.list = nullptr;
    bin.sub = std::move(sub);
  }
}

}  // namespace

void push_front(Element*& head, Element& e) noexcept {
  e.link = head;
  if (head != nullptr) {
    head->linkp = &e.link;
  }
  head = &e;
  e.linkp = &head;
}

void unlink(Element& e) noexcept {
  *e.linkp = e.link;
  if (e.link != nullptr) {
    e.link->linkp = e.linkp;
  }
  e.link = nullptr;
  e.linkp = nullptr;
}

Rect bin_area(const BinArray& ba, int i) noexcept {
  const int xi = i % ba.dim_x;
  const int yi = i / ba.dim_x;
  const Coord x0 = ba.bbox.x0 + ba.dx * xi;
  const Coord y0 = ba.bbox.y0 + ba.dy * yi;
  return Rect{x0, y0, x0 + ba.dx, y0 + ba.dy};
}

void bin_add(BinArray& ba, Element& e) noexcept {
  const bool oversized = width(e.rect) >= ba.dx || height(e.rect) >= ba.dy;
  const int i = oversized ? ba.num_bins : regular_bin(ba, e.rect);
  Bin& bin = ba.bins[static_cast<std::size_t>(i)];
  if (bin.sub != nullptr) {
    bin_add(*bin.sub, e);
  } else {
    push_front(bin.list, e);
  }
}

Element* bin_array_unbuild(BinArray& ba) noexcept {
  Element* elements = nullptr;
  for (Bin& bin : ba.bins) {
    Element* l = bin.sub != nullptr ? bin_array_unbuild(*bin.sub) : bin.list;
    while (l != nullptr) {
      Element* e = l;
      l = e->link;
      e->link = elements;
      elements = e;
    }
    bin.list = nullptr;
    bin.sub.reset();
  }
  return elements;
}

bool list_exceeds(const Element* e, int n) noexcept {
  ++n;
  while (e != nullptr && n != 0) {
    --n;
    e = e->link;
  }
  return n == 0;
}

std::unique_ptr<BinArray> bin_array_build(const Rect& bbox, Element* elements, bool subbin) {
  const std::optional<Sizing> s = size_it(bbox, elements);
  if (!s) {
    return nullptr;
  }
  auto ba = bin_array_build1(bbox, elements, s->dx, s->dy);
  if (!subbin) {
    return ba;
  }
  for (int i = 0; i < ba->num_bins; ++i) {
    Bin& bin = ba->bins[static_cast<std::size_t>(i)];
    set_sub(bin, bin_array_build(bin_area(*ba, i), bin.list, true));
  }
  Bin& oversized = ba->bins.back();
  set_sub(oversized, bin_array_build(bbox, oversized.list, true));
  return ba;
}

namespace {

// Relinks the back pointers of a list built with plain links.
void relink(Element*& head) noexcept {
  Element** linkp = &head;
  for (Element* e = head; e != nullptr; e = e->link) {
    e->linkp = linkp;
    linkp = &e->link;
  }
}

Rect list_bbox(const Element* e) noexcept {
  Rect box = e->rect;
  for (; e != nullptr; e = e->link) {
    box = Rect{std::min(box.x0, e->rect.x0), std::min(box.y0, e->rect.y0), std::max(box.x1, e->rect.x1),
               std::max(box.y1, e->rect.y1)};
  }
  return box;
}

}  // namespace

void bins_update(Plane& plane) {
  if (!list_exceeds(plane.inbox, kMinBinArrayPopulation - 1)) {
    return;
  }
  const bool old_bins = plane.root != nullptr;
  if (old_bins) {
    Element* elist = bin_array_unbuild(*plane.root);
    plane.root.reset();
    while (plane.inbox != nullptr) {
      Element* e = plane.inbox;
      plane.inbox = e->link;
      e->link = elist;
      elist = e;
    }
    plane.inbox = elist;
  }
  Rect bbox = list_bbox(plane.inbox);
  if (old_bins) {  // double the box, to avoid too many rebuilds
    const int dx = width(bbox) / 2;
    const int dy = height(bbox) / 2;
    bbox = Rect{bbox.x0 - dx, bbox.y0 - dy, bbox.x1 + dx, bbox.y1 + dy};
  }
  plane.root = bin_array_build(bbox, plane.inbox, true);
  if (plane.root != nullptr) {
    plane.inbox = nullptr;
  } else {
    relink(plane.inbox);  // Magic leaves the back pointers stale here
  }
  plane.bin_area = bbox;
}

void undo_subbin(const SubbinRecord& record) {
  Bin& bin = record.array->bins[static_cast<std::size_t>(record.bin)];
  bin.sub.reset();
  bin.list = nullptr;
  for (auto it = record.list.rbegin(); it != record.list.rend(); ++it) {
    push_front(bin.list, **it);
  }
}

std::size_t EqualTable::RectHash::operator()(const Rect& r) const noexcept {
  std::size_t h = std::hash<Coord>{}(r.x0);
  for (const Coord c : {r.y0, r.x1, r.y1}) {
    h = h * 1000003U ^ std::hash<Coord>{}(c);
  }
  return h;
}

void EqualTable::add(Element& e) {
  groups_[e.rect].push_back(&e);
  ++entries_;
  if (entries_ / buckets_ >= 3) {  // iHashResize: every chain is re-added at the head
    buckets_ *= 4;
    for (auto& [rect, group] : groups_) {
      std::reverse(group.begin(), group.end());
    }
  }
}

void EqualTable::remove(const Element& e) {
  const auto it = groups_.find(e.rect);
  if (it == groups_.end()) {
    return;
  }
  auto& group = it->second;
  group.erase(std::remove(group.begin(), group.end(), &e), group.end());
  if (group.empty()) {
    groups_.erase(it);
  }
  --entries_;
}

std::vector<Element*> EqualTable::lookup(const Rect& r) const {
  const auto it = groups_.find(r);
  if (it == groups_.end()) {
    return {};
  }
  return {it->second.rbegin(), it->second.rend()};
}

}  // namespace hefesto::db::use_index_detail
