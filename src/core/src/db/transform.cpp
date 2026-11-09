// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Positions, angles, orientations and position names under Manhattan
// transforms (layout-geometry-model "Manhattan transforms", "Transforming
// positions, angles and triangles").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) utils/geometry.c: GeoTransPos,
// GeoTransAngle, GeoTransOrient, GeoDecomposeTransform, GeoNameToPos (with
// utils/lookup.c LookupStruct). Magic is Copyright (C) 1985, 1990 Regents of
// the University of California (permissive license, THIRD_PARTY_NOTICES.md).

#include "hefesto/db/transform.hpp"

#include <array>
#include <cctype>

namespace hefesto::db {

namespace {

constexpr Transform kUpsideDown{1, 0, 0, 0, -1, 0};

struct PositionName {
  std::string_view name;
  Position value;
  bool manhattan;
};

constexpr std::array<PositionName, 33> kPositionNames = {{
    {"bl", kSouthWest, false},  {"bottom", kSouth, true},      {"br", kSouthEast, false},
    {"center", kCenter, false}, {"d", kSouth, true},           {"dl", kSouthWest, false},
    {"down", kSouth, true},     {"dr", kSouthEast, false},     {"e", kEast, true},
    {"east", kEast, true},      {"left", kWest, true},         {"n", kNorth, true},
    {"ne", kNorthEast, false},  {"north", kNorth, true},       {"northeast", kNorthEast, false},
    {"northwest", kNorthWest, false}, {"nw", kNorthWest, false}, {"right", kEast, true},
    {"s", kSouth, true},        {"se", kSouthEast, false},     {"south", kSouth, true},
    {"southeast", kSouthEast, false}, {"southwest", kSouthWest, false}, {"sw", kSouthWest, false},
    {"tl", kNorthWest, false},  {"top", kNorth, true},         {"tr", kNorthEast, false},
    {"u", kNorth, true},        {"ul", kNorthWest, false},     {"up", kNorth, true},
    {"ur", kNorthEast, false},  {"w", kWest, true},            {"west", kWest, true},
}};

// LookupStruct character match: equal, or equal ignoring case.
bool chars_match(char table, char input) noexcept {
  const auto t = static_cast<unsigned char>(table);
  const auto i = static_cast<unsigned char>(input);
  return t == i || std::tolower(t) == std::tolower(i);
}

// LookupStruct: index of the exact or unique-prefix match, -1 ambiguous, -2 none.
int lookup_position(std::string_view str) noexcept {
  int match = -2;
  for (std::size_t pos = 0; pos < kPositionNames.size(); ++pos) {
    const std::string_view entry = kPositionNames[pos].name;
    std::size_t n = 0;
    while (n < str.size() && n < entry.size() && chars_match(entry[n], str[n])) {
      ++n;
    }
    if (n != str.size()) {
      continue;
    }
    if (n == entry.size()) {
      return static_cast<int>(pos);
    }
    match = match == -2 ? static_cast<int>(pos) : -1;
  }
  return match;
}

}  // namespace

Position transform_position(const Transform& t, Position pos) noexcept {
  if (pos <= 0 || pos > 8) {
    return pos;
  }
  pos -= 1;
  if (t.a < 0) {
    pos += 4;
  } else if (t.a == 0) {
    pos += t.b < 0 ? 6 : 2;
  }
  pos = pos % 8 + 1;
  if (t.a != t.e || (t.a == 0 && t.b == t.d)) {
    constexpr std::array<Position, 9> kMirror = {kCenter, kSouth, kSouthEast, kEast, kNorthEast,
                                                 kNorth,  kNorthWest, kWest, kSouthWest};
    pos = kMirror[static_cast<std::size_t>(pos)];
  }
  return pos;
}

Position opposite_position(Position pos) noexcept {
  if (pos <= 0 || pos > 8) {
    return pos;
  }
  return (pos + 3) % 8 + 1;
}

int parse_position(std::string_view name, bool manhattan) noexcept {
  const int index = lookup_position(name);
  if (index < 0) {
    return index;
  }
  const PositionName& entry = kPositionNames[static_cast<std::size_t>(index)];
  return !manhattan || entry.manhattan ? entry.value : -2;
}

namespace {

struct AngleStep {
  std::int32_t add = 0;
  bool flip = false;
};

// The rotation GeoTransAngle adds and whether the transform flips.
AngleStep angle_step(const Transform& t, bool magic_compat) noexcept {
  if (t.a == 0 && t.e == 0) {
    const bool flip = t.b == t.d;
    if (t.b <= 0) {
      return {270, flip};
    }
    return {(magic_compat || flip) ? 90 : -90, flip};  // Q-MODEL-1: only the clockwise-90 rotation
  }
  return {t.a < 0 ? 180 : 0, t.a != t.e};
}

}  // namespace

std::int32_t transform_angle(const Transform& t, std::int32_t degrees, bool magic_compat) noexcept {
  const AngleStep step = angle_step(t, magic_compat);
  std::int32_t a = degrees + step.add;
  if (magic_compat ? a > 360 : a >= 360) {  // Q-MODEL-2
    a -= 360;
  }
  if (step.flip) {
    a = (degrees > 90 && degrees < 270) ? 360 - a : -a;
  }
  return a < 0 ? a + 360 : a;
}

Orientation orientation(const Transform& t) noexcept {
  if (t.b == 0 && t.d == 0) {
    constexpr std::array<Orientation, 4> kTable = {Orientation::S, Orientation::FS, Orientation::FN, Orientation::N};
    return kTable[static_cast<std::size_t>((t.a > 0 ? 1 : 0) + (t.e > 0 ? 2 : 0))];
  }
  if (t.a == 0 && t.e == 0) {
    constexpr std::array<Orientation, 4> kTable = {Orientation::FE, Orientation::E, Orientation::W, Orientation::FW};
    return kTable[static_cast<std::size_t>((t.b > 0 ? 1 : 0) + (t.d > 0 ? 2 : 0))];
  }
  return Orientation::N;
}

std::string_view orientation_name(Orientation o) noexcept {
  constexpr std::array<std::string_view, 8> kNames = {"N", "S", "E", "W", "FN", "FS", "FE", "FW"};
  return kNames[static_cast<std::size_t>(o)];
}

RotationDecomposition decompose(const Transform& t) noexcept {
  const Transform notrans{t.a, t.b, 0, t.d, t.e, 0};
  RotationDecomposition r;
  r.upside_down = ((notrans.a == 0) != (notrans.b == notrans.d)) != (notrans.a == notrans.e);
  const Transform rotonly = r.upside_down ? compose(kUpsideDown, notrans) : notrans;
  if (rotonly.b != 0) {
    r.angle += r.upside_down ? 270 : 90;
  }
  if (rotonly.a < 0 || rotonly.b < 0) {
    r.angle += 180;
  }
  if (r.angle > 270) {
    r.angle -= 360;
  }
  return r;
}

}  // namespace hefesto::db
