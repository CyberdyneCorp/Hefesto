// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label layers and label TEXT records (gds-write "Label layers", "Label
// TEXT records"; see hefesto/gds/write/labels.hpp): the structure body's
// entry points to the calmaPaintLabelFunc and calmaWriteLabelFunc ports of
// labels_text.hpp. STRING records go through the structure's
// ElementWriter::string_record (calmaOutStringRecord with Magic's
// warnings, never changing the label, Q-MASK-208).
// Owner: GDSW.

#include "hefesto/gds/write/labels.hpp"

namespace hefesto::gds {

namespace {

LabelOutput label_output(ElementWriter& out, PortLabelWarnings* warned) {
  LabelOutput output(out.records(), out.style());
  output.write_scale = out.scale().write;
  output.paint_scale = out.scale().paint;
  output.magscale = out.settings().magscale;
  output.compat = out.compat();
  output.string_record = [&out](RecordWriter& /*records*/, RecordType type, std::string_view text) {
    return out.string_record(type, text);
  };
  output.sink = out.sink();
  output.warned = warned;
  output.technology = out.context().technology;
  output.cell = out.cell().name();
  return output;
}

}  // namespace

diag::Status emit_label_layer(const mask::MaskPlane& plane, std::size_t layer_index, ElementWriter& out) {
  return emit_label_layer(label_output(out, nullptr), layer_index, plane);
}

diag::Status emit_labels(const db::CellDef& cell, ElementWriter& out, PortLabelWarnings& warned) {
  return emit_cell_labels(label_output(out, &warned), cell.labels());
}

}  // namespace hefesto::gds
