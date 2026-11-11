// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flattening (layout-hierarchy "Flattening into a new cell", "Flattening an
// instance in place").
// Ported from Magic VLSI 8.3.694 (fc5daa9e): commands/CmdFI.c (CmdFlatten,
// flatCopyAllLabels) and database/DBcellcopy.c (DBCellCopyAllPaint,
// DBCellCopyAllLabels, DBCellCopyAllCells, DBFlattenInPlace,
// DBCellCopyMaskHints, DBFlatCopyMaskHints; mask hints through
// hints::copy_hints/flat_copy_hints, mask-hints "Copying, flattening and
// transforming hints"). Magic is Copyright
// (C) 1985, 1990 Regents of the University of California (permissive
// license, THIRD_PARTY_NOTICES.md).

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "db/tech_masks.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/hints/hints.hpp"

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

// DBPutFontLabel -> DBFontLabelSetBBox on a label being written.
void update_font_label(const FlattenOptions& options, Label& l) {
  if (options.font_update) {
    options.font_update(l);
  } else {
    l.text_bbox.reset();
  }
}

class Flattener {
 public:
  Flattener(CellDef& dest, const tech::Technology& tech, const FlattenOptions& options)
      : dest_(dest), tech_(tech), options_(options), generated_(dest.labels().size(), false) {}

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
  //
  // DBTreeSrLabels visits a cell's labels, then its uses in subcell
  // enumeration order. flatCopyAllLabels turns the separator before the
  // label text into '.' in the shared path buffer and never restores it, so
  // once a level has copied a label its separator stays '.' for the labels
  // of the cells below it (commands/CmdFI.c:1990-2000).
  void copy_labels(const CellDef& def, const Transform& t, std::string path, bool top) {
    for (const Label& lab : def.labels()) {
      if (!lab.text.empty()) {
        put_flat_label(lab, t, path, top);
        if (!path.empty()) {
          path.back() = '.';
        }
      }
    }
    if (options_.labels == FlattenLabels::top_only) {
      return;
    }
    for (const CellUse* u : uses_in_search_order(def)) {
      if (!descends(*u, options_.descend)) {
        continue;
      }
      for_each_element(*u, [&](std::int32_t x, std::int32_t y) {
        copy_labels(*u->def, compose(t, element_transform(*u, x, y)), path + array_element_name(*u, x, y) + "/",
                    false);
      });
    }
  }

  // Parallel to the destination's labels: true for a label copied from the
  // top level of the flattened source (Magic's LABEL_GENERATE).
  [[nodiscard]] const std::vector<bool>& generated() const noexcept { return generated_; }

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
    // dbCopyAllLabels (top labels) transforms rotation and offset;
    // flatCopyAllLabels copies them unchanged in magic-compat (Q-MODEL-20).
    const bool top_only = options_.labels == FlattenLabels::top_only;
    if (lab.font >= 0 && (top_only || !options_.magic_compat)) {
      out.rotation = transform_angle(t, lab.rotation, options_.magic_compat);
      const Transform delta{t.a, t.b, 0, t.d, t.e, 0};
      out.offset = apply(delta, lab.offset);
    }
    update_font_label(options_, out);
    if (top_only) {
      (void)erase_labels_by_content(dest_.labels(), generated_, out.rect, out.text, 0);
    }
    generated_.push_back(top);
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
  std::vector<bool> generated_;
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
Label prefixed_copy(const Label& label, const std::string& prefix, const FlattenOptions& options) {
  Label fresh = label;
  fresh.text = prefix + label.text;
  update_font_label(options, fresh);
  fresh.sticky = false;
  fresh.port_directions = 0;
  fresh.port_class = PortClass::default_class;
  fresh.port_use = PortUse::default_use;
  fresh.port_shape = PortShape::default_shape;
  fresh.port_index = 0;
  return fresh;
}

// The relabeling loop of DBFlattenInPlace: every generated label gets a
// prefixed non-port copy, then every label with its rectangle and text is
// erased (DBEraseLabelsByContent with type -1).
void prefix_generated_labels(CellDef& dest, std::vector<bool> generated, const std::string& prefix,
                             const FlattenOptions& options) {
  auto& labels = dest.labels();
  std::size_t i = 0;
  while (i < labels.size()) {
    if (!generated[i]) {
      ++i;
      continue;
    }
    const Rect rect = labels[i].rect;
    const std::string text = labels[i].text;
    labels.push_back(prefixed_copy(labels[i], prefix, options));
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
  // Magic's paint, hint, label and cell copies all apply DBDescendSubcell to
  // the edit cell's own use first, so a source that fails a restricting mode
  // copies nothing; only the -dotoplabels copy passes (CU_DESCEND_SPECIAL).
  // Replicated under magic-compat; with it off the root is always copied
  // (Q-MODEL-33, compat).
  const bool copy_root = !options.magic_compat || descends(source, options.descend);
  Flattener f(dest, tech, options);
  if (copy_root) {
    if (auto s = f.copy_paint_and_cells(source, kIdentity); !s.ok()) {
      return s.error();
    }
    // DBFlatCopyMaskHints over the source's bounding box: the hints of the
    // descendants (not the source's own), clipped to it.
    hints::flat_copy_hints(source, source.bbox(), kIdentity, dest, options.descend, options.magic_compat);
  }
  if (options.labels == FlattenLabels::top_only || (copy_root && options.labels != FlattenLabels::none)) {
    f.copy_labels(source, kIdentity, std::string{}, true);
  }
  recompute_bbox(dest, options.magic_compat);
  return &dest;
}

namespace {

// The hints of every level below `def` that the flattening descends into
// (each array element, whole, mapped by its full transform `t`); the kept
// instances keep their hints in their own cells. Magic's DBFlattenInPlace
// copies only the flattened instance's own hints and loses these
// (Q-MISC-14, fix in both modes, X-2 tier 1).
void copy_descended_hints(const CellDef& def, const Transform& t, CellDef& dest, Descend mode) {
  for (const auto& u : def.uses()) {
    if (!descends(*u, mode)) {
      continue;
    }
    for_each_element(*u, [&](std::int32_t x, std::int32_t y) {
      const Transform te = compose(t, element_transform(*u, x, y));
      hints::copy_hints(*u->def, dest, te, std::nullopt);
      copy_descended_hints(*u->def, te, dest, mode);
    });
  }
}

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
  prefix_generated_labels(dest, f.generated(), in_place_prefix(use, x, y, options.magic_compat), options);
  // DBCellCopyMaskHints: the element's own hints, whole, transformed; then
  // those of its flattened descendants (Q-MISC-14).
  hints::copy_hints(*use.def, dest, t, std::nullopt);
  copy_descended_hints(*use.def, t, dest, options.descend);
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
