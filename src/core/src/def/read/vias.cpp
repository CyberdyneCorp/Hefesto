// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefReadVias (capability def-read "VIAS section grammar", "Rectangle-based
// DEF vias", "Generated DEF vias", "Via redefinition"): each `- <name>`
// starts a via in the session's via table (lef::begin_via), RECT adds a
// rectangle (lef::add_via_rect), and CUTSIZE, LAYERS, CUTSPACING,
// ENCLOSURE and ROWCOL make the via generated (lef::ViaGenerator).
// - Q-LEFDEF-60: with magic-compat ON the generated flag lasts for the
//   section and the parameters keep their last value; with it OFF both
//   reset at each via.
// - Q-LEFDEF-61 (fix): a parameter never given takes the VIARULE value or 0
//   with a warning (lef::complete_params).
// - Q-LEFDEF-62: ORIGIN, OFFSET, PATTERN, POLYGON and MASK are unknown
//   properties with magic-compat ON, and DEF 5.8 properties with it OFF.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:1894-2156, Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <array>
#include <string>

#include "def/read/reader.hpp"
#include "hefesto/lef/read/units.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "hefesto/lef/via/store.hpp"

namespace hefesto::def::detail {

namespace {

enum Property : std::size_t {
  kRect,
  kViaRule,
  kCutSize,
  kLayers,
  kCutSpacing,
  kEnclosure,
  kRowCol,
  kOrigin,   // magic-compat OFF from here (Q-LEFDEF-62)
  kOffset,
  kPattern,
  kPolygon,
  kMask,
};

constexpr std::array<std::string_view, 12> kProperties{
    "RECT", "VIARULE", "CUTSIZE", "LAYERS", "CUTSPACING", "ENCLOSURE", "ROWCOL",
    "ORIGIN", "OFFSET", "PATTERN", "POLYGON", "MASK",
};

constexpr std::size_t kMagicProperties = kOrigin;

constexpr std::array<std::string_view, 2> kStatementKeys{"-", "END"};

// `r` moved by `by` half units, saturating.
db::Rect shifted(const db::Rect& r, db::Point by) {
  const auto shift = [](db::Coord v, db::Coord d) {
    return static_cast<db::Coord>(std::clamp<std::int64_t>(std::int64_t{v} + d, db::kPlaneMin, db::kPlaneMax));
  };
  return db::Rect{shift(r.x0, by.x), shift(r.y0, by.y), shift(r.x1, by.x), shift(r.y1, by.y)};
}

}  // namespace

// The parse of one via definition.
class ViaDefinitionReader {
 public:
  ViaDefinitionReader(Reader& reader, lef::ViaDefinition& via, std::string name, lef::ViaParamSet& params,
                      bool& generated)
      : r_(reader), via_(via), name_(std::move(name)), params_(params), generated_(generated) {}

  void run() {
    while (const auto token = r_.next()) {
      if (token->starts_with(';')) {
        finish();
        return;
      }
      if (token->starts_with('+')) {
        property(r_.take());
      }
    }
  }

 private:
  void property(const std::string& keyword) {
    const auto key = lef::lookup_full(keyword, kProperties);
    if (!key || (*key >= kMagicProperties && r_.magic_compat())) {
      r_.message("Unknown via property \"" + keyword + "\" in VIAS definition; ignoring.");
      return;
    }
    if (*key == kRect) {
      rect();
    } else if (*key == kViaRule) {
      rule_ = r_.take();
    } else if (*key == kLayers) {
      layers();
    } else if (*key < kMagicProperties) {
      dimensions(static_cast<Property>(*key));
    } else {
      extension(static_cast<Property>(*key));
    }
  }

  // + RECT <layer> <pt> <pt> (LefAddViaGeometry).
  void rect() {
    const auto layer = r_.read_layer(false);
    bool semicolon = false;
    const auto area = r_.read_rect(layer.has_value(), r_.oscale() / 2.0F, semicolon);
    if (area) {
      lef::add_via_rect(via_, layer, *area, r_.via_context());
    }
  }

  void layers() {
    lef::ViaLayers layers;
    layers.bottom = r_.read_layer(false);
    layers.cut = r_.read_layer(false);
    layers.top = r_.read_layer(false);
    lef::order_layers(layers, r_.via_context());
    params_.layers = layers;
    generated_ = true;
  }

  // An integer of `keyword`; nullopt after "Invalid syntax for <keyword>.".
  std::optional<std::int32_t> integer(std::string_view keyword) {
    const auto value = parse_int(r_.take());
    if (!value) {
      r_.error("Invalid syntax for " + std::string{keyword} + ".");
    }
    return value;
  }

  // Two integers into `point`; a value that does not parse keeps its last
  // value (0 when it has none: Magic's is uninitialised).
  void pair(std::string_view keyword, std::optional<db::Point>& point) {
    db::Point p = point.value_or(db::Point{});
    p.x = integer(keyword).value_or(p.x);
    p.y = integer(keyword).value_or(p.y);
    point = p;
  }

  void dimensions(Property key) {
    const std::string_view keyword = kProperties[key];
    if (key == kCutSize) {
      pair(keyword, params_.cut_size);
    } else if (key == kCutSpacing) {
      pair(keyword, params_.cut_spacing);
    } else if (key == kEnclosure) {
      pair(keyword, params_.bottom_enclosure);
      pair(keyword, params_.top_enclosure);
    } else {
      params_.rows = integer(keyword).value_or(1);
      params_.columns = integer(keyword).value_or(1);
    }
    generated_ = true;
  }

  // ORIGIN, OFFSET, PATTERN, POLYGON and MASK (magic-compat OFF, DEF 5.8).
  void extension(Property key) {
    const std::string_view keyword = kProperties[key];
    std::optional<db::Point> point;
    if (key == kOrigin) {
      pair(keyword, point);
      origin_ = *point;
    } else if (key == kOffset) {
      pair(keyword, point);
      params_.bottom_offset = *point;
      pair(keyword, point);
      params_.top_offset = *point;
    } else if (key == kPattern) {
      params_.pattern = r_.take();
    } else if (key == kPolygon) {
      polygon();
    } else {
      (void)r_.take();  // MASK <n>: Hefesto has no mask colours
    }
  }

  // + POLYGON <layer> <pt> <pt> <pt> ...: its rectangles, as LEF polygons
  // decompose, added like RECTs (half units).
  void polygon() {
    const auto layer = r_.read_layer(false);
    const auto points = r_.read_points(r_.oscale() / 2.0F);
    if (!points || points->size() < 3) {
      r_.error("Bad polygon in VIAS definition; ignoring.");
      return;
    }
    for (const db::Rect& rect : lef::polygon_rects(*points)) {
      lef::add_via_rect(via_, layer, rect, r_.via_context());
    }
  }

  // The terminating `;`: the generated geometry (LefGenViaGeometry), then
  // ORIGIN on the whole via (magic-compat OFF).
  void finish() {
    if (generated_) {
      lef::ViaParamSet given = params_;
      given.scale = r_.oscale();
      given.origin = db::Point{};
      const lef::GeneratedViaParams complete = lef::complete_params(given, nullptr, name_, r_.via_context());
      lef::apply_generated(via_, lef::ViaGenerator::generated(complete, r_.via_context()));
    }
    if (origin_ != db::Point{}) {
      const db::Point half{lef::to_half_internal(static_cast<float>(origin_.x), r_.oscale()).value_or(0),
                           lef::to_half_internal(static_cast<float>(origin_.y), r_.oscale()).value_or(0)};
      via_.area = shifted(via_.area, half);
      for (lef::ViaShape& shape : via_.shapes) {
        shape.area = shifted(shape.area, half);
      }
    }
  }

  Reader& r_;
  lef::ViaDefinition& via_;
  std::string name_;
  lef::ViaParamSet& params_;
  bool& generated_;
  std::string rule_;  // VIARULE: no LEF VIARULE parameters are stored yet (LEFVIA 4.1)
  db::Point origin_;
};

void Reader::vias(std::int32_t total) {
  std::int32_t processed = 0;
  bool generated = false;  // Q-LEFDEF-60 ON: lasts for the section
  while (const auto token = next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      message("Unknown keyword \"" + *token + "\" in VIAS definition; ignoring.");
      skip_statement();
      continue;
    }
    if (*key == 1) {
      if (end_matches("VIAS")) {
        break;
      }
      error("Vias END statement missing.");
      continue;
    }
    progress();
    ++processed;
    via_definition(generated);
  }
  count_check(processed, total, "  Processed " + std::to_string(processed) + " vias total.", "vias");
}

void Reader::via_definition(bool& generated) {
  via_params_.rows = 1;
  via_params_.columns = 1;
  if (!magic_compat()) {
    via_params_ = lef::ViaParamSet{};  // Q-LEFDEF-60 OFF
    generated = false;
  }
  const auto name = next();
  if (!name) {
    error("Bad via statement:  Need via name");
    return;
  }
  const std::string via_name = name->substr(0, 2047);  // %2047s
  lef::ViaDefinition* via =
      lef::begin_via(context_.vias, context_.layers, via_name, lef::ViaSection::def_via, via_context());
  if (via == nullptr) {
    skip_statement();
    return;
  }
  ViaDefinitionReader{*this, *via, via_name, via_params_, generated}.run();
}

}  // namespace hefesto::def::detail
