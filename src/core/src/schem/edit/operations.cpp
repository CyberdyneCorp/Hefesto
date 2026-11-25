// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editing operations behind `schematic` (operations.hpp; capability
// schematic-editing; schematic-command "Subcommand table"). CMD's
// dispatcher looks an option up here first.
//
// Owner: EDIT.

#include "hefesto/schem/edit/operations.hpp"

#include <array>

#include "internal.hpp"

namespace hefesto::schem {

std::span<const EditOperation> edit_operations() {
  using namespace editimpl;
  static const std::array kOperations{
      EditOperation{"align", op_align},         EditOperation{"arc", op_arc},
      EditOperation{"ascend", op_ascend},       EditOperation{"box", op_box},
      EditOperation{"clipboard", op_clipboard}, EditOperation{"copy", op_copy},
      EditOperation{"delete", op_delete},       EditOperation{"descend", op_descend},
      EditOperation{"flip", op_flip},           EditOperation{"flipv", op_flipv},
      EditOperation{"hierarchy", op_hierarchy}, EditOperation{"hilight", op_hilight},
      EditOperation{"label", op_label},         EditOperation{"line", op_line},
      EditOperation{"merge", op_merge},         EditOperation{"move", op_move},
      EditOperation{"order", op_order},         EditOperation{"paste", op_paste},
      EditOperation{"place", op_place},         EditOperation{"polygon", op_polygon},
      EditOperation{"redo", op_redo},           EditOperation{"replace", op_replace},
      EditOperation{"rotate", op_rotate},       EditOperation{"select", op_select},
      EditOperation{"selection", op_selection}, EditOperation{"setprop", op_setprop},
      EditOperation{"symbol", op_symbol},       EditOperation{"text", op_text},
      EditOperation{"undo", op_undo},           EditOperation{"wire", op_wire},
  };
  return kOperations;
}

}  // namespace hefesto::schem
