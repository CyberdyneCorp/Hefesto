// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// 2D camera of the layout canvas: pan and zoom over internal database units,
// and the integer-origin transform the vertex shader uses so that full-chip
// coordinates keep their precision at any zoom. Vulkan-free.

#pragma once

#include <cstdint>

#include "hefesto/render/scene.hpp"

namespace hefesto::render {

/// What the vertex shader needs (push constants). A world point p maps to
/// normalized device coordinates as  ndc = float(p - origin) * scale + offset.
/// The subtraction is done in integers, so precision depends only on the
/// distance to the view center, not on the absolute coordinate.
struct ViewTransform {
  std::int32_t origin_x = 0;
  std::int32_t origin_y = 0;
  float scale_x = 1.0F;
  float scale_y = -1.0F;
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  /// Minimum drawn size in NDC (one pixel): sub-pixel rectangles stay visible.
  float min_size_x = 0.0F;
  float min_size_y = 0.0F;
};

class Camera {
 public:
  /// Smallest and largest zoom, in pixels per internal unit.
  static constexpr double kMinPixelsPerUnit = 1e-7;
  static constexpr double kMaxPixelsPerUnit = 1e3;

  Camera() = default;
  Camera(double center_x, double center_y, double pixels_per_unit) noexcept;

  void set_viewport(int width_px, int height_px) noexcept;
  /// set_viewport() keeping the world point at the top-left pixel fixed, so
  /// every pixel still in the viewport shows the same point (scale kept).
  /// A canvas resized under the pointer (a panel opening below it) then
  /// keeps the layout under the cursor. Without a previous viewport size
  /// (the 1 x 1 default) it is set_viewport().
  void resize_keeping_top_left(int width_px, int height_px) noexcept;
  /// Moves the view by a drag of (dx, dy) pixels (screen y grows downwards).
  void pan_pixels(double dx_px, double dy_px) noexcept;
  /// Multiplies the zoom by `factor`, keeping the world point under the
  /// pixel (x_px, y_px) fixed on screen.
  void zoom_at(double factor, double x_px, double y_px) noexcept;
  /// Centers `bounds` and zooms so that it fills the viewport minus a margin.
  void fit(const Bounds& bounds, double margin_fraction = 0.05) noexcept;

  [[nodiscard]] double center_x() const noexcept { return center_x_; }
  [[nodiscard]] double center_y() const noexcept { return center_y_; }
  [[nodiscard]] double pixels_per_unit() const noexcept { return pixels_per_unit_; }
  [[nodiscard]] int viewport_width() const noexcept { return width_px_; }
  [[nodiscard]] int viewport_height() const noexcept { return height_px_; }

  /// World coordinates of a viewport pixel.
  [[nodiscard]] double world_x(double x_px) const noexcept;
  [[nodiscard]] double world_y(double y_px) const noexcept;

  [[nodiscard]] double units_per_pixel() const noexcept { return 1.0 / pixels_per_unit_; }

  /// The world rectangle the viewport shows.
  [[nodiscard]] WorldWindow visible_window() const noexcept;

  [[nodiscard]] ViewTransform transform() const noexcept;

 private:
  double center_x_ = 0.0;
  double center_y_ = 0.0;
  double pixels_per_unit_ = 1.0;
  int width_px_ = 1;
  int height_px_ = 1;
};

}  // namespace hefesto::render
