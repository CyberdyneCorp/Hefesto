// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label layers and label TEXT records (gds-write "Label layers", "Label
// TEXT records"; see hefesto/gds/write/labels.hpp). Placeholder until task
// 10.5 ports calmaPaintLabelFunc and calmaWriteLabelFunc: no TEXT element
// is written.
// Owner: GDSW (task 10.5 replaces this file).

#include "hefesto/gds/write/labels.hpp"

namespace hefesto::gds {

diag::Status emit_label_layer(const mask::MaskPlane& /*plane*/, const mask::OutputLayer& /*layer*/,
                              ElementWriter& /*out*/) {
  return diag::Done{};
}

diag::Status emit_labels(const db::CellDef& /*cell*/, ElementWriter& /*out*/) {
  return diag::Done{};
}

}  // namespace hefesto::gds
