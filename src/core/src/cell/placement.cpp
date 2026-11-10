// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The placement reference syntax of `getcell` and `dump`.
// Ported from Magic VLSI 8.3.694 (fc5daa9e) commands/CmdCD.c
// (cmdDumpParseArgs, cmdDumpFunc) and database/DBlabel2.c (DBSrLabelLoc).
// Magic is Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include "placement.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "hefesto/cell/diag_codes.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::cell::detail {

namespace {

namespace cu = db::cmdutil;

enum Keyword { kChild, kParent, kZero, k90, k180, k270, kV, k0V, k90V, k180V, k270V, kH, k0H, k90H, k180H, k270H };
constexpr std::array<std::string_view, 16> kKeywords = {"child", "parent", "0",   "90",  "180", "270",
                                                        "v",     "0v",     "90v", "180v", "270v", "h",
                                                        "0h",    "90h",    "180h", "270h"};

enum RefPoint { kLl, kLr, kUl, kUr, kLabel };
constexpr std::array<std::string_view, 5> kRefPoints = {"ll", "lr", "ul", "ur", "label"};

// The orientations of the keyword table (Geo90Transform, GeoRef45Transform, ...).
db::Transform orientation_of(int keyword) {
  constexpr db::Transform kR90{0, 1, 0, -1, 0, 0};
  constexpr db::Transform kR180{-1, 0, 0, 0, -1, 0};
  constexpr db::Transform kR270{0, -1, 0, 1, 0, 0};
  constexpr db::Transform kUpsideDown{1, 0, 0, 0, -1, 0};
  constexpr db::Transform kSideways{-1, 0, 0, 0, 1, 0};
  constexpr db::Transform kRef45{0, 1, 0, 1, 0, 0};
  constexpr db::Transform kRef135{0, -1, 0, -1, 0, 0};
  switch (keyword) {
    case k90: return kR90;
    case k180: return kR180;
    case k270: return kR270;
    case kV: case k0V: case k180H: return kUpsideDown;
    case k90V: case k270H: return kRef45;
    case k180V: case kH: case k0H: return kSideways;
    case k270V: case k90H: return kRef135;
    default: return db::kIdentity;
  }
}

db::Point corner(const db::Rect& r, int ref) {
  switch (ref) {
    case kLr: return {r.x1, r.y0};
    case kUl: return {r.x0, r.y1};
    case kUr: return {r.x1, r.y1};
    default: return {r.x0, r.y0};
  }
}

// cmdDumpParseArgs' child bounding box: FIXED_BBOX when the cell has one.
db::Rect child_bbox(const db::CellDef& def) {
  if (def.flags().fixed_bbox) {
    if (const auto* v = def.properties().find("FIXED_BBOX")) {
      if (const auto* dims = std::get_if<db::DimensionList>(v); dims != nullptr && dims->values.size() == 4) {
        return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
      }
    }
  }
  return def.bbox();
}

diag::Diagnostic usage_error(std::string_view command, std::string first) {
  return diag::make(codes::kBadUsage, std::move(first) + "\n" + placement_usage(command));
}

/// The parsing state of cmdDumpParseArgs.
struct Parse {
  const cu::Session& s;
  const db::CellDef& child;
  const cmd::Invocation& inv;
  db::Rect bbox;
  std::span<const std::string> av;
  std::optional<db::Point> child_point;
  std::optional<db::Point> root_point;
  std::optional<db::Transform> trans;

  [[nodiscard]] std::size_t ac() const noexcept { return av.size(); }
  void advance(std::size_t n) { av = av.subspan(std::min(n, av.size())); }
  [[nodiscard]] db::Coord coord(std::size_t i, bool is_x) const {
    return cu::scaled_coord(s, av[i], 1, is_x, true);
  }
};

std::optional<diag::Diagnostic> box_error(const Parse& p) {
  return diag::make(db::codes::kNoBox, "The box's lower-left corner must point to the place\n"
                                       "    in the edit cell where you'd like to put \"" +
                                           p.inv.args[0] + "\".");
}

// The box in the edit root (ToolGetBox, rootDef == EditRootDef).
std::optional<db::Rect> root_box(const Parse& p) {
  const editor::EditorState& ed = *p.s.editor;
  if (!ed.box || (ed.box_root != nullptr && ed.edit_root != nullptr && ed.box_root != ed.edit_root)) {
    return std::nullopt;
  }
  return ed.box;
}

/// A reference point in the child (`child REF`, `ORIENT REF`): the point and
/// the number of words used after the keyword; an error otherwise.
diag::Outcome<std::pair<db::Point, std::size_t>> child_ref(const Parse& p, std::size_t label_word) {
  const int ref = cu::lookup_in(p.av[1], kRefPoints);
  if (ref >= kLl && ref <= kUr) {
    return std::pair{corner(p.bbox, ref), std::size_t{2}};
  }
  if (ref == kLabel && p.ac() >= 3) {
    const auto at = label_location(p.child, p.av[label_word]);
    if (!at) {
      return diag::make(db::codes::kNoLabel, "Couldn't find label \"" + p.av[label_word] + "\" in cell \"" +
                                                 p.inv.args[0] + "\".");
    }
    return std::pair{*at, std::size_t{3}};
  }
  if (p.ac() >= 3) {
    return std::pair{db::Point{p.coord(1, true), p.coord(2, false)}, std::size_t{3}};
  }
  return usage_error(p.inv.name, "Must provide two valid coordinates");
}

std::optional<diag::Diagnostic> parse_child(Parse& p) {
  if (p.ac() < 2) {
    return usage_error(p.inv.name, "Keyword must be followed by a reference point");
  }
  auto ref = child_ref(p, 2);
  if (!ref.ok()) {
    return ref.error();
  }
  p.child_point = ref.value().first;
  p.advance(ref.value().second);
  return std::nullopt;
}

std::optional<diag::Diagnostic> parse_parent_label(Parse& p) {
  const editor::EditorState& ed = *p.s.editor;
  for (const db::Label& l : ed.edit_cell->labels()) {
    if (l.text == p.av[2]) {
      p.root_point = db::apply(ed.edit_transform, db::Point{l.rect.x0, l.rect.y0});
      p.advance(3);
      return std::nullopt;
    }
  }
  return diag::make(db::codes::kNoLabel, "Couldn't find label \"" + p.av[2] + "\" in edit cell.");
}

std::optional<diag::Diagnostic> parse_parent(Parse& p) {
  if (p.ac() < 2) {
    return usage_error(p.inv.name, "Keyword must be followed by a reference point");
  }
  const int ref = cu::lookup_in(p.av[1], kRefPoints);
  if (ref >= kLl && ref <= kUr) {
    const auto box = root_box(p);
    if (!box) {
      return box_error(p);
    }
    p.root_point = corner(*box, ref);
    p.advance(2);
    return std::nullopt;
  }
  if (ref == kLabel && p.ac() >= 3) {
    return parse_parent_label(p);
  }
  if (p.ac() < 3) {
    return usage_error(p.inv.name, "Must provide two valid coordinates");
  }
  p.root_point = db::apply(p.s.editor->edit_transform, db::Point{p.coord(1, true), p.coord(2, false)});
  p.advance(3);
  return std::nullopt;
}

// Whether the word after an orientation is a reference point rather than
// the next keyword ("0", "90", "180" and "270" count as coordinates).
bool orientation_has_ref(const Parse& p) {
  if (p.ac() < 2) {
    return false;
  }
  const int next = cu::lookup_in(p.av[1], kKeywords);
  return next < 0 || next == kZero || p.av[1] == "90" || p.av[1] == "180" || p.av[1] == "270";
}

std::optional<diag::Diagnostic> parse_orientation(Parse& p, int keyword) {
  const db::Transform t = orientation_of(keyword);
  if (!orientation_has_ref(p)) {
    // Keep the oriented bounding box's lower-left corner in place.
    const db::Rect r = db::apply(t, p.bbox);
    p.trans = db::compose(db::translation(p.bbox.x0 - r.x0, p.bbox.y0 - r.y0), t);
    p.advance(1);
    return std::nullopt;
  }
  // Magic looks the label of `ORIENT label NAME` up under the word "label"
  // (av[1]); magic-compat keeps that, native mode uses NAME.
  auto ref = child_ref(p, p.s.magic_compat ? 1 : 2);
  if (!ref.ok()) {
    return ref.error();
  }
  const db::Point edit_point = ref.value().first;
  const db::Point moved = db::apply(t, edit_point);
  p.trans = db::compose(db::translation(edit_point.x - moved.x, edit_point.y - moved.y), t);
  p.advance(ref.value().second);
  return std::nullopt;
}

std::optional<diag::Diagnostic> parse_words(Parse& p) {
  while (p.ac() > 0) {
    const int keyword = cu::lookup_in(p.av[0], kKeywords);
    std::optional<diag::Diagnostic> err;
    if (keyword < 0) {
      err = usage_error(p.inv.name, "Unrecognized parent/child keyword: \"" + p.av[0] + "\"");
    } else if (keyword == kChild) {
      err = parse_child(p);
    } else if (keyword == kParent) {
      err = parse_parent(p);
    } else {
      err = parse_orientation(p, keyword);
    }
    if (err) {
      return err;
    }
  }
  return std::nullopt;
}

std::optional<diag::Diagnostic> default_root_point(Parse& p) {
  const editor::EditorState& ed = *p.s.editor;
  if (!ed.box) {
    return box_error(p);
  }
  if (ed.box_root != nullptr && ed.edit_root != nullptr && ed.box_root != ed.edit_root) {
    return diag::make(db::codes::kNoBox, "The box is in cell \"" + ed.box_root->name() + "\", not in the edit cell \"" +
                                             ed.edit_root->name() + "\"");
  }
  p.root_point = db::Point{ed.box->x0, ed.box->y0};
  return std::nullopt;
}

}  // namespace

std::string placement_usage(std::string_view command) {
  return "Usage: " + std::string{command} +
         " cellName [child refPointChild] [parent refPointParent]\n"
         "       [transform [refPointTrans]],\n"
         "       where the refPoints are one of:\n"
         "\t    'label' followed by a single label name,\n"
         "       or 'll' for lower left corner, 'lr' for lower right corner,\n"
         "       'ul' for upper left corner, 'ur' for upper right corner,\n"
         "       or a pair of coordinates.  The transform is one of:\n"
         "       90, 180, 270, v, 90v, 180v, 270v, h, 90h, 180h, or 270h.";
}

diag::Outcome<db::Transform> parse_placement(const cu::Session& s, const db::CellDef& child,
                                             const cmd::Invocation& inv) {
  Parse p{s, child, inv, child_bbox(child), std::span<const std::string>{inv.args}.subspan(1), {}, {}, {}};
  if (auto err = parse_words(p)) {
    return *err;
  }
  if (!p.root_point) {
    if (auto err = default_root_point(p)) {
      return *err;
    }
  }
  const db::Transform trans = p.trans.value_or(db::kIdentity);
  const db::Point child_point = p.child_point.value_or(db::Point{p.bbox.x0, p.bbox.y0});
  // With a child point it is oriented first; the default (the bbox lower
  // left) is not, since the orientation already keeps it in place.
  const db::Point ref = p.child_point ? db::apply(trans, child_point) : child_point;
  return db::compose(db::translation(p.root_point->x - ref.x, p.root_point->y - ref.y), trans);
}

std::optional<db::Point> label_location(const db::CellDef& def, std::string_view name) {
  std::optional<db::Point> found;
  db::search_labels(def, db::whole_plane(), [&](const db::LabelHit& hit) {
    if (hit.path + hit.label->text != name) {
      return true;
    }
    const db::Rect r = db::apply(hit.to_root, hit.label->rect);
    found = db::Point{r.x0, r.y0};
    return false;
  });
  return found;
}

}  // namespace hefesto::cell::detail
