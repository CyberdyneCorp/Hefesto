// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/render/camera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hefesto::render {

namespace {

std::int32_t clamp_to_int32(double value) noexcept {
  constexpr auto lo = static_cast<double>(std::numeric_limits<std::int32_t>::min());
  constexpr auto hi = static_cast<double>(std::numeric_limits<std::int32_t>::max());
  return static_cast<std::int32_t>(std::clamp(std::round(value), lo, hi));
}

double clamp_zoom(double pixels_per_unit) noexcept {
  return std::clamp(pixels_per_unit, Camera::kMinPixelsPerUnit, Camera::kMaxPixelsPerUnit);
}

}  // namespace

Camera::Camera(double center_x, double center_y, double pixels_per_unit) noexcept
    : center_x_(center_x), center_y_(center_y), pixels_per_unit_(clamp_zoom(pixels_per_unit)) {}

void Camera::set_viewport(int width_px, int height_px) noexcept {
  width_px_ = std::max(width_px, 1);
  height_px_ = std::max(height_px, 1);
}

void Camera::pan_pixels(double dx_px, double dy_px) noexcept {
  center_x_ -= dx_px / pixels_per_unit_;
  center_y_ += dy_px / pixels_per_unit_;
}

double Camera::world_x(double x_px) const noexcept {
  return center_x_ + (x_px - 0.5 * width_px_) / pixels_per_unit_;
}

double Camera::world_y(double y_px) const noexcept {
  return center_y_ - (y_px - 0.5 * height_px_) / pixels_per_unit_;
}

void Camera::zoom_at(double factor, double x_px, double y_px) noexcept {
  if (!(factor > 0.0)) {
    return;
  }
  const double anchor_x = world_x(x_px);
  const double anchor_y = world_y(y_px);
  pixels_per_unit_ = clamp_zoom(pixels_per_unit_ * factor);
  center_x_ = anchor_x - (x_px - 0.5 * width_px_) / pixels_per_unit_;
  center_y_ = anchor_y + (y_px - 0.5 * height_px_) / pixels_per_unit_;
}

void Camera::fit(const Bounds& bounds, double margin_fraction) noexcept {
  if (bounds.empty()) {
    return;
  }
  center_x_ = 0.5 * static_cast<double>(bounds.x0 + bounds.x1);
  center_y_ = 0.5 * static_cast<double>(bounds.y0 + bounds.y1);
  const double usable = std::max(1.0 - 2.0 * margin_fraction, 0.1);
  const double w = std::max(static_cast<double>(bounds.width()), 1.0);
  const double h = std::max(static_cast<double>(bounds.height()), 1.0);
  pixels_per_unit_ = clamp_zoom(usable * std::min(width_px_ / w, height_px_ / h));
}

WorldWindow Camera::visible_window() const noexcept {
  return WorldWindow{world_x(0.0), world_y(static_cast<double>(height_px_)), world_x(static_cast<double>(width_px_)),
                     world_y(0.0)};
}

ViewTransform Camera::transform() const noexcept {
  ViewTransform t;
  t.origin_x = clamp_to_int32(center_x_);
  t.origin_y = clamp_to_int32(center_y_);
  const double sx = 2.0 * pixels_per_unit_ / width_px_;
  // World y grows upwards, Vulkan NDC y grows downwards.
  const double sy = -2.0 * pixels_per_unit_ / height_px_;
  t.scale_x = static_cast<float>(sx);
  t.scale_y = static_cast<float>(sy);
  t.offset_x = static_cast<float>((t.origin_x - center_x_) * sx);
  t.offset_y = static_cast<float>((t.origin_y - center_y_) * sy);
  t.min_size_x = static_cast<float>(2.0 / width_px_);
  t.min_size_y = static_cast<float>(2.0 / height_px_);
  return t;
}

}  // namespace hefesto::render
