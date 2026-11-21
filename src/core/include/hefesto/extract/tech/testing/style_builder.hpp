// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extract::testing::StyleBuilder: builds an ExtStyle fixture without a
// technology file, so the extractor, netlister, extresist and antenna
// workstreams can start before EXTTECH's reader is wired
// (openspec/changes/m7-extraction/design.md D3 "Dependencies", tasks.md
// 3.3). Header-only, for tests; production code never includes it.
//
// Owner: INTEGRATION (this header); EXTTECH may add setters as the data
// model grows.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract::testing {

class StyleBuilder {
 public:
  explicit StyleBuilder(std::string name = "default") { style_.name = std::move(name); }

  StyleBuilder& units_per_lambda(float units) {
    style_.units_per_lambda = units;
    return *this;
  }
  StyleBuilder& cscale(double value) {
    style_.cscale = value;
    return *this;
  }
  StyleBuilder& rscale(double value) {
    style_.rscale = value;
    return *this;
  }
  StyleBuilder& step(std::int32_t step_size) {
    style_.step_size = step_size;
    return *this;
  }
  StyleBuilder& side_halo(std::int32_t halo) {
    style_.side_halo = halo;
    return *this;
  }
  StyleBuilder& fringe(FringeModel model) {
    style_.fringe_model = model;
    return *this;
  }
  StyleBuilder& plane_order(std::vector<std::int32_t> order) {
    style_.plane_order = std::move(order);
    return *this;
  }
  StyleBuilder& no_plane_ordering() {
    style_.plane_ordering = false;
    return *this;
  }
  StyleBuilder& resist(ResistClass rc) {
    style_.resist_classes.push_back(std::move(rc));
    return *this;
  }
  StyleBuilder& area_cap(tech::TileType type, double cap) {
    style_.caps.area.push_back(TypeCap{type, {}, cap});
    return *this;
  }
  StyleBuilder& perimeter_cap(tech::TileType type, tech::TypeMask outside, double cap) {
    style_.caps.perimeter.push_back(TypeCap{type, outside, cap});
    return *this;
  }
  StyleBuilder& overlap(OverlapRule rule) {
    style_.caps.overlap.push_back(std::move(rule));
    return *this;
  }
  StyleBuilder& sidewall(SidewallRule rule) {
    style_.caps.sidewall.push_back(std::move(rule));
    return *this;
  }
  StyleBuilder& sideoverlap(SideOverlapRule rule) {
    style_.caps.sideoverlap.push_back(std::move(rule));
    return *this;
  }
  StyleBuilder& device(DevicePattern pattern) {
    style_.devices.push_back(std::move(pattern));
    return *this;
  }
  StyleBuilder& device_info(std::vector<DeviceInfo> table) {
    style_.set_device_info(std::move(table));
    return *this;
  }
  StyleBuilder& substrate(SubstrateDecl decl) {
    style_.substrate = std::move(decl);
    return *this;
  }
  StyleBuilder& antenna(AntennaRule rule) {
    style_.antenna.push_back(std::move(rule));
    return *this;
  }
  StyleBuilder& tiedown(tech::TypeMask types) {
    style_.tiedown = types;
    return *this;
  }
  StyleBuilder& active(tech::TypeMask types) {
    style_.active = types;
    return *this;
  }
  StyleBuilder& non_electrical(tech::TypeMask types) {
    style_.non_electrical = types;
    return *this;
  }

  [[nodiscard]] ExtStyle build() const { return style_; }
  [[nodiscard]] std::shared_ptr<const ExtStyle> shared() const { return std::make_shared<const ExtStyle>(style_); }

 private:
  ExtStyle style_;
};

}  // namespace hefesto::extract::testing
