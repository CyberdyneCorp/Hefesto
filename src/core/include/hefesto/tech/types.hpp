// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tile types, planes and their masks: the vocabulary shared by the technology
// database (TECH), the layout database (MODEL), the cell I/O (CELL) and the
// viewer (VIEWER). Capability tech-layers-and-planes, requirements "Built-in
// planes" and "Built-in tile types".
// (Magic ref: database/database.h.in:46,85-143,191-205)
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace hefesto::tech {

/// A tile type number, 0..255. Types 0..8 are built in, technology types
/// start at 9, generated stacked-contact types follow the user types.
using TileType = std::uint16_t;

/// A plane number, 0..63. Planes 0..5 are built in, technology planes start at 6.
using PlaneId = std::uint8_t;

inline constexpr std::size_t kMaxTileTypes = 256;  // TT_MAXTYPES: codes 0..255
inline constexpr std::size_t kUsableTileTypes = 254;  // the last 2 codes are reserved
inline constexpr std::size_t kMaxPlanes = 64;  // PL_MAXTYPES, built-in planes included

// Built-in tile types (tech-layers-and-planes, "Built-in tile types").
inline constexpr TileType kSpace = 0;
inline constexpr TileType kCheckPaint = 1;
inline constexpr TileType kCheckSubcell = 2;
inline constexpr TileType kErrorP = 3;
inline constexpr TileType kErrorS = 4;
inline constexpr TileType kErrorPS = 5;
inline constexpr TileType kMagnet = 6;
inline constexpr TileType kFence = 7;
inline constexpr TileType kRotate = 8;
inline constexpr TileType kFirstUserType = 9;

// Built-in planes (tech-layers-and-planes, "Built-in planes").
inline constexpr PlaneId kPlaneRouter = 0;
inline constexpr PlaneId kPlaneDrcCheck = 1;
inline constexpr PlaneId kPlaneDrcError = 2;
inline constexpr PlaneId kPlaneMHint = 3;
inline constexpr PlaneId kPlaneFHint = 4;
inline constexpr PlaneId kPlaneRHint = 5;
inline constexpr PlaneId kFirstUserPlane = 6;

/// A set of tile types (TileTypeBitMask).
class TypeMask {
 public:
  constexpr TypeMask() noexcept = default;

  [[nodiscard]] static TypeMask of(TileType type) noexcept {
    TypeMask mask;
    mask.set(type);
    return mask;
  }
  [[nodiscard]] static TypeMask all() noexcept {
    TypeMask mask;
    mask.bits_.set();
    return mask;
  }

  void set(TileType type, bool value = true) noexcept { bits_.set(type, value); }
  void clear_type(TileType type) noexcept { bits_.reset(type); }
  [[nodiscard]] bool test(TileType type) const noexcept { return type < kMaxTileTypes && bits_.test(type); }
  [[nodiscard]] bool empty() const noexcept { return bits_.none(); }
  [[nodiscard]] std::size_t count() const noexcept { return bits_.count(); }
  /// Whether every type of `other` is in this set.
  [[nodiscard]] bool contains(const TypeMask& other) const noexcept { return (other.bits_ & ~bits_).none(); }
  /// Whether the two sets share a type.
  [[nodiscard]] bool intersects(const TypeMask& other) const noexcept { return (other.bits_ & bits_).any(); }

  TypeMask& operator|=(const TypeMask& other) noexcept {
    bits_ |= other.bits_;
    return *this;
  }
  TypeMask& operator&=(const TypeMask& other) noexcept {
    bits_ &= other.bits_;
    return *this;
  }
  [[nodiscard]] TypeMask operator~() const noexcept {
    TypeMask mask;
    mask.bits_ = ~bits_;
    return mask;
  }
  friend TypeMask operator|(TypeMask a, const TypeMask& b) noexcept { return a |= b; }
  friend TypeMask operator&(TypeMask a, const TypeMask& b) noexcept { return a &= b; }
  friend bool operator==(const TypeMask&, const TypeMask&) = default;

 private:
  std::bitset<kMaxTileTypes> bits_;
};

/// A set of planes (PlaneMask): bit p is plane p.
using PlaneMask = std::uint64_t;

[[nodiscard]] constexpr PlaneMask plane_bit(PlaneId plane) noexcept {
  return PlaneMask{1} << plane;
}

/// Long or short form of a plane or type name ("Short and long name reporting").
enum class NameForm : std::uint8_t { long_name, short_name };

}  // namespace hefesto::tech
