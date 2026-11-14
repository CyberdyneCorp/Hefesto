// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label layers and label TEXT records (capability gds-write "Label layers",
// "Label TEXT records"). Ported from Magic 8.3.694 calma/CalmaWrite.c:
// 1514-1606 (the label part of calmaOutFunc), 2829-2984
// (calmaWriteLabelFunc) and 3000-3048 (calmaPaintLabelFunc); Magic is (c)
// the Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/write/labels_text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include "hefesto/gds/write/labels_sort.hpp"
#include "hefesto/gds/write/merge_plane.hpp"

namespace hefesto::gds {

namespace {

constexpr std::int32_t kCalmaLayerMax = 255;  // CALMA_LAYER_MAX

/// The style layer at `index` when it is written to GDS (CalmaIsValidLayer
/// of its calma number; no calma statement is Magic's -1).
const mask::OutputLayer* gds_layer(const mask::OutputStyle& style, std::int32_t index) noexcept {
  if (index < 0 || static_cast<std::size_t>(index) >= style.layers.size()) {
    return nullptr;
  }
  const mask::OutputLayer& layer = style.layers[static_cast<std::size_t>(index)];
  if (!layer.gds || layer.gds->layer < 0 || layer.gds->layer > kCalmaLayerMax) {
    return nullptr;
  }
  return &layer;
}

/// A run of records that stops at the first failure.
class Records {
 public:
  explicit Records(RecordWriter& out) : out_(out) {}

  Records& empty(RecordType type) {
    if (status_) {
      status_ = out_.write_empty(type);
    }
    return *this;
  }
  Records& int2(RecordType type, std::int32_t value) {
    const std::array<std::int16_t, 1> word{static_cast<std::int16_t>(value)};  // calmaOutI2: low 16 bits
    if (status_) {
      status_ = out_.write_int2(type, word);
    }
    return *this;
  }
  Records& bits(RecordType type, std::uint16_t value) {
    const std::array<std::byte, 2> word{static_cast<std::byte>(value >> 8U), static_cast<std::byte>(value & 0xffU)};
    if (status_) {
      status_ = out_.write(type, DataType::bitarray, word);
    }
    return *this;
  }
  Records& real8(RecordType type, double value) {
    const std::array<double, 1> word{value};
    if (status_) {
      status_ = out_.write_real8(type, word);
    }
    return *this;
  }
  Records& point(std::int64_t x, std::int64_t y) {
    const std::array<std::int64_t, 2> xy{x, y};
    if (status_) {
      status_ = write_xy(out_, xy);
    }
    return *this;
  }
  Records& string(const LabelOutput& output, std::string_view text) {
    if (status_) {
      status_ = output.string_record ? output.string_record(out_, RecordType::string, text)
                                     : out_.write_string(RecordType::string, text);
    }
    return *this;
  }
  Records& fail(diag::Diagnostic error) {
    if (status_) {
      status_ = std::move(error);
    }
    return *this;
  }

  [[nodiscard]] diag::Status status() && { return std::move(status_); }

 private:
  RecordWriter& out_;
  diag::Status status_ = diag::Done{};
};

/// (lo + hi) * scale / 2 with C truncation, in 64-bit; nullopt on overflow.
std::optional<std::int64_t> scaled_center(db::Coord lo, db::Coord hi, std::int64_t scale) noexcept {
  std::int64_t product = 0;
  if (__builtin_mul_overflow(std::int64_t{lo} + hi, scale, &product)) {
    return std::nullopt;
  }
  return product / 2;
}

diag::Diagnostic overflow_error() {
  return diag::make("GDS-WRITE-201", "A GDS label coordinate does not fit a 4-byte integer; the write fails.");
}

/// The text element's centre point, or an overflow error.
Records& center_point(Records& records, const db::Rect& r, std::int64_t scale) {
  const std::optional<std::int64_t> x = scaled_center(r.x0, r.x1, scale);
  const std::optional<std::int64_t> y = scaled_center(r.y0, r.y1, scale);
  if (!x || !y) {
    return records.fail(overflow_error());
  }
  return records.point(*x, *y);
}

/// PRESENTATION, STRANS, MAG and ANGLE of a font label.
void font_records(Records& records, const LabelOutput& output, const db::Label& label) {
  records.bits(RecordType::presentation, text_presentation(label.font, label.justification))
      .bits(RecordType::strans, 0)
      .real8(RecordType::mag, text_magnification(label.size, output.style.scale, output.magscale, output.compat));
  if (label.rotation != 0) {
    records.real8(RecordType::angle, static_cast<double>(label.rotation));
  }
}

/// The label rectangle on an operator-less rectangle layer.
diag::Status label_rectangle(const LabelOutput& output, const db::Label& label, std::int32_t rect_layer) {
  const mask::OutputLayer* layer = gds_layer(output.style, rect_layer);
  const db::Rect& r = label.rect;
  if (layer == nullptr || !layer->ops.empty() || r.x1 <= r.x0 || r.y1 <= r.y0) {
    return diag::Done{};
  }
  const std::array<db::Point, 4> corners{{{r.x0, r.y0}, {r.x1, r.y0}, {r.x1, r.y1}, {r.x0, r.y1}}};
  return write_boundary(output.out, layer->gds->layer, layer->gds->datatype, corners, output.write_scale);
}

std::string type_label(const LabelOutput& output, tech::TileType type) {
  if (output.technology != nullptr) {
    return std::string(output.technology->type_name(type));
  }
  return std::to_string(type);
}

/// GDS-WRITE-202 once per type and write (Q-MASK-206).
void warn_port_layer(const LabelOutput& output, tech::TileType type, bool written) {
  if (output.sink == nullptr || (output.warned != nullptr && output.warned->test(type))) {
    return;
  }
  if (output.warned != nullptr) {
    output.warned->set(type);
  }
  const std::string name = type_label(output, type);
  const std::string what = written ? "written on their text layer" : "not written";
  output.sink->emit(diag::make_native("GDS-WRITE-202", "Port labels of type " + name + " are " + what +
                                                           ": output style \"" + output.style.name +
                                                           "\" has no \"labels " + name + " port\" layer."));
}

}  // namespace

std::uint16_t text_presentation(std::int32_t font, db::Position justification) noexcept {
  // Indexed by Magic's GEO_* positions: center, N, NE, E, SE, S, SW, W, NW.
  constexpr std::array<std::uint16_t, 9> kJustify{0x5, 0x9, 0x8, 0x4, 0x0, 0x1, 0x2, 0x6, 0xa};
  auto word = static_cast<std::uint16_t>((static_cast<std::uint32_t>(font) & 0x3U) << 4U);
  if (justification >= 0 && static_cast<std::size_t>(justification) < kJustify.size()) {
    word = static_cast<std::uint16_t>(word | kJustify[static_cast<std::size_t>(justification)]);
  }
  return word;
}

double text_magnification(std::int32_t size, const mask::StyleScale& scale, double magscale,
                          const compat::CompatSettings& compat) noexcept {
  double mag = (static_cast<double>(size) / 800) * static_cast<double>(scale.scale_factor) /
               static_cast<double>(scale.expander);
  if (!compat.magic_compat && std::isfinite(magscale) && magscale > 0) {
    mag /= magscale;
  }
  return mag;
}

LabelLayers plain_label_layers(const mask::OutputStyle& style, tech::TileType type) noexcept {
  if (type >= tech::kMaxTileTypes) {
    return {};
  }
  const std::int32_t layer = style.label_layer[type];
  return {layer, layer};
}

std::optional<LabelLayers> port_label_layers(const mask::OutputStyle& style, tech::TileType type,
                                             const compat::CompatSettings& compat) noexcept {
  if (type >= tech::kMaxTileTypes) {
    return std::nullopt;
  }
  const std::int32_t text = style.port_text[type];
  const std::int32_t rect = style.port_layer[type];
  if (rect >= 0) {
    return LabelLayers{text, rect};
  }
  if (!compat.magic_compat && text >= 0) {
    return LabelLayers{text, text};
  }
  return std::nullopt;
}

diag::Status emit_label(const LabelOutput& output, const db::Label& label, LabelLayers layers) {
  const mask::OutputLayer* text_layer = gds_layer(output.style, layers.text);
  if (text_layer == nullptr) {
    return diag::Done{};
  }
  Records records(output.out);
  records.empty(RecordType::text)
      .int2(RecordType::layer, text_layer->gds->layer)
      .int2(RecordType::texttype, text_layer->gds->datatype);
  if (label.font >= 0) {
    font_records(records, output, label);
  }
  center_point(records, label.rect, output.write_scale).string(output, label.text).empty(RecordType::endel);
  diag::Status status = std::move(records).status();
  if (!status) {
    return status;
  }
  return label_rectangle(output, label, layers.rect);
}

diag::Status emit_cell_labels(const LabelOutput& output, std::span<const db::Label> labels) {
  for (const db::Label& label : labels) {
    if (label.is_port()) {
      continue;
    }
    if (diag::Status status = emit_label(output, label, plain_label_layers(output.style, label.type)); !status) {
      return status;
    }
  }
  for (const std::size_t index : port_label_order(labels)) {
    const db::Label& label = labels[index];
    const std::optional<LabelLayers> layers = port_label_layers(output.style, label.type, output.compat);
    if (label.type < tech::kMaxTileTypes && output.style.port_layer[label.type] < 0) {
      warn_port_layer(output, label.type, layers.has_value());
    }
    if (!layers) {
      continue;
    }
    if (diag::Status status = emit_label(output, label, *layers); !status) {
      return status;
    }
  }
  return diag::Done{};
}

diag::Status emit_label_layer(const LabelOutput& output, std::size_t layer_index, const mask::MaskPlane& plane,
                              const std::optional<db::Rect>& clip) {
  if (layer_index >= output.style.layers.size()) {
    return diag::Done{};
  }
  const mask::OutputLayer* layer = gds_layer(output.style, static_cast<std::int32_t>(layer_index));
  if (layer == nullptr) {
    return diag::Done{};
  }
  diag::Status status = diag::Done{};
  const db::Rect area = clip.value_or(db::whole_plane());
  plane.plane().enumerate(area, tech::TypeMask::of(mask::kMaskSolid), [&](const db::TileView& t) {
    if (t.split) {
      return true;  // non-Manhattan tiles are ignored
    }
    db::Rect r = t.area;
    if (clip) {
      r = db::Rect{std::max(r.x0, clip->x0), std::max(r.y0, clip->y0), std::min(r.x1, clip->x1),
                   std::min(r.y1, clip->y1)};
    }
    Records records(output.out);
    records.empty(RecordType::text).int2(RecordType::layer, layer->gds->layer).int2(RecordType::texttype,
                                                                                    layer->gds->datatype);
    center_point(records, r, output.paint_scale).string(output, layer->name).empty(RecordType::endel);
    status = std::move(records).status();
    return status.ok();
  });
  return status;
}

}  // namespace hefesto::gds
