// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pick ids (pick_id.hpp; design.md D10). Owner: INSPECT.

#include "hefesto/view3d/pick/pick_id.hpp"

#include <limits>

namespace hefesto::view3d {

namespace {

constexpr std::uint64_t kMaxPickId = std::numeric_limits<PickId>::max();

std::uint64_t kind_size(const Chunk& chunk, PickKind kind) noexcept {
  switch (kind) {
    case PickKind::rect:
      return chunk.rects.size();
    case PickKind::side:
      return chunk.sides.size();
    case PickKind::triangle:
      return chunk.triangles.size();
  }
  return 0;
}

}  // namespace

std::uint64_t pick_instance_count(const Chunk& chunk) noexcept {
  return chunk.rects.size() + chunk.sides.size() + chunk.triangles.size();
}

std::uint64_t pick_local_index(const Chunk& chunk, PickKind kind, std::size_t index) noexcept {
  switch (kind) {
    case PickKind::rect:
      return index;
    case PickKind::side:
      return chunk.rects.size() + index;
    case PickKind::triangle:
      return chunk.rects.size() + chunk.sides.size() + index;
  }
  return index;
}

PickId encode_pick(const MeshModel& mesh, const PickRef& ref) noexcept {
  if (ref.chunk >= mesh.chunks.size() || ref.index >= kind_size(mesh.chunks[ref.chunk], ref.kind)) {
    return kNoPick;
  }
  std::uint64_t id = 1;
  for (std::size_t c = 0; c < ref.chunk; ++c) {
    id += pick_instance_count(mesh.chunks[c]);
  }
  id += pick_local_index(mesh.chunks[ref.chunk], ref.kind, ref.index);
  return id > kMaxPickId ? kNoPick : static_cast<PickId>(id);
}

std::optional<PickRef> decode_pick(const MeshModel& mesh, PickId id) noexcept {
  if (id == kNoPick) {
    return std::nullopt;
  }
  std::uint64_t rest = id - 1U;
  for (std::size_t c = 0; c < mesh.chunks.size(); ++c) {
    const Chunk& chunk = mesh.chunks[c];
    if (rest >= pick_instance_count(chunk)) {
      rest -= pick_instance_count(chunk);
      continue;
    }
    for (const PickKind kind : {PickKind::rect, PickKind::side, PickKind::triangle}) {
      if (rest < kind_size(chunk, kind)) {
        return PickRef{c, kind, static_cast<std::size_t>(rest)};
      }
      rest -= kind_size(chunk, kind);
    }
  }
  return std::nullopt;
}

}  // namespace hefesto::view3d
