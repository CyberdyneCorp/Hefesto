// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label and port operations (layout-labels-ports).
// Ported from Magic VLSI 8.3.694 (fc5daa9e): database/DBlabel.c
// (DBPutFontLabel, DBPickLabelLayer, dbPickFunc1, dbPickFunc2,
// DBAdjustLabels, dbGetLabelArea, DBEraseGlobLabel), utils/match.c (Match),
// commands/CmdLQ.c (PORT_RENUMBER) and database/DBcellname.c
// (strcmpbynum, made a total order per Q-CELL-12). Magic is Copyright (C)
// 1985, 1990 Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).

#include "hefesto/db/labels.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

#include "hefesto/db/diag_codes.hpp"
#include "hefesto/util/record_order.hpp"
#include "db/tech_masks.hpp"

namespace hefesto::db {

namespace {

constexpr Coord kBorder = 5;

// --- DBPickLabelLayer --------------------------------------------------------

// types[0] covers, types[1] components, types[2] touches.
using PickMasks = std::array<tech::TypeMask, 3>;

PickMasks initial_area_masks() {
  const tech::TypeMask all_but = all_but_space_and_drc();
  return PickMasks{all_but, all_but, tech::TypeMask{}};
}

tech::TileType side_type(const TileView& t) noexcept {
  return t.split ? (t.right_side ? t.right_type : t.left_type) : t.type;
}

// dbPickFunc2 over one plane; returns when space stops the search.
void pick_area(const CellDef& def, const TechMasks& masks, tech::PlaneId plane, const Rect& area, PickMasks& m) {
  const tech::TypeMask& plane_types = masks.plane_types(plane);
  def.plane(plane).enumerate(area, tech::TypeMask::all(), [&](const TileView& tile) {
    const tech::TileType type = side_type(tile);
    if (type == tech::kSpace) {
      m[0] &= ~plane_types;
      m[1] &= ~plane_types;
      return false;
    }
    tech::TypeMask tmp = plane_types;
    tmp.set(type, false);
    m[0] &= ~tmp;
    tmp &= ~masks.residues(type);
    m[1] &= ~tmp;
    m[2].set(type);
    return true;
  });
}

PickMasks pick_point(const CellDef& def, const Label& label) {
  const Rect check = expand(label.rect, 1);
  tech::TypeMask touching;
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(check, tech::TypeMask::all(), [&](const TileView& tile) {
      const tech::TileType type = side_type(tile);
      if (type != tech::kSpace) {
        touching.set(type);
      }
      return true;
    });
  }
  return PickMasks{touching, touching, touching};
}

PickMasks pick_areas(const CellDef& def, const TechMasks& masks, const Rect& a, const std::optional<Rect>& b) {
  PickMasks m1 = initial_area_masks();
  PickMasks m2 = initial_area_masks();
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    pick_area(def, masks, static_cast<tech::PlaneId>(p), a, m1);
    if (b) {
      pick_area(def, masks, static_cast<tech::PlaneId>(p), *b, m2);
    }
  }
  if (b) {
    for (std::size_t i = 0; i < 3; ++i) {
      m1[i] |= m2[i];
    }
  }
  return m1;
}

PickMasks pick_masks(const CellDef& def, const TechMasks& masks, const Label& label) {
  const Rect& r = label.rect;
  if (r.x0 == r.x1 && r.y0 == r.y1) {
    return pick_point(def, label);
  }
  if (r.x0 == r.x1) {
    return pick_areas(def, masks, Rect{r.x0 - 1, r.y0, r.x1, r.y1}, Rect{r.x0, r.y0, r.x1 + 1, r.y1});
  }
  if (r.y0 == r.y1) {
    return pick_areas(def, masks, Rect{r.x0, r.y0 - 1, r.x1, r.y1}, Rect{r.x0, r.y0, r.x1, r.y1 + 1});
  }
  return pick_areas(def, masks, r, std::nullopt);
}

struct Choices {
  std::array<tech::TileType, 6> c{};  // choice1..choice6, all space
};

class LayerChooser {
 public:
  LayerChooser(const tech::Technology& tech, const PickMasks& m, tech::TileType current)
      : tech_(tech), m_(m), current_(current), plane_(tech.home_plane(current)) {}

  void consider(tech::TileType i) {
    if (tech_.connects_to(i).test(current_) && consider_connected(i)) {
      return;
    }
    if (same_plane(i) || current_ == tech::kSpace) {
      consider_same_plane(i);
    }
  }

  [[nodiscard]] tech::TileType result() const noexcept {
    for (const tech::TileType t : choices_.c) {
      if (t != tech::kSpace) {
        return t;
      }
    }
    return tech::kSpace;
  }

 private:
  [[nodiscard]] bool same_plane(tech::TileType i) const { return plane_ && tech_.home_plane(i) == plane_; }

  // Returns true when the type was taken (`continue` in Magic).
  bool consider_connected(tech::TileType i) {
    if (same_plane(i)) {
      if (m_[0].test(i)) {
        choices_.c[0] = i;
        return true;
      }
      if (m_[1].test(i)) {
        choices_.c[1] = i;
        return true;
      }
    }
    if (m_[0].test(i)) {
      choices_.c[2] = i;
      return true;
    }
    if (m_[1].test(i)) {
      choices_.c[3] = i;
      return true;
    }
    return false;
  }

  void consider_same_plane(tech::TileType i) {
    if (m_[0].test(i)) {
      if (choices_.c[4] == tech::kSpace) {
        choices_.c[4] = i;
        return;
      }
      tech::TypeMask others = tech_.connects_to(i);
      others.set(i, false);
      if (!others.empty()) {
        choices_.c[4] = i;
      } else if (m_[1].test(i)) {
        choices_.c[5] = i;
      }
      return;
    }
    if (m_[1].test(i)) {
      choices_.c[5] = i;
    }
  }

  const tech::Technology& tech_;
  const PickMasks& m_;
  tech::TileType current_;
  std::optional<tech::PlaneId> plane_;
  Choices choices_;
};

// --- DBAdjustLabels ------------------------------------------------------------

// dbGetLabelArea over the tiles of other types under the label.
std::optional<Rect> remaining_label_area(const CellDef& def, tech::PlaneId plane, const Label& label) {
  const Plane& p = def.plane(plane);
  const tech::TypeMask only = tech::TypeMask::of(label.type);
  bool any = false;
  p.enumerate(label.rect, only, [&](const TileView&) {
    any = true;
    return false;
  });
  if (!any) {
    return std::nullopt;
  }
  Rect r = label.rect;
  p.enumerate(label.rect, ~only, [&](const TileView& tile) {
    const Rect& t = tile.area;
    if (t.x0 > r.x0) {
      r.x1 = t.x0;
    } else if (t.x1 < r.x1) {
      r.x0 = t.x1;
    } else if (t.y0 > r.y0) {
      r.y1 = t.y0;
    } else if (t.y1 < r.y1) {
      r.y0 = t.y1;
    }
    return true;
  });
  return r.is_null() ? std::nullopt : std::optional<Rect>{r};
}

bool glob_at(std::string_view p, std::string_view s) noexcept;

// Whether `c` lies in the range `lo`-`hi` (either order).
bool in_range(char lo, char hi, char c) noexcept {
  return (lo <= c && hi >= c) || (lo >= c && hi <= c);
}

// Index just past the matching item of a "[...]" class at the head of `p`
// (scanning from 1), or 0 when `c` matches nothing.
std::size_t glob_class_match(std::string_view p, char c) noexcept {
  for (std::size_t i = 1; i < p.size() && p[i] != ']'; ++i) {
    if (p[i] == c) {
      return i;
    }
    if (i + 1 < p.size() && p[i + 1] == '-') {
      if (i + 2 >= p.size()) {
        return 0;
      }
      if (in_range(p[i], p[i + 2], c)) {
        return i;
      }
      i += 2;
    }
  }
  return 0;
}

// One "[...]" class at the head of `p` against `c`; returns the length of
// the class or 0 when it does not match.
std::size_t glob_class(std::string_view p, char c) noexcept {
  std::size_t i = glob_class_match(p, c);
  if (i == 0) {
    return 0;
  }
  while (i < p.size() && p[i] != ']') {
    ++i;
  }
  return i < p.size() ? i + 1 : i;
}

// "*" at the head of `p`: tries every postfix of `s`.
bool glob_star(std::string_view p, std::string_view s) noexcept {
  p.remove_prefix(1);
  if (p.empty()) {
    return true;
  }
  for (; !s.empty(); s.remove_prefix(1)) {
    if (glob_at(p, s)) {
      return true;
    }
  }
  return false;
}

// Pattern characters consumed by matching one character `c`; 0 = mismatch.
std::size_t glob_step(std::string_view p, char c) noexcept {
  switch (p.front()) {
    case '?':
      return 1;
    case '[':
      return glob_class(p, c);
    case '\\':
      return p.size() >= 2 && p[1] == c ? 2 : 0;
    default:
      return p.front() == c ? 1 : 0;
  }
}

bool glob_at(std::string_view p, std::string_view s) noexcept {
  while (!p.empty()) {
    if (p.front() == '*') {
      return glob_star(p, s);
    }
    if (s.empty()) {
      return false;
    }
    const std::size_t step = glob_step(p, s.front());
    if (step == 0) {
      return false;
    }
    p.remove_prefix(std::min(step, p.size()));
    s.remove_prefix(1);
  }
  return s.empty();
}

int lower(char c) noexcept {
  return std::tolower(static_cast<unsigned char>(c));
}

bool same_text(std::string_view a, std::string_view b, bool case_sensitive) noexcept {
  if (case_sensitive) {
    return a == b;
  }
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return lower(x) == lower(y); });
}

}  // namespace

Position auto_justification(const Rect& b, const Rect& rect) noexcept {
  const Coord tx = std::min<Coord>(kBorder, (b.x1 - b.x0) / 3);
  const Coord ty = std::min<Coord>(kBorder, (b.y1 - b.y0) / 3);
  const Coord x1 = b.x0 + tx;
  const Coord x2 = b.x1 - tx;
  const Coord y1 = b.y0 + ty;
  const Coord y2 = b.y1 - ty;
  const Coord lx = (rect.x1 + rect.x0) / 2;
  const Coord ly = (rect.y1 + rect.y0) / 2;
  const auto pick = [&](Position low, Position high, Position mid) { return ly <= y1 ? low : (ly >= y2 ? high : mid); };
  if (lx <= x1) {
    return pick(kNorthEast, kSouthEast, kEast);
  }
  if (lx >= x2) {
    return pick(kNorthWest, kSouthWest, kWest);
  }
  return pick(kNorth, kSouth, kNorth);
}

diag::Outcome<Label*> put_label(CellDef& def, Label label, bool auto_justify) {
  if (label.text.empty()) {
    return diag::make(codes::kLabelEmpty, "a label needs non-empty text");
  }
  if (auto_justify) {
    label.justification = auto_justification(def.bbox(), label.rect);
  }
  if (label.font < 0) {
    label.size = 0;
    label.rotation = 0;
    label.offset = Point{};
  }
  def.labels().push_back(std::move(label));
  mark_modified(def);
  return &def.labels().back();
}

tech::TileType choose_label_layer(const CellDef& def, const tech::Technology& tech, const Label& label) {
  const TechMasks masks(tech, def.plane_count());
  const PickMasks m = pick_masks(def, masks, label);
  if (m[0].test(label.type)) {
    return label.type;
  }
  LayerChooser chooser(tech, m, label.type);
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    const tech::TypeMask& on_plane = masks.plane_types(static_cast<tech::PlaneId>(p));
    for (std::size_t i = tech::kMagnet; i < tech.user_type_end(); ++i) {
      if (on_plane.test(static_cast<tech::TileType>(i))) {
        chooser.consider(static_cast<tech::TileType>(i));
      }
    }
  }
  return chooser.result();
}

bool adjust_labels(CellDef& def, const tech::Technology& tech, const Rect& area, std::vector<std::string>* messages) {
  bool modified = false;
  for (Label& lab : def.labels()) {
    if (!touch(lab.rect, area)) {
      continue;
    }
    const tech::TileType new_type = choose_label_layer(def, tech, lab);
    if (new_type == lab.type || lab.sticky) {
      continue;
    }
    const auto plane = tech.home_plane(lab.type);
    if (new_type == tech::kSpace && plane && *plane < def.plane_count()) {
      if (const auto r = remaining_label_area(def, *plane, lab)) {
        if (messages != nullptr && !def.flags().internal) {
          messages->push_back("Adjusting size of label \"" + lab.text + "\" in cell " + def.name() + ".");
        }
        lab.rect = *r;
        modified = true;
        continue;
      }
    }
    if (messages != nullptr && !def.flags().internal) {
      messages->push_back("Moving label \"" + lab.text + "\" from " + std::string{tech.type_name(lab.type)} +
                          " to " + std::string{tech.type_name(new_type)} + " in cell " + def.name() + ".");
    }
    lab.type = new_type;
    modified = true;
  }
  if (modified) {
    mark_modified(def);
  }
  return modified;
}

std::size_t erase_labels(CellDef& def, const tech::Technology& tech, const Rect& area, const tech::TypeMask& types,
                         std::optional<std::string_view> glob) {
  const auto doomed = [&](const Label& lab) {
    if (!label_in_area(lab.rect, area)) {
      return false;
    }
    if (!types.test(kLabelsPseudoType)) {
      if (!types.test(lab.type)) {
        return false;
      }
      if (lab.type != tech::kSpace && tech.connects_to(choose_label_layer(def, tech, lab)).test(lab.type)) {
        return false;
      }
    }
    return !glob || glob_match(*glob, lab.text);
  };
  // Decide every label against the paint before removing any (removal does
  // not change paint, so the outcome equals Magic's single pass).
  std::vector<bool> erase(def.labels().size());
  std::transform(def.labels().begin(), def.labels().end(), erase.begin(), doomed);
  std::size_t n = 0;
  std::size_t i = 0;
  auto& labels = def.labels();
  labels.erase(std::remove_if(labels.begin(), labels.end(), [&](const Label&) { return erase[i++]; }), labels.end());
  n = static_cast<std::size_t>(std::count(erase.begin(), erase.end(), true));
  if (n > 0) {
    mark_modified(def);
  }
  return n;
}

bool glob_match(std::string_view pattern, std::string_view text) noexcept {
  return glob_at(pattern, text);
}

tech::TileType label_layer_for(const tech::Technology& tech, tech::TileType type) noexcept {
  const tech::ContactInfo* c = tech.contact(type);
  if (c == nullptr || !c->stacked) {
    return type;
  }
  return std::max(c->component_a, c->component_b);
}

int natural_compare(std::string_view a, std::string_view b) noexcept {
  return util::compare_natural(a, b);
}

void renumber_ports(CellDef& def, bool magic_compat) {
  std::vector<Label*> sorted;
  sorted.reserve(def.labels().size());
  for (Label& l : def.labels()) {
    sorted.push_back(&l);
  }
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const Label* a, const Label* b) { return natural_compare(a->text, b->text) < 0; });
  std::uint32_t p = 0;
  const Label* last = nullptr;
  for (Label* lab : sorted) {
    if (!lab->is_port()) {
      continue;
    }
    if (last != nullptr && same_text(last->text, lab->text, magic_compat)) {
      --p;
    }
    lab->port_index = p;
    last = lab;
    ++p;
  }
}

}  // namespace hefesto::db
