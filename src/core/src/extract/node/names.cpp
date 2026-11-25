// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node names and label roles (capability extract-nodes-and-labels "Node
// reference point and generated name", "Label types by trailing
// character", "Node name selection"; openspec/changes/m7-extraction
// design.md D8). Q-EXTRACT-4's one branch is label_insert_position().
// (Magic ref: extract/ExtBasic.c:942-1012, extract/ExtBasic.c:4689-4709,
// extract/ExtRegion.c:315-331, extract/ExtRegion.c:489-503)
//
// Owner: EXTNODE (design D3).

#include <cstdlib>
#include <string>

#include "hefesto/extract/node/names.hpp"

namespace hefesto::extract {

LabelRole label_role(std::string_view text) noexcept {
  if (text.empty()) {
    return LabelRole::none;
  }
  switch (text.back()) {
    case '@':
      return LabelRole::node_attribute;
    case '$':
      return LabelRole::terminal_attribute;
    case '^':
      return LabelRole::gate_attribute;
    default:
      return LabelRole::name;
  }
}

std::size_t label_insert_position(const std::vector<LabelRegion>& labels, bool port,
                                  const compat::CompatSettings& compat) noexcept {
  if (port || labels.empty()) {
    return 0;
  }
  if (!compat.magic_compat) {
    return labels.size();  // Q-EXTRACT-4 OFF: attachment order
  }
  // Magic: after the head, and after every port that follows it.
  std::size_t at = 1;
  while (at < labels.size() && labels[at].port) {
    ++at;
  }
  return at;
}

const LabelRegion* name_label(const NodeRegion& node) noexcept {
  for (const LabelRegion& label : node.labels) {
    if (label_role(label.text) == LabelRole::name) {
      return &label;
    }
  }
  return nullptr;
}

std::string node_name(const NodeRegion& node, const tech::Technology& technology,
                      const compat::CompatSettings& /*compat*/) {
  if (const LabelRegion* label = name_label(node)) {
    return label->text;
  }
  return generated_name(node, technology);
}

std::string generated_name(const NodeRegion& node, const tech::Technology& technology) {
  const auto coord = [](std::int32_t v) {
    const std::int64_t wide = v;
    return (wide < 0 ? std::string{"n"} : std::string{}) + std::to_string(wide < 0 ? -wide : wide);
  };
  std::string name{technology.plane_name(node.plane, tech::NameForm::short_name)};
  name += '_';
  name += coord(node.reference.x);
  name += '_';
  name += coord(node.reference.y);
  name += node.right_side ? "r#" : "#";
  return name;
}

}  // namespace hefesto::extract
