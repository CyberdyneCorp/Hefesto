// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifoutput statement reader: one pass over the section loads one style
// ("cifoutput section grammar", "Style loading and variant-conditional
// statements", "scalefactor statement", "units, stepsize and gridlimit
// statements", "Mask layer declarations", "labels statement and label layer
// mapping", "GDS layer mapping statement", "min-width, render and options
// statements"), with Magic's tech messages at the same points.
//
// Ported from Magic VLSI 8.3.694 cif/CIFtech.c (CIFTechStyleInit,
// CIFTechLine, cifCheckCalmaNum, CIFTechFinal's empty-layer check),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2 tier 1): a `grid`
// statement without a value reports "Unable to parse grid limit value."
// instead of reading a NULL argument; `render` checks only its argument
// count (3-D display attributes are not kept; capability text:
// "informative for Hefesto").
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <utility>

#include "hefesto/diag/codes.hpp"
#include "mask/style/reader.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask::style_detail {

void Messages::add(std::string_view code, std::string text, bool native) {
  diag::Location where;
  std::optional<diag::ObjectRef> section;
  if (line_ != nullptr && line_->line > 0) {
    where = diag::SourceLocation{line_->file, line_->line};
    section = diag::ObjectRef{diag::ObjectKind::section, "cifoutput"};
  }
  diag::Diagnostic d = native ? diag::make_native(code, std::move(text), where) : diag::make(code, std::move(text), where);
  d.related = section;
  messages_.push_back(std::move(d));
}

namespace {

using style_text::c_atoi;
using style_text::starts_with;

constexpr std::size_t kMaxCifLayers = tech::kMaxTileTypes - 1;  // MAXCIFLAYERS
constexpr std::int32_t kCalmaLayerMax = 255;                    // CALMA_LAYER_MAX

/// cifCheckCalmaNum: decimal digits only, 0..255.
bool calma_number_ok(const std::string& text) {
  const std::int32_t n = c_atoi(text);
  return n >= 0 && n <= kCalmaLayerMax &&
         std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

class Reader {
 public:
  Reader(const tech::Technology* technology, const std::string& wanted)
      : technology_(technology), machine_(StyleStateMachine::Options{wanted, StyleStateMachine::Registration::exact,
                                                                     StyleStateMachine::VariantRule::shorter}) {
    style_.scale = StyleScale{0, 1, 0};  // cifTechStyleInit
  }

  void line(const tech::RawLine& raw);
  [[nodiscard]] LoadResult finish();

 private:
  void wrong_args(const std::string& word) { messages_.error("Wrong number of arguments in " + word + " statement."); }
  [[nodiscard]] bool settings_line(const Tokens& t);
  void scalefactor_line(const Tokens& t);
  void grid_line(const Tokens& t);
  void statement(const Tokens& t);
  void layer_line(const Tokens& t);
  void labels_line(const Tokens& t);
  void apply_labels(const tech::TypeMask& mask, bool port, bool no_port);
  void calma_line(const Tokens& t);
  void min_width_line(const Tokens& t);
  void render_line(const Tokens& t);
  void options_line(const Tokens& t);
  void op_line(const Tokens& t);
  [[nodiscard]] LayerContext layer_context() {
    return LayerContext{technology_, &style_.layers, current_layer_, &messages_};
  }

  const tech::Technology* technology_;
  StyleStateMachine machine_;
  Messages messages_;
  OutputStyle style_;
  std::optional<std::size_t> current_layer_;  // cifCurLayer
  bool current_op_ = false;                   // cifCurOp != NULL
  bool got_labels_ = false;                   // cifGotLabels
  bool section_failed_ = false;
};

void Reader::line(const tech::RawLine& raw) {
  messages_.at(raw);
  const Tokens& t = raw.tokens;
  if (t.empty()) {
    return;
  }
  const StyleStateMachine::Fed fed = machine_.feed(t);
  if (fed.kind == StyleStateMachine::LineKind::style) {
    if (fed.wrong_arguments) {
      wrong_args(t[0]);
    }
    return;
  }
  if (machine_.no_style()) {
    section_failed_ = true;  // CIFCurStyle == NULL: the line handler fails (Q-MASK-17)
    return;
  }
  if (!machine_.active() || settings_line(t)) {
    return;
  }
  if (fed.kind == StyleStateMachine::LineKind::variant) {
    if (fed.wrong_arguments) {
      wrong_args(t[0]);
    }
    return;
  }
  if (machine_.applies()) {
    statement(t);
  }
}

bool Reader::settings_line(const Tokens& t) {
  const std::string& word = t[0];
  if (word == "scalefactor") {
    scalefactor_line(t);
  } else if (word == "units") {
    if (t.size() != 2) {
      wrong_args(word);
    } else if (starts_with(t[1], "angstr")) {
      style_.options |= style_option::angstroms;
    }
  } else if (word == "stepsize") {
    if (t.size() != 2) {
      wrong_args(word);
      return true;
    }
    style_.step_size = c_atoi(t[1]);
    if (style_.step_size <= 0) {
      messages_.error("Step size must be positive integer.");
      style_.step_size = 0;
    }
  } else if (starts_with(word, "grid")) {
    grid_line(t);
  } else {
    return false;
  }
  return true;
}

void Reader::scalefactor_line(const Tokens& t) {
  if (t.size() < 2 || t.size() > 4) {
    wrong_args(t[0]);
    return;
  }
  const ScaleValue scale = parse_scale(t[1]).value_or(ScaleValue{0, 1});
  style_.scale.scale_factor = style_text::clamp_int(scale.numerator);
  style_.scale.expander = style_text::clamp_int(scale.expander);
  // "nanometers" multiplies the expander by 10, "angstroms" by 100; a
  // historic reducer value and "calmaonly" are ignored.
  if (t.size() >= 3 && starts_with(t.back(), "nanom")) {
    style_.scale.expander = style_text::clamp_int(std::int64_t{style_.scale.expander} * 10);
  } else if (t.size() >= 3 && starts_with(t.back(), "angstr")) {
    style_.scale.expander = style_text::clamp_int(std::int64_t{style_.scale.expander} * 100);
  }
  style_.scale.reducer = 1;
  if (style_.scale.scale_factor <= 0) {
    style_.scale.scale_factor = 0;
    messages_.error("Scalefactor must be a strictly positive value.");
  }
}

void Reader::grid_line(const Tokens& t) {
  if (t.size() < 2 || !style_text::str_is_int(t[1])) {
    messages_.error("Unable to parse grid limit value.");
    return;
  }
  style_.grid_limit = c_atoi(t[1]);
  if (style_.grid_limit < 0) {
    messages_.error("Grid limit must be a positive integer.");
    style_.grid_limit = 0;
  }
}

void Reader::statement(const Tokens& t) {
  const std::string& word = t[0];
  if (word == "templayer" || word == "layer" || word == "labellayer") {
    layer_line(t);
  } else if (word == "labels") {
    labels_line(t);
  } else if (word == "calma" || starts_with(word, "gds")) {
    calma_line(t);
  } else if (word == "min-width") {
    min_width_line(t);
  } else if (word == "render") {
    render_line(t);
  } else if (word == "options") {
    options_line(t);
  } else {
    op_line(t);
  }
}

void Reader::layer_line(const Tokens& t) {
  if (style_.layers.size() == kMaxCifLayers) {
    current_layer_.reset();
    messages_.error("Can't handle more than " + std::to_string(kMaxCifLayers) + " CIF layers.");
    messages_.error("Your local Magic wizard can fix this.");
    return;
  }
  if (t.size() != 2 && t.size() != 3) {
    current_layer_.reset();
    wrong_args(t[0]);
    return;
  }
  if (!current_op_ && current_layer_ && !got_labels_) {
    messages_.warning("Layer \"" + style_.layers[*current_layer_].name + "\" contains no material.");
  }
  OutputLayer layer;
  layer.name = t[1];
  layer.temporary = t[0] == "templayer";
  layer.label_layer = t[0] == "labellayer";
  style_.layers.push_back(std::move(layer));
  current_layer_ = style_.layers.size() - 1;
  current_op_ = false;
  got_labels_ = false;
  if (t.size() == 3) {
    // A layer list on the layer line is an `or` operation.
    LayerSets sets = parse_layers(layer_context(), t[2], false);
    LayerOp op;
    op.kind = OpKind::op_or;
    op.types = sets.types;
    op.cif_layers = std::move(sets.layers);
    style_.layers.back().ops.push_back(std::move(op));
    current_op_ = true;
  }
}

void Reader::labels_line(const Tokens& t) {
  if (!current_layer_) {
    messages_.error("Must define layer before giving labels it holds.");
    return;
  }
  if (style_.layers[*current_layer_].temporary) {
    messages_.warning("Why are you attaching labels to a temporary layer?");
  }
  bool port = false;
  bool no_port = false;
  if (t.size() == 3) {
    // Prefixes of 4 and 6 characters, and the exact word `text`, which is
    // the same as no option (Q-MASK-11).
    port = starts_with(t[2], "port");
    no_port = !port && starts_with(t[2], "noport");
    if (!port && !no_port && t[2] != "text") {
      messages_.error("Unknown option " + t[2] + " for labels statement.");
      wrong_args(t[0]);
      return;
    }
  } else if (t.size() != 2) {
    wrong_args(t[0]);
    return;
  }
  apply_labels(noisy_types(layer_context(), t[1]), port, no_port);
  got_labels_ = true;
}

void Reader::apply_labels(const tech::TypeMask& mask, bool port, bool no_port) {
  const auto layer = static_cast<std::int32_t>(style_.layers.size() - 1);
  OutputLayer& out = style_.layers.back();
  const bool first = out.label_types.empty();
  out.label_types |= mask;
  out.label_port_only = port && (first || out.label_port_only);
  for (std::size_t i = 0; i < tech::kMaxTileTypes; ++i) {
    if (!mask.test(static_cast<tech::TileType>(i))) {
      continue;
    }
    if (port) {
      style_.port_layer[i] = layer;
      if (style_.port_text[i] == kNoLayer) {
        style_.port_text[i] = layer;
      }
      continue;
    }
    style_.label_layer[i] = layer;
    if (!no_port) {
      style_.port_text[i] = layer;
    }
  }
}

void Reader::calma_line(const Tokens& t) {
  if (!current_layer_) {
    messages_.error("Must define layers before giving their Calma types.");
    return;
  }
  OutputLayer& layer = style_.layers[*current_layer_];
  if (layer.temporary) {
    messages_.warning("Why assign a Calma number to a temporary layer?");
  }
  if (t.size() != 3) {
    wrong_args(t[0]);
    return;
  }
  if (!calma_number_ok(t[1]) || !calma_number_ok(t[2])) {
    messages_.error("Calma layer and type numbers must be 0 to " + std::to_string(kCalmaLayerMax) + ".");
  }
  layer.gds = GdsLayer{c_atoi(t[1]), c_atoi(t[2])};
}

void Reader::min_width_line(const Tokens& t) {
  if (!current_layer_) {
    messages_.error("Must define layers before assigning a minimum width.");
    return;
  }
  if (t.size() != 2) {
    wrong_args(t[0]);
    return;
  }
  style_.layers[*current_layer_].min_width = c_atoi(t[1]);
  style_.options |= style_option::grow_slivers;
}

void Reader::render_line(const Tokens& t) {
  if (t.size() != 5) {
    wrong_args(t[0]);
    return;
  }
  current_layer_.reset();  // not in a layer definition any more
}

void Reader::options_line(const Tokens& t) {
  if (t.size() < 2) {
    wrong_args(t[0]);
    return;
  }
  struct Option {
    std::string_view word;
    std::uint32_t flag;
  };
  static constexpr Option kOptions[] = {
      {"calma-permissive-labels", style_option::permissive_labels},
      {"set-minimum-grid", style_option::minimum_grid},
      {"grow-euclidean", style_option::grow_euclidean},
      {"see-no-vendor", style_option::see_no_vendor},
      {"no-errors", style_option::no_errors},
      {"string-limit", style_option::string_limit},
  };
  for (std::size_t i = 1; i < t.size(); ++i) {
    for (const Option& option : kOptions) {
      if (t[i] == option.word) {
        style_.options |= option.flag;  // other words are ignored
      }
    }
  }
  style_.see_no_vendor = style_.has_option(style_option::see_no_vendor);
}

void Reader::op_line(const Tokens& t) {
  if (!current_layer_) {
    messages_.error("Must define layer before specifying operation.");
    return;
  }
  if (auto op = parse_operator(OpContext{layer_context(), style_.grid_limit}, t)) {
    style_.layers[*current_layer_].ops.push_back(std::move(*op));
    current_op_ = true;
  }
}

LoadResult Reader::finish() {
  LoadResult out;
  out.names = machine_.names();
  out.shadowed = machine_.shadowed();
  out.section_failed = section_failed_;
  out.loaded = !machine_.no_style() && machine_.state() != StyleStateMachine::State::not_loaded;
  if (machine_.no_style()) {
    out.style.load_messages = messages_.take();
    return out;  // no cifoutput style at all (CIFCurStyle == NULL)
  }
  if (current_layer_ && !current_op_ && !got_labels_) {
    messages_.warning("Layer \"" + style_.layers[*current_layer_].name + "\" contains no material.");
  }
  style_.name = machine_.loading();
  finalize(technology_, style_, messages_);
  style_.load_messages = messages_.take();
  out.style = std::move(style_);
  return out;
}

}  // namespace

LoadResult load_style(const tech::Technology* technology, const tech::RawSection& section, const std::string& wanted) {
  Reader reader(technology, wanted);
  for (const tech::RawLine& line : section.lines) {
    reader.line(line);
  }
  return reader.finish();
}

}  // namespace hefesto::mask::style_detail
