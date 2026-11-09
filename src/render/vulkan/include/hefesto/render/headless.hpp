// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A Vulkan device of its own and an offscreen color target, for the canvas
// benchmark and the renderer's GPU tests. No window and no display: on macOS
// it runs through MoltenVK, on Linux on any Vulkan driver (including lavapipe).
//
// This is also the "texture mode" path of design.md D9: a frame rendered into
// an image that a UI could then composite.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "hefesto/render/camera.hpp"
#include "hefesto/render/renderer.hpp"

namespace hefesto::render {

struct HeadlessOptions {
  int width = 1920;
  int height = 1080;
  bool enable_validation = false;
};

/// CPU and GPU time of one offscreen frame.
struct FrameTiming {
  /// Record + submit + wait for completion, on the calling thread.
  double cpu_ms = 0.0;
  /// From GPU timestamps around the render pass; empty when the device has
  /// no timestamp support on its graphics queue.
  std::optional<double> gpu_ms;
};

/// Device-memory heap usage, from VK_EXT_memory_budget when available.
struct MemoryBudget {
  bool available = false;
  std::uint64_t device_local_usage = 0;
  std::uint64_t device_local_budget = 0;
};

class HeadlessCanvas {
 public:
  /// Creates the instance, picks a device (discrete first), creates the
  /// device, the offscreen target and a CanvasRenderer on it.
  /// Throws VulkanError when no Vulkan device can be created.
  explicit HeadlessCanvas(const HeadlessOptions& options = {});
  ~HeadlessCanvas();
  HeadlessCanvas(const HeadlessCanvas&) = delete;
  HeadlessCanvas& operator=(const HeadlessCanvas&) = delete;

  [[nodiscard]] CanvasRenderer& renderer() noexcept;
  [[nodiscard]] int width() const noexcept;
  [[nodiscard]] int height() const noexcept;

  /// Clears the target, draws `draws` of the uploaded instances with `view`
  /// and waits for the GPU. The CPU time includes recording and submission.
  FrameTiming render(const ViewTransform& view, std::span<const LayerBatch> draws,
                     std::span<const LayerBatch> polygon_draws = {});

  /// Reads back the RGBA8 pixel at (x, y) of the last frame (tests only; slow).
  [[nodiscard]] std::uint32_t read_pixel(int x, int y);

  [[nodiscard]] std::string device_name() const;
  [[nodiscard]] std::string driver_info() const;
  /// Vulkan version of the device, e.g. "1.2.296".
  [[nodiscard]] std::string api_version() const;
  [[nodiscard]] MemoryBudget memory_budget() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::render
