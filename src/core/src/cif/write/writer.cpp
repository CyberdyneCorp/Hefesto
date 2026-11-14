// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol writer (see writer.hpp). Ports Magic 8.3.694 (fc5daa9e)
// cif/CIFwrite.c CIFWrite (after the subtree loading and the reducer
// check), cifWriteMarkFunc, cifOut, cifOutFunc, cifWriteUseFunc,
// cifWriteLabelFunc, cifWritePaintFunc and CIFWriteFlat, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Differences from Magic, none visible in the text of a write that
// succeeds:
// - Nothing in the layout changes (Q-MASK-208): the symbol numbers live in
//   a map instead of cd_client, a cell the prepare phase could not read is
//   skipped (cifOut reads it again), and the flat symbol is a scratch cell
//   outside the library instead of `__CIF2__` in it.
// - A cancellation stops the write (Magic's SigInterruptPending empties
//   the stack and writes the rest); the caller discards the file.
// - The CIFhier property is honoured with magic-compat OFF (Q-MASK-410):
//   a cell with it forces both output switches on for itself and for the
//   children it numbers.
// Owner: CIF.

#include <span>
#include <utility>

#include "codes.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "writer.hpp"

namespace hefesto::cif::detail {

namespace {

constexpr std::string_view kFlatName = "__CIF2__";
constexpr std::uint64_t kCancelPoll = 4096;  // array elements between cancellation checks

}  // namespace

// --- TextOut ------------------------------------------------------------------------

void TextOut::put(std::string_view text) {
  if (!status_.ok()) {
    return;
  }
  buffer_ += text;
  if (buffer_.size() >= kChunk) {
    (void)flush();
  }
}

diag::Status TextOut::flush() {
  if (status_.ok() && !buffer_.empty()) {
    const auto* data = reinterpret_cast<const std::byte*>(buffer_.data());  // NOLINT: byte view of the text
    status_ = sink_.write(std::span<const std::byte>{data, buffer_.size()});
  }
  buffer_.clear();
  return status_;
}

// --- SymbolWriter ---------------------------------------------------------------------

SymbolWriter::SymbolWriter(const PreparedWrite& prepared, TextOut& out, diag::DiagnosticSink& sink,
                           mask::GenerationContext& ctx)
    : prepared_(prepared), out_(out), sink_(sink), ctx_(ctx), source_(mask::generation_source(*prepared.editor)) {
  const CifSettings& settings = prepared.options.settings;
  source_.compat = prepared.compat;
  source_.unfracture = settings.unfracture;
  const mask::OutputStyle& style = *prepared.style;
  numbers_scale_.scale_factor = style.scale.scale_factor;
  numbers_scale_.reducer = style.scale.reducer;
  numbers_scale_.area_labels = settings.arealabels;
  base_.subcells = settings.hier;
  base_.arrays = settings.array;
  base_.unfracture = settings.unfracture;
  base_.fixed_bbox = false;
}

diag::Status SymbolWriter::cancelled() const {
  return diag::make(write_codes::kCancelled, "CIF write cancelled.");
}

// cifWriteMarkFunc for each use, in the enumeration order sampled before
// the cell's generation (CIFwrite.c:333).
void SymbolWriter::mark_children(const db::CellDef& def, const mask::HierarchyOptions& options) {
  for (const db::CellUse* use : db::uses_in_search_order(def, prepared_.compat)) {
    if (use->def == nullptr) {
      continue;
    }
    std::int64_t& number = number_of(*use->def);
    if (number != 0) {
      continue;
    }
    number = next_--;
    stack_.push_back(Pending{use->def, options});
  }
}

diag::Status SymbolWriter::write_hierarchical() {
  const db::CellDef& root = *prepared_.root;
  number_of(root) = -1;
  stack_.push_back(Pending{&root, base_});
  while (!stack_.empty()) {
    const Pending pending = stack_.back();
    stack_.pop_back();
    std::int64_t& number = number_of(*pending.def);
    if (number >= 0) {
      continue;  // already output
    }
    if (ctx_.cancelled()) {
      return cancelled();
    }
    number = -number;
    const std::int64_t written = number;
    if (!pending.def->flags().available) {
      continue;  // could not be read: no definition, the calls stay
    }
    const mask::HierarchyOptions options =
        mask::hierarchy_options(*pending.def, pending.inherited, false, prepared_.compat);
    mark_children(*pending.def, options);
    if (diag::Status s = symbol(*pending.def, written, options); !s.ok()) {
      return s;
    }
  }
  finish(std::abs(number_of(root)));
  return diag::Done{};
}

diag::Status SymbolWriter::write_flat() {
  const db::CellDef& root = *prepared_.root;
  // Q-MASK-321: `__CIF2__` with magic-compat ON, the root's name with it OFF.
  const std::string name = prepared_.compat.magic_compat ? std::string{kFlatName} : root.name();
  db::CellDef scratch(name, prepared_.editor->technology->plane_count());
  scratch.flags().available = true;
  mask::flatten_for_output(source_, root, *prepared_.style, scratch);
  if (ctx_.cancelled()) {
    return cancelled();
  }
  mask::HierarchyOptions options = base_;
  options.subcells = false;  // the scratch cell has no subcells
  options.arrays = false;
  if (diag::Status s = symbol(scratch, 1, options); !s.ok()) {
    return s;
  }
  finish(1);
  return diag::Done{};
}

void SymbolWriter::finish(std::int64_t root_number) {
  out_.put("C " + std::to_string(root_number) + ";\nEnd\n");
}

// cifOutFunc.
diag::Status SymbolWriter::symbol(const db::CellDef& def, std::int64_t number, const mask::HierarchyOptions& options) {
  const mask::OutputStyle& style = *prepared_.style;
  out_.put(symbol_start(number, style.scale.reducer, style.scale.expander));
  if (const auto line = name_line(def.name(), prepared_.options.settings.prefix)) {
    out_.put(*line);
  }
  if (diag::Status s = layers(def, options); !s.ok()) {
    return s;
  }
  labels(def);
  if (diag::Status s = calls(def); !s.ok()) {
    return s;
  }
  out_.put("DF;\n");
  return diag::Done{};
}

diag::Status SymbolWriter::layers(const db::CellDef& def, const mask::HierarchyOptions& options) {
  const mask::OutputStyle& style = *prepared_.style;
  const mask::LayerPlanes planes = mask::generate_hierarchical(source_, def, style, ctx_, options);
  if (ctx_.cancelled()) {
    return cancelled();
  }
  for (std::size_t i = 0; i < style.layers.size(); ++i) {
    const mask::OutputLayer& layer = style.layers[i];
    if (!layer.temporary && planes[i] != nullptr) {
      layer_tiles(layer, *planes[i]);
    }
  }
  return diag::Done{};
}

// The DBSrPaintArea over the whole plane with cifWriteLabelFunc or
// cifWritePaintFunc.
void SymbolWriter::layer_tiles(const mask::OutputLayer& layer, const mask::MaskPlane& plane) {
  tech::TypeMask solid;
  solid.set(mask::kMaskSolid);
  const std::int64_t reducer = numbers_scale_.reducer;
  bool named = false;
  (void)plane.plane().enumerate(db::whole_plane(), solid, [&](const db::TileView& tile) {
    if (layer.label_layer) {
      if (!tile.split) {  // cifWriteLabelFunc ignores non-Manhattan tiles
        out_.put(layer_label_line(layer.name, tile.area, numbers_scale_));
      }
      return true;
    }
    if (!named) {
      out_.put("L " + layer.name + ";\n");
      named = true;
    }
    out_.put(tile.split ? triangle_line(tile, reducer) : box_line(tile.area, reducer));
    ++ctx_.statistics.flat_rects;  // CIFRects
    return true;
  });
}

// The labels of cifOutFunc, in the cell's label order.
void SymbolWriter::labels(const db::CellDef& def) {
  const mask::OutputStyle& style = *prepared_.style;
  const bool magic_compat = prepared_.compat.magic_compat;
  for (const db::Label& label : def.labels()) {
    const std::int32_t index = label.type < style.label_layer.size() ? style.label_layer[label.type] : mask::kNoLayer;
    std::optional<std::string> layer;
    if (index >= 0 && static_cast<std::size_t>(index) < style.layers.size()) {
      layer = style.layers[static_cast<std::size_t>(index)].name;
    }
    const LabelText text = label_text(label.text, magic_compat);
    if (text.changed) {
      sink_.emit(diag::make(write_codes::kLabelText, "Warning: characters changed in string '" + label.text +
                                                         "'; modified string is '" + text.text + "'"));
    } else if (text.unsafe) {
      // Q-MASK-322 ON: Magic writes it silently; the file will not read back.
      sink_.emit(diag::make_native(write_codes::kLabelText,
                                   "Warning: label \"" + label.text +
                                       "\" contains whitespace or ';'; the CIF file will not read it back."));
    }
    out_.put(cell_label_line(text.text, label.rect, layer, numbers_scale_));
  }
}

// cifWriteUseFunc for each use, in the enumeration order sampled after the
// cell's generation (CIFwrite.c:467).
diag::Status SymbolWriter::calls(const db::CellDef& def) {
  const bool ids = prepared_.options.settings.idcell;
  std::uint64_t elements = 0;
  bool stopped = false;
  for (const db::CellUse* use : db::uses_in_search_order(def, prepared_.compat)) {
    if (use->def == nullptr) {
      continue;
    }
    const std::int64_t number = std::abs(number_of(*use->def));
    call_lines(*use, number, numbers_scale_, ids, [&](const std::string& text) {
      out_.put(text);
      stopped = ++elements % kCancelPoll == 0 && ctx_.cancelled();
      return !stopped;
    });
    if (stopped) {
      return cancelled();
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::cif::detail
