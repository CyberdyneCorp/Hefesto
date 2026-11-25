// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// View state and camera (openspec/changes/m10-3d-view/design.md D7;
// view-3d "Default view", "Viewing transform and shading"; Q-MISC-74 fix,
// Q-MISC-78 compat). The view values follow Magic's `float` arithmetic
// (graphics/W3Dmain.c Set3DDefaults :1332-1380, w3dRescale :847-857, the
// `view`/`scroll`/`zoom` commands :1024-1230), so the commands print what
// Magic prints.
//
// Owner: CAMERA (design D3).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "hefesto/view3d/camera/view_state.hpp"

namespace hefesto::view3d {

namespace {

bool positive_finite(float f) {
  return std::isfinite(f) && f > 0.0F;
}

// Magic's `2.0 / ((float)extent * 1.1)`: the product and the division in
// double, rounded to the `float` scalex/scaley.
float fit_scale(std::int64_t extent) {
  return static_cast<float>(2.0 / (static_cast<double>(static_cast<float>(extent)) * 1.1));
}

// Magic's `(scalex > scaley) ? scaley : scalex`. An empty extent would
// divide by zero (an infinite scale, then NaN in the matrix): it is left
// out, and a cell empty in both directions is fitted as 1 x 1.
float default_scale(std::int64_t width, std::int64_t height) {
  if (width <= 0 && height <= 0) {
    return fit_scale(1);
  }
  if (width <= 0) {
    return fit_scale(height);
  }
  if (height <= 0) {
    return fit_scale(width);
  }
  const float scalex = fit_scale(width);
  const float scaley = fit_scale(height);
  return (scalex > scaley) ? scaley : scalex;
}

Matrix4 identity() {
  return Matrix4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
}

// Column-major element (row r, column c).
float& at(Matrix4& m, int r, int c) {
  return m[static_cast<std::size_t>((c * 4) + r)];
}

float at(const Matrix4& m, int r, int c) {
  return m[static_cast<std::size_t>((c * 4) + r)];
}

Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
  Matrix4 out{};
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      float sum = 0.0F;
      for (int k = 0; k < 4; ++k) {
        sum += at(a, r, k) * at(b, k, c);
      }
      at(out, r, c) = sum;
    }
  }
  return out;
}

Matrix4 scaling(float x, float y, float z) {
  Matrix4 m = identity();
  at(m, 0, 0) = x;
  at(m, 1, 1) = y;
  at(m, 2, 2) = z;
  return m;
}

Matrix4 translation(float x, float y, float z) {
  Matrix4 m = identity();
  at(m, 0, 3) = x;
  at(m, 1, 3) = y;
  at(m, 2, 3) = z;
  return m;
}

// glRotatef(degrees, axis) for a coordinate axis (0 = x, 1 = y, 2 = z): a
// right-handed rotation.
Matrix4 rotation(float degrees, int axis) {
  const double radians = static_cast<double>(degrees) * std::numbers::pi / 180.0;
  const auto c = static_cast<float>(std::cos(radians));
  const auto s = static_cast<float>(std::sin(radians));
  const int u = (axis + 1) % 3;  // the plane of rotation is (u, v)
  const int v = (axis + 2) % 3;
  Matrix4 m = identity();
  at(m, u, u) = c;
  at(m, u, v) = -s;
  at(m, v, u) = s;
  at(m, v, v) = c;
  return m;
}

// Q-MISC-74: A from the drawable's current size, keeping internal units 1:1
// and fitting the limiting dimension.
Matrix4 aspect(int width, int height) {
  if (width <= 0 || height <= 0 || width == height) {
    return identity();
  }
  const auto w = static_cast<float>(width);
  const auto h = static_cast<float>(height);
  return width > height ? scaling(h / w, 1.0F, 1.0F) : scaling(1.0F, w / h, 1.0F);
}

constexpr float kMinFieldOfView = 1.0F;
constexpr float kMaxFieldOfView = 150.0F;
constexpr float kDefaultFieldOfView = 45.0F;

float clamped_field_of_view(float degrees) {
  if (!std::isfinite(degrees)) {
    return kDefaultFieldOfView;
  }
  return std::fmin(std::fmax(degrees, kMinFieldOfView), kMaxFieldOfView);
}

// The orthographic view volume [-1, 1]^3 seen from z = +d, with the field
// of view chosen so that the z = 0 plane frames as in the orthographic
// view. The depth row is negated so that z' grows toward the viewer, as in
// the orthographic view.
Matrix4 perspective(float field_of_view_deg) {
  const double half = static_cast<double>(clamped_field_of_view(field_of_view_deg)) * std::numbers::pi / 360.0;
  const auto d = static_cast<float>(1.0 / std::tan(half));
  const float near_z = std::fmax(d - 1.0F, 0.01F * d);
  const float far_z = d + 1.0F;
  Matrix4 m{};
  at(m, 0, 0) = d;
  at(m, 1, 1) = d;
  at(m, 2, 2) = -(far_z + near_z) / (near_z - far_z);
  at(m, 2, 3) = -(2.0F * far_z * near_z) / (near_z - far_z);
  at(m, 3, 2) = -1.0F;
  return multiply(m, translation(0.0F, 0.0F, -d));
}

void set_angles(ViewState& view, const Angles& angles) {
  view.view_x = angles.x;
  view.view_y = angles.y;
  view.view_z = angles.z;
}

// `zoom factor 1 rel` / `zoom factor factor rel`.
void zoom(ViewState& view, float factor, bool zoom_z) {
  if (!positive_finite(factor)) {
    return;
  }
  view.scale_xy *= factor;
  if (zoom_z) {
    view.scale_z *= factor;
  }
}

// `scroll dx dy 0 rel`. A zero scale_xy (reachable only by editing the
// struct) would divide by zero.
void pan(ViewState& view, float dx, float dy) {
  if (!positive_finite(view.scale_xy)) {
    return;
  }
  view.trans_x += dx / view.scale_xy;
  view.trans_y += dy / view.scale_xy;
}

}  // namespace

ViewState default_view(const db::Rect& bbox, Mode mode, float f) {
  const std::int64_t width = bbox.width();
  const std::int64_t height = bbox.height();
  ViewState view;
  view.trans_x = static_cast<float>(-(std::int64_t{bbox.x0} + (width >> 1)));
  view.trans_y = static_cast<float>(-(std::int64_t{bbox.y0} + (height >> 1)));
  view.trans_z = 0.0F;
  view.scale_xy = default_scale(width, height);
  view.scale_z = 25.0F;
  view.prescale_z = static_cast<float>(0.0001);
  if (mode == Mode::mask_layer) {
    rescale(view, f);
  }
  return view;
}

void rescale(ViewState& view, float f) {
  if (!positive_finite(f)) {
    return;
  }
  view.scale_xy /= f;
  view.prescale_z /= f;
  view.scale_z *= f;
  view.trans_y *= f;
  view.trans_x *= f;
}

float inverse_factor(float f) {
  return static_cast<float>(1.0 / static_cast<double>(f));
}

Matrix4 matrix(const ViewState& view, int width, int height, const ProjectionParams& projection) {
  Matrix4 m = multiply(aspect(width, height), scaling(view.scale_xy, view.scale_xy, view.prescale_z));
  m = multiply(m, rotation(view.view_x, 0));
  m = multiply(m, rotation(view.view_y, 1));
  m = multiply(m, rotation(view.view_z, 2));
  m = multiply(m, translation(view.trans_x, view.trans_y, view.trans_z));
  if (projection.kind == Projection::perspective) {
    m = multiply(perspective(projection.field_of_view_deg), m);
  }
  return m;
}

void apply(ViewState& view, const Gesture& gesture) {
  const ViewState unchanged = view;
  apply(view, gesture, unchanged);
}

void apply(ViewState& view, const Gesture& gesture, const ViewState& home) {
  switch (gesture.kind) {
    case Gesture::Kind::orbit:  // `view dy 0 dx rel`
      view.view_x += gesture.dy;
      view.view_z += gesture.dx;
      return;
    case Gesture::Kind::pan:
      pan(view, gesture.dx, gesture.dy);
      return;
    case Gesture::Kind::zoom:
      zoom(view, gesture.factor, gesture.zoom_z);
      return;
    case Gesture::Kind::preset:  // `view ax ay az`
      set_angles(view, preset(gesture.preset));
      return;
    case Gesture::Kind::reset:  // `defaults`
      view = home;
      return;
  }
}

Angles preset(Preset which) {
  switch (which) {
    case Preset::top:
      return Angles{0.0F, 0.0F, 0.0F};
    case Preset::isometric:
      return Angles{-54.7356F, 0.0F, 45.0F};
    case Preset::front:
      return Angles{-90.0F, 0.0F, 0.0F};
    case Preset::side:
      return Angles{-90.0F, 0.0F, -90.0F};
  }
  return Angles{};
}

namespace {

bool finite_box(const Extent3& b) {
  return std::isfinite(b.x0) && std::isfinite(b.y0) && std::isfinite(b.x1) && std::isfinite(b.y1) &&
         std::isfinite(b.z0) && std::isfinite(b.z1);
}

// The largest |x| or |y| in clip space of the corners of `b` (displayed
// coordinates) under `m`.
double clip_extent(const Matrix4& m, const Extent3& b, double scale_z) {
  double extent = 0.0;
  for (unsigned corner = 0; corner < 8U; ++corner) {
    const double x = (corner & 1U) != 0U ? b.x1 : b.x0;
    const double y = (corner & 2U) != 0U ? b.y1 : b.y0;
    const double z = ((corner & 4U) != 0U ? b.z1 : b.z0) * scale_z;
    for (int r = 0; r < 2; ++r) {
      const auto e = [&](int c) { return static_cast<double>(at(m, r, c)); };
      const double v = (e(0) * x) + (e(1) * y) + (e(2) * z) + e(3);
      extent = std::fmax(extent, std::fabs(v));
    }
  }
  return extent;
}

}  // namespace

ViewState frame_view(const ViewState& view, const Extent3& box, int width_px, int height_px) {
  if (!finite_box(box) || !std::isfinite(view.scale_z)) {
    return view;
  }
  const auto scale_z = static_cast<double>(view.scale_z);
  ViewState out = view;
  out.trans_x = static_cast<float>(-0.5 * (box.x0 + box.x1));
  out.trans_y = static_cast<float>(-0.5 * (box.y0 + box.y1));
  out.trans_z = static_cast<float>(-0.5 * (box.z0 + box.z1) * scale_z);
  ViewState unit = out;
  unit.scale_xy = 1.0F;
  const double extent = clip_extent(matrix(unit, width_px, height_px, ProjectionParams{}), box, scale_z);
  // Below a millionth of a root unit the box is a point (float rounding of
  // the rotation, not an extent).
  constexpr double kPointExtent = 1e-6;
  if (extent > kPointExtent && std::isfinite(extent)) {
    out.scale_xy = static_cast<float>(kFrameFill / extent);
  }
  // A huge scale_z or box overflows the float translation (`scroll inf`
  // would turn the view into NaN): keep the view instead.
  const bool finite = std::isfinite(out.trans_x) && std::isfinite(out.trans_y) && std::isfinite(out.trans_z) &&
                      std::isfinite(out.scale_xy) && out.scale_xy > 0.0F;
  return finite ? out : view;
}

float exit_mask_factor(bool magic_compat, float applied_factor, float current_factor) {
  return magic_compat ? current_factor : applied_factor;
}

float change_output_style(ViewState& view, bool magic_compat, float f_old, float f_new) {
  if (magic_compat || !positive_finite(f_old) || !positive_finite(f_new)) {
    return f_old;
  }
  rescale(view, f_new / f_old);
  return f_new;
}

}  // namespace hefesto::view3d
