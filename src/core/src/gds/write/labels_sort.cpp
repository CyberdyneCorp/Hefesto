// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Port label order (capability gds-write "Label TEXT records", Q-MASK-205
// replicate). Ported from Magic 8.3.694 calma/CalmaWrite.c:1541-1595 (the
// LabelList built head first, then qsort with compport); Magic is (c) the
// Regents of the University of California, distributed under its permissive
// license (THIRD_PARTY_NOTICES.md).
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/write/labels_sort.hpp"

namespace hefesto::gds {

std::vector<std::size_t> port_label_order(std::span<const db::Label> labels) {
  std::vector<std::size_t> order;
  for (std::size_t i = labels.size(); i-- > 0;) {
    if (labels[i].is_port()) {
      order.push_back(i);  // the head-first list: reverse label order
    }
  }
  stable_merge_sort(order, [&labels](std::size_t a, std::size_t b) {
    return labels[a].port_index < labels[b].port_index;
  });
  return order;
}

}  // namespace hefesto::gds
