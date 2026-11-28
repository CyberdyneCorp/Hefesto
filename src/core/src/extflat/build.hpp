// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record builders of EFbuild.c (capability ext-flattening-read "Node
// and substrate records", "Device records", "Parameters records",
// "Equivalence, merge and kill records", "Use, port, cap, resist, attr and
// connect records"; ext-flattening-hierarchy "Node merge semantics").
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "state.hpp"

namespace hefesto::extflat::detail {

/// How efBuildNode was called.
struct NodeRecord {
  std::string_view name;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::optional<std::string_view> layer;  // nullopt: no layer (type 0)
  bool substrate = false;      // isSubsnode: the `substrate` record
  bool device_substrate = false;  // isDevSubsnode
  bool ext_node = false;       // isExtNode: a `node`/`substrate` record of the root file
  float cap = 0.0F;            // nodeCap (aF, already scaled)
  std::span<const std::string_view> area_perim{};  // the area/perimeter tokens (av, ac)
};

/// efBuildNode.
void build_node(ReadState& st, Def& def, const NodeRecord& record);
/// efAdjustSubCap.
void adjust_subcap(ReadState& st, Def& def, std::string_view name, float cap);
/// efBuildAttr.
void build_attr(ReadState& st, Def& def, std::string_view name, const db::Rect& r, std::string_view layer,
                std::string_view text);
/// efBuildPortNode.
void build_port_node(ReadState& st, Def& def, std::string_view name, std::int32_t index, std::int32_t x,
                     std::int32_t y, std::string_view layer, bool toplevel);
/// efBuildEquiv; `resist` is EFReadFile's resist for an `equiv` record and
/// false for a `Short` device or a deferred equiv.
void build_equiv(ReadState& st, Def& def, std::string_view name1, std::string_view name2, bool resist);
/// efBuildMerge: records the connection with its capacitance delta (aF,
/// scaled) and the area/perimeter tokens.
void build_merge(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float cap,
                 std::span<const std::string_view> area_perim);
/// efBuildCap: a coupling of `cap` aF (scaled).
void build_cap(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float cap);
/// efBuildResistor: a resistor of `res` mohm.
void build_resistor(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float res);
/// efBuildUse.
void build_use(ReadState& st, Def& def, std::string_view def_name, std::string_view use_id, const Transform& t);
/// efBuildDevice: `args` are the tokens after the coordinates; returns
/// false for "Incomplete terminal description".
[[nodiscard]] bool build_device(ReadState& st, Def& def, std::uint8_t device_class, std::string_view type,
                                const db::Rect& r, std::span<const std::string_view> args);
/// A `device` record of type `Short`: an `equiv` of two terminal names.
void build_short(ReadState& st, Def& def, std::span<const std::string_view> args);
/// efBuildDeviceParams.
void build_device_params(ReadState& st, std::string_view type, std::span<const std::string_view> args);

/// extDevTable: the class index of a `device` record's class word.
[[nodiscard]] std::optional<std::uint8_t> device_class(std::string_view word) noexcept;
/// The class word of a class index.
[[nodiscard]] std::string_view device_class_name(std::uint8_t cls) noexcept;
inline constexpr std::uint8_t kDevFet = 0;
inline constexpr std::uint8_t kDevVolt = 7;

/// (int)(0.5 + (float)v * locScale), as the reader scales coordinates.
[[nodiscard]] std::int32_t scale_coord(const ReadState& st, std::int32_t v) noexcept;

}  // namespace hefesto::extflat::detail
