// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cifinput statement reader: one pass over the section loads one style
// ("Cifinput section grammar", "Input style declaration and default style",
// "Variant-conditional statements", "Scalefactor statement", "Grid limit",
// "CIF layer name table", "Layer and templayer rules", "GDS layer and
// datatype mapping", "Label layer statements"), with Magic's tech messages
// at the same points, and CIFReadTechFinal's normalisation and its
// `Input style ...` line ("Input-scale reporting").
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFrdtech.c
// (CIFReadNameToType, CIFParseReadLayers, cifReadStyleInit,
// CIFReadTechLine, CIFReadTechFinal, cifParseCalmaNums), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Deviations (docs/decisions/cross-cutting.md X-2 tier 1): a `grid`
// statement without a value reports "Unable to parse grid limit value."
// instead of reading a NULL argument; a variant suffix longer than the style
// name never matches (Magic reads before the name); a full CIF layer table
// does not fall back to the type alias table (Magic would set layout type
// bits in a CIF layer mask); `labels` on an `ignore`d layer does not read
// the plane of type -1.
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "mask/input/reader.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "hefesto/diag/codes.hpp"
#include "hefesto/mask/style/core.hpp"
#include "mask/style/text.hpp"

namespace hefesto::mask::input_detail {

void Messages::add(std::string_view code, std::string text, bool native) {
  diag::Location where;
  std::optional<diag::ObjectRef> section;
  if (line_ != nullptr && line_->line > 0) {
    where = diag::SourceLocation{line_->file, line_->line};
    section = diag::ObjectRef{diag::ObjectKind::section, "cifinput"};
  }
  diag::Diagnostic d = native ? diag::make_native(code, std::move(text), where) : diag::make(code, std::move(text), where);
  d.related = section;
  messages_.push_back(std::move(d));
}

void Messages::plain(std::string_view code, std::string text) { messages_.push_back(diag::make(code, std::move(text))); }

namespace {

using style_text::c_atoi;
using style_text::starts_with;
using Tokens = std::vector<std::string>;

constexpr std::int32_t kCalmaLayerMax = 255;  // CALMA_LAYER_MAX

/// CIFParseReadLayers' chunks: split at commas, a run of commas after a
/// name counting as one ("a,,b" is a and b; ",a" starts with an empty name).
std::vector<std::string> read_layer_names(std::string_view text) {
  std::vector<std::string> names;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t comma = text.find(',', pos);
    names.emplace_back(text.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos));
    if (comma == std::string_view::npos) {
      break;
    }
    pos = text.find_first_not_of(',', comma);
  }
  return names;
}

std::optional<OpKind> op_kind(const std::string& word) {
  static const std::pair<std::string_view, OpKind> kOps[] = {
      {"and", OpKind::op_and},         {"and-not", OpKind::and_not},       {"or", OpKind::op_or},
      {"grow", OpKind::grow},          {"grow-grid", OpKind::grow_grid},   {"shrink", OpKind::shrink},
      {"copyup", OpKind::copyup},      {"boundary", OpKind::boundary},     {"not-square", OpKind::not_square},
      {"mask-hints", OpKind::mask_hints}, {"tagged", OpKind::tagged},
  };
  for (const auto& [keyword, kind] : kOps) {
    if (word == keyword) {
      return kind;
    }
  }
  return std::nullopt;
}

class Reader {
 public:
  Reader(const tech::Technology& technology, const std::string& wanted, const compat::CompatSettings& compat,
         bool list_messages)
      : technology_(technology),
        compat_(compat),
        list_messages_(list_messages),
        machine_(StyleStateMachine::Options{wanted,
                                            compat.magic_compat ? StyleStateMachine::Registration::prefix
                                                                : StyleStateMachine::Registration::exact,
                                            StyleStateMachine::VariantRule::not_longer}) {}

  void line(const tech::RawLine& raw);
  [[nodiscard]] LoadResult finish();

 private:
  void wrong_args(const std::string& word) { messages_.error("Wrong number of arguments in " + word + " statement."); }
  void check_prefix_shadow(const Tokens& t);
  [[nodiscard]] bool settings_line(const Tokens& t);
  void scalefactor_line(const Tokens& t);
  void grid_line(const Tokens& t);
  void statement(const Tokens& t);
  void rule_line(const Tokens& t, bool temporary);
  void calma_line(const Tokens& t);
  void labels_line(const Tokens& t);
  void apply_labels(const std::vector<std::size_t>& layers, std::optional<LabelKind> kind);
  [[nodiscard]] std::optional<tech::TileType> label_type_of_rule();
  void warn_superseded(std::size_t layer, tech::TileType type);
  void ignore_line(const Tokens& t);
  void options_line(const Tokens& t);
  void op_line(const Tokens& t);
  [[nodiscard]] bool op_operands(const Tokens& t, LayerOp& op);
  void link_op(LayerOp op);
  void warn_layer_rule_op(OpKind kind);

  /// CIFReadNameToType.
  [[nodiscard]] std::optional<std::size_t> name_to_layer(const std::string& name, bool new_ok);
  /// CIFParseReadLayers with newOK = TRUE.
  [[nodiscard]] std::vector<std::size_t> read_layers(std::string_view list);

  const tech::Technology& technology_;
  compat::CompatSettings compat_;
  bool list_messages_;
  StyleStateMachine machine_;
  Messages messages_;
  InputStyle style_;
  std::optional<std::size_t> current_rule_;  // cifCurReadLayer
  bool current_op_ = false;                  // cifCurReadOp != NULL
  bool section_failed_ = false;
  bool table_full_reported_ = false;
};

// --- style lines --------------------------------------------------------------

void Reader::check_prefix_shadow(const Tokens& t) {
  if (!list_messages_ || !(t.size() == 2 || (t.size() == 4 && starts_with(t[2], "variant")))) {
    return;
  }
  std::vector<std::string> candidates;
  if (t.size() == 2) {
    candidates.push_back(t[1]);
  } else {
    for (const std::string& variant : style_text::split_commas(t[3])) {
      candidates.push_back(t[1] + variant);
    }
  }
  const std::vector<std::string> declared = machine_.names();
  for (const std::string& name : candidates) {
    if (std::find(declared.begin(), declared.end(), name) != declared.end()) {
      continue;
    }
    const auto longer = std::find_if(declared.begin(), declared.end(), [&name](const std::string& s) {
      return s.size() > name.size() && starts_with(s, name);
    });
    if (longer != declared.end()) {
      messages_.native(kPrefixShadow, "Input style \"" + name + "\" is a prefix of input style \"" + *longer + "\".");
    }
  }
}

void Reader::line(const tech::RawLine& raw) {
  messages_.at(raw);
  const Tokens& t = raw.tokens;
  if (t.empty()) {
    return;
  }
  if (t[0] == "style") {
    check_prefix_shadow(t);
  }
  const StyleStateMachine::Fed fed = machine_.feed(t);
  if (fed.kind == StyleStateMachine::LineKind::style) {
    if (fed.wrong_arguments) {
      wrong_args(t[0]);
    }
    return;
  }
  if (machine_.no_style()) {
    section_failed_ = true;  // cifCurReadStyle == NULL: CIFReadTechLine returns FALSE
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

/// scalefactor and grid lines, which apply even under a non-matching
/// variant (TECH_SUSPENDED).
bool Reader::settings_line(const Tokens& t) {
  if (t[0] == "scalefactor") {
    scalefactor_line(t);
    return true;
  }
  if (starts_with(t[0], "grid")) {
    grid_line(t);
    return true;
  }
  return false;
}

void Reader::scalefactor_line(const Tokens& t) {
  if (t.size() < 2 || t.size() > 4) {
    wrong_args(t[0]);
    return;
  }
  const ScaleValue parsed = parse_scale(t[1]).value_or(ScaleValue{0, 1});
  ScaleSource source{parsed.numerator, parsed.expander, 0};
  if (t.size() >= 3) {
    if (starts_with(t.back(), "nanom")) {
      source.unit = 10;
    } else if (starts_with(t.back(), "angstr")) {
      source.unit = 100;
    }
  }
  if (source.unit != 0 && t[1].find('.') != std::string::npos) {
    messages_.native(kDecimalUnit, "scalefactor " + t[1] + " with a unit keyword: the multiplier is " +
                                       (compat_.magic_compat ? "set to the unit (magic-compat)"
                                                             : "multiplied by the unit") +
                                       " (Q-MASK-300).");
  }
  if (source.value <= 0) {
    source.value = 0;
    messages_.error("Scalefactor must be a strictly positive value.");
  }
  style_.scale_source = source;
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

// --- statements ---------------------------------------------------------------

void Reader::statement(const Tokens& t) {
  const std::string& word = t[0];
  if (word == "layer") {
    rule_line(t, false);
  } else if (word == "templayer" || word == "fault") {
    rule_line(t, true);
  } else if (word == "calma" || starts_with(word, "gds")) {
    calma_line(t);
  } else if (word == "labels") {
    labels_line(t);
  } else if (word == "ignore") {
    ignore_line(t);
  } else if (word == "options") {
    options_line(t);
  } else {
    op_line(t);
  }
}

std::optional<std::size_t> Reader::name_to_layer(const std::string& name, bool new_ok) {
  if (const auto found = style_.find_cif_layer(name)) {
    return found;
  }
  if (!new_ok) {
    return std::nullopt;
  }
  if (style_.cif_layers.size() >= kMaxInputLayers) {
    if (!table_full_reported_) {
      messages_.error("CIF read layer table ran out of space at " + std::to_string(kMaxInputLayers) + " layers.");
      messages_.error("Get your Magic maintainer to increase the table size.");
      table_full_reported_ = true;
    }
    return std::nullopt;
  }
  style_.cif_layers.push_back(name);
  style_.labels.emplace_back();
  return style_.cif_layers.size() - 1;
}

std::vector<std::size_t> Reader::read_layers(std::string_view list) {
  std::vector<std::size_t> layers;
  for (const std::string& name : read_layer_names(list)) {
    if (const auto layer = name_to_layer(name, true)) {
      layers.push_back(*layer);
    }
  }
  std::sort(layers.begin(), layers.end());
  layers.erase(std::unique(layers.begin(), layers.end()), layers.end());
  return layers;
}

void Reader::rule_line(const Tokens& t, bool temporary) {
  current_rule_.reset();
  current_op_ = false;
  if (style_.rules.size() >= kMaxInputLayers) {
    messages_.error("Can't handle more than " + std::to_string(kMaxInputLayers) + " layers per style.");
    messages_.error("Your local Magic wizard can increase the table size.");
    return;
  }
  if (t.size() != 2 && t.size() != 3) {
    wrong_args(t[0]);
    return;
  }
  InputRule rule;
  rule.temporary = temporary;
  if (temporary) {
    const auto layer = name_to_layer(t[1], true);
    if (!layer) {
      return;
    }
    rule.cif_layer = *layer;
    if (t[0] == "fault") {
      messages_.error("Error:  Fault layers deprecated.  Treating as templayer");
    }
  } else {
    const tech::Lookup<tech::TileType> found = technology_.find_type(t[1]);
    if (!found.ok()) {
      messages_.error(std::string{found.error == tech::LookupError::ambiguous ? "Ambiguous" : "Unrecognized"} +
                      " layer (type) name \"" + t[1] + "\"");
      return;
    }
    rule.type = *found.id;
  }
  if (t.size() == 3) {
    LayerOp op;
    op.kind = OpKind::op_or;
    op.cif_layers = read_layers(t[2]);
    rule.ops.push_back(std::move(op));
    current_op_ = true;
  }
  style_.rules.push_back(std::move(rule));
  current_rule_ = style_.rules.size() - 1;
}

void Reader::calma_line(const Tokens& t) {
  if (t.size() != 4) {
    wrong_args(t[0]);
    return;
  }
  const auto layer = name_to_layer(t[1], false);
  if (!layer) {
    messages_.error("Unrecognized CIF layer: \"" + t[1] + "\"");
    return;
  }
  // Q-MASK-326 (compat): Magic rejects a list of 255 or more entries.
  const std::optional<std::size_t> limit =
      compat_.magic_compat ? std::optional<std::size_t>{static_cast<std::size_t>(kCalmaLayerMax)} : std::nullopt;
  const auto layers = parse_calma_numbers(t[2], limit, messages_);
  const auto types = parse_calma_numbers(t[3], limit, messages_);  // both lists are parsed, as in Magic
  if (!layers || !types || layers->empty() || types->empty()) {
    return;
  }
  for (const std::int32_t l : *layers) {
    for (const std::int32_t d : *types) {
      style_.calma[GdsKey{l, d}] = *layer;
    }
  }
}

// --- labels -------------------------------------------------------------------

void Reader::labels_line(const Tokens& t) {
  if (!current_rule_) {
    messages_.error("Must define layer before giving labels it holds.");
    return;
  }
  std::optional<LabelKind> kind;
  if (t.size() == 3) {
    const std::string& k = t[2];
    if (k == "text" || k == "sticky") {  // Q-MASK-301 (replicate)
      kind = LabelKind::text;
    } else if (k == "port") {
      kind = LabelKind::port;
    } else if (starts_with(k, "cell")) {
      kind = LabelKind::cell_id;
    }
  }
  if (t.size() != 2 && !kind) {
    wrong_args(t[0]);
    return;
  }
  apply_labels(read_layers(t[1]), kind);
}

/// The layout type a `labels` statement records for the current rule; for a
/// templayer Magic records the CIF layer index as a type (Q-MASK-311).
std::optional<tech::TileType> Reader::label_type_of_rule() {
  const InputRule& rule = style_.rules[*current_rule_];
  if (!rule.temporary) {
    return rule.type;
  }
  if (!compat_.magic_compat) {
    messages_.native(kTemplayerLabels, "labels under the templayer \"" + style_.cif_layers[rule.cif_layer] +
                                           "\" ignored (Q-MASK-311).");
    return std::nullopt;
  }
  if (rule.cif_layer >= technology_.type_count()) {
    messages_.native(kTemplayerLabels, "labels under the templayer \"" + style_.cif_layers[rule.cif_layer] +
                                           "\" ignored: its index " + std::to_string(rule.cif_layer) +
                                           " is not a layout type (Q-MASK-311).");
    return std::nullopt;
  }
  return static_cast<tech::TileType>(rule.cif_layer);
}

void Reader::warn_superseded(std::size_t layer, tech::TileType type) {
  const std::int32_t old = style_.labels[layer].type;
  if (old <= 0 || static_cast<std::size_t>(old) >= technology_.type_count()) {
    return;  // space, or an ignored layer (Magic reads DBPlane(-1))
  }
  const auto prior = static_cast<tech::TileType>(old);
  const auto on_plane = [&](tech::TileType t, tech::TileType home_of) {
    const std::optional<tech::PlaneId> home = technology_.home_plane(home_of);
    return home && (technology_.type_planes(t) & tech::plane_bit(*home)) != 0;
  };
  if (!on_plane(type, prior) && !on_plane(prior, type)) {
    messages_.error("Labels on layer \"" + style_.cif_layers[layer] + "\" attached to \"" +
                    std::string{technology_.type_name(type)} + "\" supersedes prior attachment to \"" +
                    std::string{technology_.type_name(prior)} + "\".");
  }
}

void Reader::apply_labels(const std::vector<std::size_t>& layers, std::optional<LabelKind> kind) {
  const std::optional<tech::TileType> type = label_type_of_rule();
  if (!type) {
    return;
  }
  for (const std::size_t layer : layers) {
    warn_superseded(layer, *type);
    style_.labels[layer].type = static_cast<std::int32_t>(*type);
    if (kind) {
      style_.labels[layer].kind = *kind;
    }
  }
}

void Reader::ignore_line(const Tokens& t) {
  if (t.size() != 2) {
    wrong_args(t[0]);
    return;
  }
  for (const std::size_t layer : read_layers(t[1])) {
    if (style_.labels[layer].type == static_cast<std::int32_t>(tech::kSpace)) {
      style_.labels[layer].type = kDiscardLabels;
    }
  }
}

void Reader::options_line(const Tokens& t) {
  if (t.size() < 2) {
    wrong_args(t[0]);
    return;
  }
  for (std::size_t i = 1; i < t.size(); ++i) {
    if (t[i] == "ignore-unknown-layer-labels") {
      style_.options |= input_option::ignore_unknown_layer_labels;
    } else if (t[i] != "no-reconnect-labels") {  // deprecated, accepted
      messages_.error("Unknown cifinput option \"" + t[i] + "\".");
    }
  }
}

// --- operators ----------------------------------------------------------------

bool Reader::op_operands(const Tokens& t, LayerOp& op) {
  switch (op.kind) {
    case OpKind::op_and:
    case OpKind::and_not:
    case OpKind::op_or:
    case OpKind::copyup:
      if (t.size() != 2) {
        return false;
      }
      op.cif_layers = read_layers(t[1]);
      return true;
    case OpKind::grow:
    case OpKind::grow_grid:
    case OpKind::shrink:
      return t.size() == 2;
    case OpKind::mask_hints:
      op.text = t.size() == 2 ? t[1] : std::string{};
      return t.size() == 2;
    case OpKind::tagged:
      if (t.size() != 2 && t.size() != 3) {
        return false;
      }
      op.text = t[1];
      if (t.size() == 3) {
        op.cif_layers = read_layers(t[2]);
      }
      return true;
    default:  // boundary, not-square: extra fields are ignored
      return true;
  }
}

/// Q-MASK-307: boundary and mask-hints only have their input meaning in a
/// templayer without copyup; the loader warns in both settings.
void Reader::warn_layer_rule_op(OpKind kind) {
  const InputRule& rule = style_.rules[*current_rule_];
  const bool hint_like = kind == OpKind::boundary || kind == OpKind::mask_hints;
  const bool in_copyup = rule.find(OpKind::copyup) != nullptr;
  const bool copyup_after_hint =
      kind == OpKind::copyup && (rule.find(OpKind::boundary) != nullptr || rule.find(OpKind::mask_hints) != nullptr);
  if ((hint_like && (!rule.temporary || in_copyup)) || copyup_after_hint) {
    messages_.native(kBoundaryInLayer, std::string{"boundary or mask-hints in a "} +
                                           (rule.temporary ? "copyup templayer" : "layer rule") +
                                           " is not an input boundary or hint (Q-MASK-307).");
  }
}

void Reader::link_op(LayerOp op) {
  InputRule& rule = style_.rules[*current_rule_];
  if (!current_op_ && op.kind == OpKind::op_or && rule.ops.empty()) {
    rule.ops.push_back(std::move(op));
  } else {
    rule.ops.push_back(std::move(op));
    rule.simple = false;
  }
  current_op_ = true;
}

void Reader::op_line(const Tokens& t) {
  if (!current_rule_) {
    messages_.error("Must define layer before specifying operations.");
    return;
  }
  const std::optional<OpKind> kind = op_kind(t[0]);
  if (!kind) {
    messages_.error("Unknown statement \"" + t[0] + "\".");
    return;
  }
  LayerOp op;
  op.kind = *kind;
  if (!op_operands(t, op)) {
    wrong_args(t[0]);
    return;
  }
  if (op.kind == OpKind::grow || op.kind == OpKind::grow_grid || op.kind == OpKind::shrink) {
    op.distance = c_atoi(t[1]);
    if (op.distance <= 0) {
      messages_.error("Grow/shrink distance must be greater than zero.");
      return;
    }
  }
  warn_layer_rule_op(op.kind);
  link_op(std::move(op));
}

// --- final --------------------------------------------------------------------

LoadResult Reader::finish() {
  LoadResult out;
  out.names = machine_.names();
  out.section_failed = section_failed_;
  out.loaded = !machine_.no_style();
  style_.name = machine_.loading();
  if (out.loaded) {
    std::vector<std::int32_t> distances;
    for (const InputRule& rule : style_.rules) {
      for (const LayerOp& op : rule.ops) {
        distances.push_back(op.distance);
      }
    }
    style_.set_source_distances(std::move(distances));
    if (style_.scale_source.value <= 0) {
      messages_.error("CIF input style \"" + style_.name + "\" bad scalefactor; using 1.");
    }
    const InputScaleStep step = style_.resolve(compat_);
    if (auto warning = truncation_warning(style_, step, compat_)) {
      messages_.native(warning->code, warning->text);
    }
    messages_.plain(kInputStyleScale, "Input style " + style_.name + ": scaleFactor=" +
                                          std::to_string(style_.scale_factor) +
                                          ", multiplier=" + std::to_string(style_.multiplier));
  }
  style_.load_messages = messages_.take();
  out.style = std::move(style_);
  return out;
}

}  // namespace

std::optional<std::vector<std::int32_t>> parse_calma_numbers(std::string_view text, std::optional<std::size_t> limit,
                                                             Messages& messages) {
  std::vector<std::int32_t> numbers;
  std::size_t i = 0;
  while (!limit || numbers.size() < *limit) {
    if (i >= text.size()) {
      return numbers;
    }
    std::int32_t number = -1;
    if (text[i] != '*') {
      number = c_atoi(text.substr(i));
      if (number < 0 || number > kCalmaLayerMax) {
        messages.error("Calma layer and type numbers must be 0 to " + std::to_string(kCalmaLayerMax) + ".");
        return std::nullopt;
      }
    }
    for (; i < text.size() && text[i] != ','; ++i) {
      if (text[i] != '*' && std::isdigit(static_cast<unsigned char>(text[i])) == 0) {
        messages.error("Calma layer/type numbers must be numeric or '*'");
        return std::nullopt;
      }
    }
    while (i < text.size() && text[i] == ',') {
      ++i;
    }
    numbers.push_back(number);
  }
  messages.error("Too many layer/type numbers in line; maximum = " + std::to_string(*limit));
  return std::nullopt;
}

std::optional<diag::Diagnostic> truncation_warning(const InputStyle& style, const InputScaleStep& step,
                                                   const compat::CompatSettings& compat) {
  if (compat.magic_compat || !step.truncated) {
    return std::nullopt;
  }
  return diag::make_native(kScaleTruncated, "Input style " + style.name + ": dividing by " +
                                                std::to_string(step.reduced) +
                                                " truncated the scale factor or a distance, as Magic does.");
}

LoadResult load_style(const tech::Technology& technology, const tech::RawSection& section, const std::string& wanted,
                      const compat::CompatSettings& compat, bool list_messages) {
  Reader reader{technology, wanted, compat, list_messages};
  for (const tech::RawLine& raw : section.lines) {
    reader.line(raw);
  }
  return reader.finish();
}

}  // namespace hefesto::mask::input_detail
