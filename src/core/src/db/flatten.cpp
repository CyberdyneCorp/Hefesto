// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flattening (layout-hierarchy "Flattening into a new cell", "Flattening an
// instance in place").
// Ported from Magic VLSI 8.3.694 (fc5daa9e): commands/CmdFI.c (CmdFlatten,
// flatCopyAllLabels) and database/DBcellcopy.c (DBCellCopyAllPaint,
// DBCellCopyAllLabels, DBCellCopyAllCells, DBFlattenInPlace). Mask hints
// are not copied yet (capability mask-hints is later). Magic is Copyright
// (C) 1985, 1990 Regents of the University of California (permissive
// license, THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "db/tech_masks.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"

namespace hefesto::db {

namespace {

Rect paint_extent(const CellDef& def) {
  Rect r;
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    r = include(r, def.plane(static_cast<tech::PlaneId>(p)).bounding_box());
  }
  return r;
}

void for_each_element(const CellUse& use, const std::function<void(std::int32_t, std::int32_t)>& f) {
  const ArrayInfo& a = use.array;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      f(x, y);
    }
  }
}

class Flattener {
 public:
  Flattener(CellDef& dest, const tech::Technology& tech, const FlattenOptions& options)
      : dest_(dest), tech_(tech), options_(options) {}

  // DBCellCopyAllPaint + DBCellCopyAllCells over `def` and its descended
  // subtree, `t` mapping `def` into the destination.
  diag::Status copy_paint_and_cells(const CellDef& def, const Transform& t) {
    const Rect extent = paint_extent(def);
    if (!extent.is_null()) {
      if (auto s = copy_paint(def, extent, t, dest_, tech_, all_but_space_and_drc()); !s.ok()) {
        return s;
      }
    }
    for (const auto& u : def.uses()) {
      if (!descends(*u, options_.descend)) {
        if (auto s = copy_use(*u, t); !s.ok()) {
          return s;
        }
        continue;
      }
      diag::Status status = diag::Done{};
      for_each_element(*u, [&](std::int32_t x, std::int32_t y) {
        if (status.ok()) {
          status = copy_paint_and_cells(*u->def, compose(t, element_transform(*u, x, y)));
        }
      });
      if (!status.ok()) {
        return status;
      }
    }
    return diag::Done{};
  }

  // flatCopyAllLabels over `def` and its descended subtree. `path` is the
  // hierarchical prefix ("" at the top level of the flattened source).
  void copy_labels(const CellDef& def, const Transform& t, const std::string& path, bool top) {
    for (const Label& lab : def.labels()) {
      if (!lab.text.empty()) {
        put_flat_label(lab, t, path, top);
      }
    }
    if (options_.labels == FlattenLabels::top_only) {
      return;
    }
    for (const auto& u : def.uses()) {
      if (!descends(*u, options_.descend)) {
        continue;
      }
      for_each_element(*u, [&](std::int32_t x, std::int32_t y) {
        copy_labels(*u->def, compose(t, element_transform(*u, x, y)), path + array_element_name(*u, x, y) + "/",
                    false);
      });
    }
  }

  // Labels added for the top level of the flattened source, in order.
  [[nodiscard]] const std::vector<std::size_t>& top_label_indices() const noexcept { return top_indices_; }

 private:
  void put_flat_label(const Label& lab, const Transform& t, const std::string& path, bool top) {
    Label out = lab;
    out.rect = apply(t, lab.rect);
    out.justification = transform_position(t, lab.justification);
    out.text_bbox.reset();
    if (!path.empty()) {
      std::string text = path;
      text.back() = '.';  // the last separator becomes '.'
      out.text = text + lab.text;
    }
    if (!top) {
      clear_port_and_flags(out);
    }
    if (!options_.magic_compat && lab.font >= 0) {  // Q-MODEL-20
      out.rotation = transform_angle(t, lab.rotation, false);
      const Transform delta{t.a, t.b, 0, t.d, t.e, 0};
      out.offset = apply(delta, lab.offset);
    }
    if (top) {
      top_indices_.push_back(dest_.labels().size());
    }
    dest_.labels().push_back(std::move(out));
    mark_modified(dest_);
  }

  static void clear_port_and_flags(Label& l) {
    l.sticky = false;
    l.port_directions = 0;
    l.port_class = PortClass::default_class;
    l.port_use = PortUse::default_use;
    l.port_shape = PortShape::default_shape;
    l.port_index = 0;
  }

  diag::Status copy_use(const CellUse& u, const Transform& t) {
    PlaceOptions po;
    po.transform = compose(t, u.transform);
    po.array = u.array;
    po.locked = u.locked;
    if (find_use(dest_, u.id) == nullptr) {
      po.id = u.id;
    }
    auto placed = place(dest_, *u.def, po);
    if (!placed.ok()) {
      return placed.error();
    }
    return diag::Done{};
  }

  CellDef& dest_;
  const tech::Technology& tech_;
  const FlattenOptions& options_;
  std::vector<std::size_t> top_indices_;
};

std::string in_place_prefix(const CellUse& use, std::int32_t x, std::int32_t y, bool magic_compat) {
  const ArrayInfo& a = use.array;
  const bool xs = a.xlo != a.xhi;
  const bool ys = a.ylo != a.yhi;
  if (xs && ys) {
    const auto first = magic_compat ? x : y;  // Q-MODEL-7
    const auto second = magic_compat ? y : x;
    return use.id + "[" + std::to_string(first) + "][" + std::to_string(second) + "]/";
  }
  if (xs) {
    return use.id + "[" + std::to_string(x) + "]/";
  }
  if (ys) {
    return use.id + "[" + std::to_string(y) + "]/";
  }
  return use.id + "/";
}

// A non-port copy of `label` with `prefix` before its text.
Label prefixed_copy(const Label& label, const std::string& prefix) {
  Label fresh = label;
  fresh.text = prefix + label.text;
  fresh.sticky = false;
  fresh.port_directions = 0;
  fresh.port_class = PortClass::default_class;
  fresh.port_use = PortUse::default_use;
  fresh.port_shape = PortShape::default_shape;
  fresh.port_index = 0;
  return fresh;
}

// Erases every label with `rect` and `text` (DBEraseLabelsByContent with type
// -1), keeping `generated` parallel; returns the new position of index `i`.
std::size_t erase_labels_by_content(std::vector<Label>& labels, std::vector<bool>& generated, const Rect& rect,
                                    const std::string& text, std::size_t i) {
  std::size_t w = 0;
  std::size_t new_i = i;
  for (std::size_t r = 0; r < labels.size(); ++r) {
    if (r == i) {
      new_i = w;
    }
    if (labels[r].rect == rect && labels[r].text == text) {
      continue;
    }
    if (w != r) {
      labels[w] = std::move(labels[r]);
      generated[w] = generated[r];
    }
    ++w;
  }
  labels.resize(w);
  generated.resize(w);
  return new_i;
}

// The relabeling loop of DBFlattenInPlace: every generated label gets a
// prefixed non-port copy, then every label with its rectangle and text is
// erased (DBEraseLabelsByContent with type -1).
void prefix_generated_labels(CellDef& dest, std::vector<bool> generated, const std::string& prefix) {
  auto& labels = dest.labels();
  std::size_t i = 0;
  while (i < labels.size()) {
    if (!generated[i]) {
      ++i;
      continue;
    }
    const Rect rect = labels[i].rect;
    const std::string text = labels[i].text;
    labels.push_back(prefixed_copy(labels[i], prefix));
    generated.push_back(false);
    i = erase_labels_by_content(labels, generated, rect, text, i);
  }
}

}  // namespace

diag::Outcome<CellDef*> flatten_to_new(CellLibrary& library, const CellDef& source, std::string_view new_name,
                                       const tech::Technology& tech, const FlattenOptions& options) {
  if (library.find(new_name) != nullptr) {
    return diag::make(codes::kCellExists, "cell \"" + std::string{new_name} + "\" already exists");
  }
  auto created = library.create(new_name, source.plane_count());
  if (!created.ok()) {
    return created.error();
  }
  CellDef& dest = *created.value();
  dest.flags().available = true;
  Flattener f(dest, tech, options);
  if (auto s = f.copy_paint_and_cells(source, kIdentity); !s.ok()) {
    return s.error();
  }
  if (options.labels != FlattenLabels::none) {
    f.copy_labels(source, kIdentity, std::string{}, true);
  }
  recompute_bbox(dest, options.magic_compat);
  return &dest;
}

namespace {

// One array element of DBFlattenInPlace: paint, labels, kept instances and
// the relabeling of the child's top-level labels.
diag::Status flatten_element(CellUse& use, std::int32_t x, std::int32_t y, const tech::Technology& tech,
                             const FlattenOptions& options) {
  CellDef& dest = *use.parent;
  Flattener f(dest, tech, options);
  const Transform t = element_transform(use, x, y);
  if (auto s = f.copy_paint_and_cells(*use.def, t); !s.ok()) {
    return s;
  }
  if (options.labels != FlattenLabels::none) {
    f.copy_labels(*use.def, t, std::string{}, true);
  }
  std::vector<bool> generated(dest.labels().size(), false);
  for (const std::size_t i : f.top_label_indices()) {
    generated[i] = true;
  }
  prefix_generated_labels(dest, std::move(generated), in_place_prefix(use, x, y, options.magic_compat));
  return diag::Done{};
}

std::int32_t index_step(std::int32_t lo, std::int32_t hi) noexcept {
  return lo < hi ? 1 : (lo > hi ? -1 : 0);
}

// The next element: x varies fastest from xlo toward xhi, then y. Magic
// never advances y for a y-only array (an endless loop); Hefesto advances
// it whenever x is exhausted or does not vary.
void next_element(const ArrayInfo& a, std::int32_t& x, std::int32_t& y) noexcept {
  x += index_step(a.xlo, a.xhi);
  const bool past = (a.xlo < a.xhi && x > a.xhi) || (a.xlo > a.xhi && x < a.xhi);
  if (a.xlo == a.xhi || past) {
    y += index_step(a.ylo, a.yhi);
    x = a.xlo;
  }
}

}  // namespace

diag::Status flatten_in_place(CellUse& use, const tech::Technology& tech, const FlattenOptions& options) {
  if (use.parent == nullptr || use.def == nullptr) {
    return diag::make(codes::kUseNotFound, "instance \"" + use.id + "\" has no parent");
  }
  CellDef& dest = *use.parent;
  const ArrayInfo a = use.array;
  std::int32_t x = a.xlo;
  std::int32_t y = a.ylo;
  while (true) {
    if (auto s = flatten_element(use, x, y, tech, options); !s.ok()) {
      return s;
    }
    if (x == a.xhi && y == a.yhi) {
      break;
    }
    next_element(a, x, y);
  }
  if (auto s = remove_use(use); !s.ok()) {
    return s;
  }
  recompute_bbox(dest, options.magic_compat);
  return diag::Done{};
}

}  // namespace hefesto::db
