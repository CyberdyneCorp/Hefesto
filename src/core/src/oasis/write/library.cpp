// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The translation of a gds-write element stream into an OASIS file
// (oasis-write "GDS-consistent content", "File layout", "Standard
// properties", "Layer names", "Shape repetition detection", "Text
// translation"). The structures and their order are gds-write's; each
// structure becomes a CELL. In a structure gds-write generates, the body
// is the references, then the mask layers in style order (BOUNDARYs, or
// the TEXTs of a `labellayer` layer), then the label TEXTs, each followed
// by its label rectangle: the BOUNDARYs between the references and the
// first label TEXT are the repetition candidates, one run of equal
// layer/datatype being one mask layer. Vendor structures are translated
// element by element.
// Owner: OASW.

#include <utility>

#include "hefesto/oasis/codec/primitives.hpp"
#include "internal.hpp"

namespace hefesto::oasis::detail {

namespace {

/// The body of one structure.
class StructureWriter {
 public:
  StructureWriter(Encoder& out, TranslateContext& translate, const LibraryContext& library, LibraryResult& result)
      : out_(out), translate_(translate), library_(library), result_(result) {}

  void add(const GdsElement& e) {
    if (translate_.vendor) {
      write(translate(e, translate_));
      return;
    }
    switch (e.kind) {
      case GdsKind::boundary:
        boundary(e);
        return;
      case GdsKind::text:
        text(e);
        return;
      default:
        flush();
        write(translate(e, translate_));
        return;
    }
  }

  void finish() { flush(); }

 private:
  enum class Phase : std::uint8_t { references, mask, labels };

  void boundary(const GdsElement& e) {
    Translated t = translate(e, translate_);
    if (phase_ == Phase::labels || !library_.settings->repetitions || t.items.size() != 1 ||
        !t.items.front().properties.empty()) {
      write(t);  // a label rectangle, or no grouping
      return;
    }
    phase_ = Phase::mask;
    const std::pair<std::int32_t, std::int32_t> layer{e.layer, e.datatype};
    if (!candidates_.empty() && layer != layer_) {
      flush();
    }
    layer_ = layer;
    candidates_.push_back(std::move(t.items.front().element));
  }

  void text(const GdsElement& e) {
    flush();
    const std::string text{std::string_view{e.string}.substr(0, e.string.find('\0'))};
    if (phase_ != Phase::labels && library_.label_layer_texts.contains({e.layer, e.datatype, text})) {
      phase_ = Phase::mask;
    } else {
      phase_ = Phase::labels;
      result_.font_labels += e.presentation ? 1U : 0U;
    }
    write(translate(e, translate_));
  }

  void flush() {
    if (candidates_.empty()) {
      return;
    }
    for (Element& e : group_shapes(candidates_)) {
      element(e);
    }
    candidates_.clear();
  }

  void element(const Element& e) {
    if (diag::Status s = out_.element(e); !s.ok()) {
      reject(s);
    }
  }

  void write(const Translated& t) {
    if (diag::Status s = write_items(t, out_); !s.ok()) {
      reject(s);
    }
  }

  void reject(const diag::Status& s) {
    result_.failed = true;
    if (library_.sink != nullptr) {
      library_.sink->emit(diag::make(codes::kEncoding, "Error: an element of cell " + translate_.cell +
                                                           " cannot be written to OASIS: " + s.error().text));
    }
  }

  Encoder& out_;
  TranslateContext& translate_;
  const LibraryContext& library_;
  LibraryResult& result_;
  Phase phase_ = Phase::references;
  std::vector<Element> candidates_;
  std::pair<std::int32_t, std::int32_t> layer_{};
};

class LibraryTranslator {
 public:
  LibraryTranslator(gds::ByteSource& source, const LibraryContext& context, Encoder& out)
      : items_(source),
        context_(context),
        out_(out),
        names_(context.sink),
        table_(gds::char_table(*context.style, context.compat)) {}

  LibraryResult run() {
    for (;;) {
      diag::Outcome<std::optional<GdsItem>> item = items_.next();
      if (!item.ok()) {
        result_.status = item.error();
        break;
      }
      if (!item.value() || item.value()->kind == GdsItem::Kind::library_end) {
        break;
      }
      if (diag::Status s = step(*item.value()); !s.ok()) {
        result_.status = s;
        break;
      }
    }
    if (diag::Status s = begin(std::nullopt); !s.ok() && result_.status.ok()) {
      result_.status = s;
    }
    if (diag::Status s = out_.finish(); !s.ok() && result_.status.ok()) {
      result_.status = s;
    }
    return std::move(result_);
  }

 private:
  diag::Status step(GdsItem& item) {
    switch (item.kind) {
      case GdsItem::Kind::library_name:
        return begin(item.name);
      case GdsItem::Kind::structure:
        return structure(item.name);
      default:
        return diag::Done{};  // an element outside a structure
    }
  }

  // START, the file properties and, in non-strict mode, the layer names.
  diag::Status begin(std::optional<std::string> library_name) {
    if (begun_) {
      return diag::Done{};
    }
    begun_ = true;
    if (diag::Status s = out_.begin(); !s.ok()) {
      return s;
    }
    std::optional<std::string> top;
    if (library_name && !context_.settings->library) {
      top = names_.of(*library_name);
    }
    if (diag::Status s = out_.standard_properties(top); !s.ok()) {
      return s;
    }
    if (context_.settings->layernames) {
      write_layer_names(*context_.style, context_.compat, out_, context_.sink);
    }
    return diag::Done{};
  }

  diag::Outcome<std::vector<GdsElement>> body() {
    std::vector<GdsElement> elements;
    for (;;) {
      diag::Outcome<std::optional<GdsItem>> item = items_.next();
      if (!item.ok()) {
        return item.error();
      }
      if (!item.value() || item.value()->kind != GdsItem::Kind::element) {
        return elements;  // ENDSTR (or a truncated stream)
      }
      elements.push_back(std::move(item.value()->element));
    }
  }

  diag::Status structure(const std::string& payload) {
    if (diag::Status s = begin(std::nullopt); !s.ok()) {
      return s;
    }
    diag::Outcome<std::vector<GdsElement>> elements = body();
    if (!elements.ok()) {
      return elements.error();
    }
    const std::string output = names_.of(payload);
    if (diag::Status s = out_.begin_cell(output); !s.ok()) {
      return s;
    }
    if (elements.value().empty() && context_.reencode) {
      if (std::optional<diag::Status> done = context_.reencode(payload, output, out_, names_)) {
        result_.failed = result_.failed || !done->ok();
        return diag::Done{};
      }
    }
    TranslateContext translate;
    translate.names = &names_;
    translate.sink = context_.sink;
    translate.cell = output;
    const std::string name{std::string_view{payload}.substr(0, payload.find('\0'))};
    translate.vendor = !context_.generated.contains(name) && name.rfind("XXXXX", 0) != 0;
    translate.table = &table_;
    translate.strings = gds::StringOptions{context_.style->has_option(mask::style_option::string_limit),
                                           context_.settings->lower};
    translate.compat = context_.compat;
    StructureWriter writer(out_, translate, context_, result_);
    for (const GdsElement& e : elements.value()) {
      writer.add(e);
    }
    writer.finish();
    result_.failed = result_.failed || translate.failed;
    return diag::Done{};
  }

  GdsItems items_;
  const LibraryContext& context_;
  Encoder& out_;
  OutputNames names_;
  gds::CharTable table_;
  LibraryResult result_;
  bool begun_ = false;
};

void layer_name_error(diag::DiagnosticSink* sink, const mask::OutputLayer& layer) {
  if (sink != nullptr) {
    sink->emit(diag::make(codes::kLayerName, "Error: output layer " + layer.name + " has negative datatype " +
                                                 std::to_string(layer.gds->datatype) +
                                                 "; no LAYERNAME is written for it."));
  }
}

}  // namespace

void write_layer_names(const mask::OutputStyle& style, const compat::CompatSettings& compat, Encoder& out,
                       diag::DiagnosticSink* sink) {
  const gds::CharTable table = gds::char_table(style, compat);
  for (const mask::OutputLayer& layer : style.layers) {
    if (layer.temporary || !layer.gds || layer.gds->layer < 0 || layer.gds->layer > 255 ||
        layer.gds->datatype > 65535) {
      continue;
    }
    if (layer.gds->datatype < 0) {
      layer_name_error(sink, layer);
      continue;
    }
    std::string name = layer.name;
    if (!legal_string(StringKind::n_string, name)) {
      name = table.map(name);
    }
    if (legal_string(StringKind::n_string, name)) {
      (void)out.layer_name(name, static_cast<std::uint32_t>(layer.gds->layer),
                           static_cast<std::uint32_t>(layer.gds->datatype));
    }
  }
}

LibraryResult translate_library(gds::ByteSource& gds_stream, const LibraryContext& context, Encoder& out) {
  LibraryTranslator translator(gds_stream, context, out);
  return translator.run();
}

}  // namespace hefesto::oasis::detail
