// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The layers export writes (design D14; Q-MISC-72, Q-MISC-77): visibility,
// drawn styles, one entry per layer name, physical heights in µm and the
// warnings for layers without one.
//
// Owner: EXPORT (design D3).

#include <algorithm>
#include <bit>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/display/scene.hpp"
#include "view3d/export/model.hpp"

namespace hefesto::view3d {

namespace {

constexpr std::string_view kNoPhysicalHeight = "VIEW3D-EXPORT-003";
constexpr std::string_view kTooLarge = "VIEW3D-EXPORT-005";
constexpr std::uint64_t kMaxU32 = 0xFFFFFFFFU;

constexpr std::uint64_t padded4(std::uint64_t size) noexcept {
  return (size + 3U) & ~std::uint64_t{3U};
}

bool flagged(const std::vector<bool>& flags, std::size_t layer) {
  return layer < flags.size() && flags[layer];
}

// Excluded layers are left out whatever the visibility option (`-all`).
bool visible(const ExportOptions& options, std::size_t layer) {
  return !flagged(options.excluded, layer) && (!options.visible_only || !flagged(options.hidden, layer));
}

// Only solid and stippled styles are drawn (the renderer's rule, N3).
bool drawn(const ExportOptions& options, const StackLayer& layer) {
  if (options.styles == nullptr) {
    return true;
  }
  const display::ResolvedStyle style = display::resolve_style(*options.styles, layer.style);
  return style.fill == display::FillKind::solid || style.fill == display::FillKind::stipple;
}

std::vector<bool> layers_present(const MeshModel& mesh, std::size_t count) {
  std::vector<bool> present(count, false);
  const auto mark = [&](LayerId layer) {
    if (layer < count) {
      present[layer] = true;
    }
  };
  for (const Chunk& chunk : mesh.chunks) {
    for (const RectInstance& r : chunk.rects) {
      mark(r.layer);
    }
    for (const SideInstance& s : chunk.sides) {
      mark(s.layer);
    }
    for (const Triangle& t : chunk.triangles) {
      mark(t.layer);
    }
  }
  return present;
}

// Visible, drawn entries with geometry; of several entries of one name (a
// type in two styles, Q-MISC-77) the last, in layer order.
std::vector<LayerId> candidates(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options) {
  const std::vector<bool> present = layers_present(mesh, stack.size());
  std::map<std::string, LayerId> last;
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const StackLayer& layer = stack.layers()[i];
    if (present[i] && visible(options, i) && drawn(options, layer)) {
      last[layer.name] = static_cast<LayerId>(i);
    }
  }
  std::vector<LayerId> ids;
  ids.reserve(last.size());
  for (const auto& entry : last) {
    ids.push_back(entry.second);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

// The physical height, or the model's own h and t when they differ from the
// stack's (a contact spanned by J1).
std::optional<PhysicalHeight> height_um(const MeshModel& mesh, const LayerStack& stack, LayerId id) {
  std::optional<PhysicalHeight> physical = stack.physical_height_um(id);
  if (!physical || id >= mesh.layer_heights.size()) {
    return physical;
  }
  const DrawHeight& drawn_height = mesh.layer_heights[id];
  if (drawn_height == stack.draw_height(id)) {
    return physical;
  }
  return PhysicalHeight{detail::draw_to_um(stack, drawn_height.height),
                        detail::draw_to_um(stack, drawn_height.thickness)};
}

ExportLayer describe(const LayerStack& stack, LayerId id, const PhysicalHeight& height, const ExportOptions& options) {
  const StackLayer& layer = stack.layers()[id];
  const double k = options.zscale.value_or(1.0);
  ExportLayer out;
  out.layer = id;
  out.name = layer.name;
  out.material = layer.name;
  out.z_bottom_um = height.height_um * k;
  out.z_top_um = (height.height_um + height.thickness_um) * k;
  if (options.styles == nullptr) {
    return out;
  }
  const display::ResolvedStyle style = display::resolve_style(*options.styles, layer.style);
  out.srgb = {style.r, style.g, style.b};
  const std::vector<display::DisplayStyle>& table = options.styles->table.styles;
  if (layer.style < table.size() && table[layer.style].long_name && !table[layer.style].long_name->empty()) {
    out.material = *table[layer.style].long_name;
  }
  return out;
}

// VIEW3D-STACK-001 of a mask-layer stack. Without the output style, every
// mask layer without a `render` height is named (none is known temporary).
void warn_mask_heights(const LayerStack& stack, const ExportOptions& options, diag::DiagnosticSink& sink) {
  std::optional<diag::Diagnostic> warning;
  if (options.output_style != nullptr) {
    warning = missing_height_warning(stack, *options.output_style);
  } else {
    mask::OutputStyle named;
    for (const StackLayer& layer : stack.layers()) {
      named.layers.resize(std::max(named.layers.size(), layer.mask_layer.value_or(0) + 1));
    }
    warning = missing_height_warning(stack, named);
  }
  if (warning) {
    sink.emit(*warning);
  }
}

void warn_layout_heights(const std::vector<std::string>& names, diag::DiagnosticSink& sink) {
  if (names.empty()) {
    return;
  }
  std::string text = "No physical height for layer(s):";
  for (const std::string& name : names) {
    text += " " + name;
  }
  text += "; they are left out of the export (no extract-style height, or no µm scale without an output style)";
  sink.emit(diag::make_native(kNoPhysicalHeight, std::move(text)));
}

}  // namespace

namespace detail {

double draw_to_um(const LayerStack& stack, float value) {
  if (stack.mode() == Mode::mask_layer && stack.magic_compat()) {
    return static_cast<double>(value);
  }
  return static_cast<double>(value) * stack.microns_per_unit();
}

double xy_unit_um(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options) {
  if (options.xy_unit_um > 0.0) {
    return options.xy_unit_um;
  }
  return stack.microns_per_unit() / static_cast<double>(mesh.units_per_root > 0 ? mesh.units_per_root : 1);
}

ExportModel build_export_model(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                               diag::DiagnosticSink& sink) {
  ExportModel model;
  model.layers = export_layers(mesh, stack, options, sink);
  model.geometry = build_geometry(mesh, stack, options, model.layers);
  const double unit = detail::xy_unit_um(mesh, stack, options);
  model.origin_x_um = static_cast<double>(mesh.region.x0) * unit;
  model.origin_y_um = static_cast<double>(mesh.region.y0) * unit;
  model.zscale = options.zscale.value_or(1.0);
  return model;
}

std::string safe_json_text(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte < 0x20 || c == '"' || c == '\\' || byte == 0x7F) {
      c = '_';
    }
  }
  return out;
}

std::string safe_file_part(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    const auto byte = static_cast<unsigned char>(c);
    const bool plain = (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                       c == '_' || c == '-' || c == '.' || c == '+';
    if (!plain) {
      c = '_';
    }
  }
  return out;
}

bool glb_fits(std::size_t json_size, std::size_t bin_size) noexcept {
  constexpr std::uint64_t kHeader = 12;
  constexpr std::uint64_t kChunkHeader = 8;
  std::uint64_t total = kHeader + kChunkHeader + padded4(json_size);
  if (bin_size > 0) {
    total += kChunkHeader + padded4(bin_size);
  }
  return total <= kMaxU32;
}

bool stl_fits(std::size_t triangles) noexcept {
  return triangles <= kMaxU32;  // size_t and uint64_t are both unsigned; no cast (GCC -Wuseless-cast)
}

diag::Diagnostic too_large(std::string_view what) {
  return diag::make(kTooLarge, "export3d: the model is too large for the format (" + std::string{what} +
                                   "); nothing is written");
}

void put_u16(std::vector<std::byte>& out, std::uint16_t value) {
  out.push_back(static_cast<std::byte>(value & 0xFFU));
  out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void put_u32(std::vector<std::byte>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32U; shift += 8U) {
    out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
  }
}

void put_f32(std::vector<std::byte>& out, float value) {
  put_u32(out, std::bit_cast<std::uint32_t>(value));
}

}  // namespace detail

std::vector<ExportLayer> export_layers(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
                                       diag::DiagnosticSink& sink) {
  std::vector<ExportLayer> layers;
  std::vector<std::string> unheighted;
  for (const LayerId id : candidates(mesh, stack, options)) {
    if (const std::optional<PhysicalHeight> height = height_um(mesh, stack, id)) {
      layers.push_back(describe(stack, id, *height, options));
    } else {
      unheighted.push_back(stack.layers()[id].name);
    }
  }
  if (stack.mode() == Mode::mask_layer) {
    warn_mask_heights(stack, options, sink);
  } else {
    warn_layout_heights(unheighted, sink);
  }
  return layers;
}

}  // namespace hefesto::view3d
