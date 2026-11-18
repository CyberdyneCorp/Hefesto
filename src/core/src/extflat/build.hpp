// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record builders of EFbuild.c that the subset needs (capability
// ext-flattening-read "Node and substrate records", "Equivalence, merge and
// kill records", "Use, port, cap, resist, attr and connect records", and
// the node side of "Device records"; ext-flattening-hierarchy "Node merge
// semantics"). Shorts are `none` and resistance overlays are off, as for
// `def write`.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

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
};

/// efBuildNode.
void build_node(ReadState& st, Def& def, const NodeRecord& record);
/// efBuildPortNode.
void build_port_node(ReadState& st, Def& def, std::string_view name, std::int32_t index, std::int32_t x,
                     std::int32_t y, std::string_view layer, bool toplevel);
/// efBuildEquiv (shorts `none`, no overlay, not SPICE).
void build_equiv(ReadState& st, Def& def, std::string_view name1, std::string_view name2);
/// efBuildMerge (shorts `none`): records the connection.
void build_merge(ReadState& st, Def& def, std::string_view name1, std::string_view name2);
/// efConnInitSubs for `cap` and `resist` records: their names are checked
/// (with the messages) but nothing is kept.
void check_connection(ReadState& st, std::string_view name1, std::string_view name2);
/// efBuildUse.
void build_use(ReadState& st, Def& def, std::string_view def_name, std::string_view use_id, const Transform& t);
/// The `device` and `fet` records: efBuildDevice's node side. `args` are
/// the tokens after the coordinates; returns false for "Incomplete
/// terminal description".
[[nodiscard]] bool build_device(ReadState& st, Def& def, std::uint8_t device_class, std::string_view type,
                                db::Point ll, std::span<const std::string> args);
/// A `device` record of type `Short`: an `equiv` of two terminal names.
void build_short(ReadState& st, Def& def, std::span<const std::string> args);
/// efBuildDeviceParams: only the messages.
void build_device_params(ReadState& st, Def& def, std::string_view type, std::span<const std::string> args);

/// extDevTable: the class index of a `device` record's class word.
[[nodiscard]] std::optional<std::uint8_t> device_class(std::string_view word) noexcept;
inline constexpr std::uint8_t kDevFet = 0;

/// (int)(0.5 + (float)v * locScale), as the reader scales coordinates.
[[nodiscard]] std::int32_t scale_coord(const ReadState& st, std::int32_t v) noexcept;

}  // namespace hefesto::extflat::detail
