// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The via-geometry statements of a LAYER, VIA or VIARULE body (private to
// lef/read; capability lef-read "LEF VIA and VIARULE definitions"; design
// D6): RECT through LefAddViaGeometry (lef/via/store.hpp), and with
// magic-compat OFF (Q-LEFDEF-7) RECT MASK, RECT ITERATE, POLYGON and the
// VIARULE-parameter statements of a VIA, whose geometry comes from LEFVIA's
// generator. layer_section.cpp's body calls it for every entry it adds via
// geometry to.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/lef/via/generate.hpp"
#include "hefesto/lef/via/store.hpp"
#include "lef/read/reader.hpp"

namespace hefesto::lef::detail {

/// The most rectangles one RECT ITERATE, or cuts one ROWCOL, may make
/// (proposed resource cap, design D21; magic-compat OFF only, where Magic
/// reads neither).
inline constexpr std::int64_t kMaxViaRepeat = 65536;

class ViaBody {
 public:
  /// `parameters`: a VIA section, whose body may give VIARULE parameters
  /// (not a VIARULE section, nor a LAYER section).
  ViaBody(Reader& reader, ViaDefinition& via, std::string name, bool parameters)
      : reader_(reader), via_(via), name_(std::move(name)), parameters_(parameters) {}

  /// RECT x1 y1 x2 y2 (half internal units), with RECT MASK and RECT
  /// ITERATE when magic-compat is OFF. `ignored`: the current layer is an
  /// IGNORE entry, whose RECT is silent with magic-compat OFF (Q-LEFDEF-20).
  void rect(const LayerState& layer, bool ignored);

  /// With magic-compat OFF, the statements Magic does not know in a via
  /// body: POLYGON and, in a VIA section, VIARULE, CUTSIZE, LAYERS,
  /// CUTSPACING, ENCLOSURE, ROWCOL, ORIGIN, OFFSET and PATTERN. True when
  /// `keyword` was one of them and its statement has been read.
  bool extension(std::string_view keyword, const LayerState& layer);

  /// The end of the body: a VIA that named a VIARULE gets the generated
  /// geometry (magic-compat OFF).
  void finish();

 private:
  void add(const LayerState& layer, const db::Rect& rect);
  void iterate(const LayerState& layer, const db::Rect& rect);
  void polygon(const LayerState& layer);
  bool parameter(std::string_view keyword);
  std::vector<std::string> arguments();
  void layers_statement(const std::vector<std::string>& args);
  void rowcol_statement(const std::vector<std::string>& args);

  Reader& reader_;
  ViaDefinition& via_;
  ViaRectBuilder rects_{via_};  // committed in finish()
  std::string name_;
  bool parameters_;
  bool has_rule_ = false;
  ViaParamSet params_;
};

}  // namespace hefesto::lef::detail
