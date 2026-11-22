// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The 3D window's face log (face_log.hpp; m10-3d-view task 12.4). The face
// order and winding follow Magic 8.3.694 (fc5daa9e) graphics/W3Dmain.c
// (w3dFillTile, w3dFillPolygon, w3dFillDiagonal, w3dRenderVolume),
// Copyright (C) 1985, 1990 Regents of the University of California, used
// under Magic's permissive license (see THIRD_PARTY_NOTICES.md).
//
// Owner: WIND3D (design D3).

#include "hefesto/view3d/client/face_log.hpp"

#include <array>
#include <cstdio>
#include <vector>

#include "hefesto/display/scene.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::view3d {

namespace {

struct Vertex {
  float x;
  float y;
  float z;
};

// z top and z bottom of a face drawn with h and t (w3dRenderVolume).
struct ZRange {
  float top;
  float bottom;
};

ZRange z_range(const DrawHeight& ht, float scale_z) {
  const float top = -ht.height * scale_z;
  return ZRange{top, top - (ht.thickness * scale_z)};
}

std::string style_name(const display::StyleSet* styles, std::size_t index) {
  if (styles != nullptr && index < styles->table.styles.size()) {
    const std::optional<std::string>& name = styles->table.styles[index].long_name;
    if (name && !name->empty() && name->find_first_of(" \t\n") == std::string::npos) {
      return *name;
    }
  }
  return "#" + std::to_string(index);
}

// Only solid and stippled styles are drawn (N3; W3Dmain.c:406, :439).
bool drawable(const display::StyleSet* styles, std::size_t index) {
  if (styles == nullptr) {
    return true;
  }
  const display::FillKind fill = display::resolve_style(*styles, index).fill;
  return fill == display::FillKind::solid || fill == display::FillKind::stipple;
}

class Writer {
 public:
  Writer(const Scene& scene, const ViewState& view, const display::StyleSet* styles)
      : scene_(scene), scale_z_(view.scale_z) {
    const std::size_t count = scene.stack.size();
    names_.resize(count);
    drawn_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t style = scene.stack.layers()[i].style;
      names_[i] = style_name(styles, style);
      drawn_[i] = drawable(styles, style) && !(i < scene.hidden.size() && scene.hidden[i]);
    }
  }

  std::string run() {
    out_ = "gl-capture 1\n";
    if (scene_.mesh) {
      for (const Chunk& chunk : scene_.mesh->chunks) {
        write_chunk(chunk);
      }
    }
    return std::move(out_);
  }

 private:
  [[nodiscard]] bool drawn(LayerId layer) const { return layer < drawn_.size() && drawn_[layer]; }

  [[nodiscard]] DrawHeight layer_height(LayerId layer) const {
    const std::vector<DrawHeight>& heights = scene_.mesh->layer_heights;
    return layer < heights.size() ? heights[layer] : scene_.stack.draw_height(layer);
  }

  [[nodiscard]] float x_of(const Chunk& c, float x) const {
    return static_cast<float>((c.origin_x + static_cast<double>(x)) * static_cast<double>(scene_.xy_factor));
  }
  [[nodiscard]] float y_of(const Chunk& c, float y) const {
    return static_cast<float>((c.origin_y + static_cast<double>(y)) * static_cast<double>(scene_.xy_factor));
  }

  void poly(LayerId layer, const std::vector<Vertex>& vertices) {
    out_ += "poly " + names_[layer] + " " + std::to_string(vertices.size());
    std::array<char, 64> buffer{};
    for (const Vertex& v : vertices) {
      std::snprintf(buffer.data(), buffer.size(), " %.9g %.9g %.9g", static_cast<double>(v.x),
                    static_cast<double>(v.y), static_cast<double>(v.z));
      out_ += buffer.data();
    }
    out_ += '\n';
  }

  // A top face (counter-clockwise from above) and the bottom face (the
  // same ring reversed), w3dFillTile / w3dFillPolygon.
  void flat_pair(LayerId layer, std::vector<Vertex> ring, const ZRange& z) {
    for (Vertex& v : ring) {
      v.z = z.top;
    }
    poly(layer, ring);
    std::vector<Vertex> bottom(ring.rbegin(), ring.rend());
    for (Vertex& v : bottom) {
      v.z = z.bottom;
    }
    poly(layer, bottom);
  }

  void write_rect(const Chunk& c, const RectInstance& r) {
    const float x0 = x_of(c, r.x0);
    const float y0 = y_of(c, r.y0);
    const float x1 = x_of(c, r.x1);
    const float y1 = y_of(c, r.y1);
    flat_pair(r.layer, {{x0, y0, 0.0F}, {x1, y0, 0.0F}, {x1, y1, 0.0F}, {x0, y1, 0.0F}},
              z_range(layer_height(r.layer), scale_z_));
  }

  void write_triangle(const Chunk& c, const Triangle& t) {
    std::vector<Vertex> ring;
    for (int i = 0; i < 3; ++i) {
      ring.push_back({x_of(c, t.x[i]), y_of(c, t.y[i]), 0.0F});
    }
    const double area = (static_cast<double>(ring[1].x - ring[0].x) * static_cast<double>(ring[2].y - ring[0].y)) -
                        (static_cast<double>(ring[2].x - ring[0].x) * static_cast<double>(ring[1].y - ring[0].y));
    if (area < 0.0) {
      std::swap(ring[1], ring[2]);  // counter-clockwise from above
    }
    flat_pair(t.layer, std::move(ring), z_range(DrawHeight{t.height, t.thickness}, scale_z_));
  }

  // a_bot, b_bot, b_top, a_top with the outside on the right of a -> b
  // (w3dFillDiagonal; the renderer's side quad).
  void write_side(const Chunk& c, const SideInstance& s) {
    const bool left = (s.flags & side_flag::outward_left) != 0;
    const Vertex a{x_of(c, left ? s.x1 : s.x0), y_of(c, left ? s.y1 : s.y0), 0.0F};
    const Vertex b{x_of(c, left ? s.x0 : s.x1), y_of(c, left ? s.y0 : s.y1), 0.0F};
    const DrawHeight ht = (s.flags & side_flag::own_height) != 0 ? DrawHeight{s.height, s.thickness}
                                                                  : layer_height(s.layer);
    const ZRange z = z_range(ht, scale_z_);
    if (z.top == z.bottom) {
      return;  // "If height is zero, don't bother to draw sides"
    }
    poly(s.layer, {{a.x, a.y, z.bottom}, {b.x, b.y, z.bottom}, {b.x, b.y, z.top}, {a.x, a.y, z.top}});
  }

  void write_chunk(const Chunk& c) {
    for (const RectInstance& r : c.rects) {
      if (drawn(r.layer)) {
        write_rect(c, r);
      }
    }
    for (const Triangle& t : c.triangles) {
      if (drawn(t.layer)) {
        write_triangle(c, t);
      }
    }
    for (const SideInstance& s : c.sides) {
      if (drawn(s.layer)) {
        write_side(c, s);
      }
    }
  }

  const Scene& scene_;
  float scale_z_;
  std::vector<std::string> names_;
  std::vector<bool> drawn_;
  std::string out_;
};

}  // namespace

std::string face_log(const Scene& scene, const ViewState& view, const display::StyleSet* styles) {
  return Writer{scene, view, styles}.run();
}

void FaceLogDisplay::redraw(const editor::EditorState& editor, const WindowState& window) {
  const bool compat = editor.drc.compat_settings().magic_compat;
  BuildContext ctx;
  const Scene scene = build_scene(editor, window, compat, ctx);
  const display::StyleSet* styles = editor.styles ? &*editor.styles : nullptr;
  static_cast<void>(files_.write_atomic(path_, face_log(scene, window.view, styles)));
  if (next_ != nullptr) {
    next_->redraw(editor, window);
  }
}

void FaceLogDisplay::closed(const WindowState& window) {
  if (next_ != nullptr) {
    next_->closed(window);
  }
}

}  // namespace hefesto::view3d
