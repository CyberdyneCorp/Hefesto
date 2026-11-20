// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The NONDEFAULTRULES section of DEF write (capability def-write
// "NONDEFAULTRULES section"; Q-LEFDEF-98, 99, 100).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c DefWriteCell (the
// non-default rule output), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <set>
#include <string>
#include <vector>

#include "def/write/internal.hpp"

namespace hefesto::def::write_detail {

namespace {

/// What a rule's layer entry names (lefRule::lefInfo).
struct Target {
  std::string name;  // canonName
  std::optional<tech::TileType> type;
  bool found = false;
};

Target target(const lef::NonDefaultRuleLayer& entry, const lef::LayerTable& layers, const lef::ViaTable& vias) {
  if (entry.target == lef::RuleTarget::layer) {
    if (const lef::LayerRecord* rec = layers.find(entry.name)) {
      return Target{rec->canonical_name, rec->type, true};
    }
  } else if (entry.target == lef::RuleTarget::via) {
    const auto* found = vias.find(entry.name);
    if (found != nullptr && found->value) {
      return Target{found->value->canonical_name, found->value->type, true};
    }
  }
  return Target{entry.name, std::nullopt, false};
}

/// -anal: the route layers the rule does not mention, at their default
/// width, each record once (aliases share one). Cleared slots are skipped
/// (Q-LEFDEF-100; Magic dereferences them).
void anal_lines(const Context& c, const std::vector<InfoEntry>& order, const std::vector<Target>& targets,
                std::string& out) {
  std::set<const lef::LayerRecord*> done;
  for (const InfoEntry& e : order) {
    if (e.layer == nullptr || e.layer->layer_class != lef::LayerClass::route || !done.insert(e.layer).second) {
      continue;
    }
    if (e.layer->type == tech::kSpace) {
      continue;
    }
    bool mentioned = false;
    for (const Target& t : targets) {
      mentioned = mentioned || t.type == e.layer->type;
    }
    if (!mentioned) {
      out += "\n     + LAYER " + e.layer->canonical_name + " WIDTH " +
             g10(scaled(e.layer->width.value_or(0), c.scale));
    }
  }
}

// The layer entries: with magic-compat ON every line repeats the first
// entry and WIREEXT is not scaled; with it OFF each entry is its own and
// WIREEXT is in DEF units (Q-LEFDEF-98).
void entry_lines(const Context& c, const lef::NonDefaultRule& rule, const std::vector<Target>& targets,
                 std::string& out) {
  for (std::size_t i = 0; i < rule.layers.size(); ++i) {
    const std::size_t k = c.compat ? 0 : i;
    const lef::NonDefaultRuleLayer& e = rule.layers[k];
    out += "\n     + LAYER " + targets[k].name + " WIDTH " + g10(scaled(e.width, c.scale));
    if (e.extension > 0) {
      const double half = static_cast<double>(static_cast<float>(e.extension)) / 2.0;
      out += " WIREEXT " + g10(c.compat ? half : half * static_cast<double>(c.scale));
    }
  }
}

}  // namespace

void write_rules(const Context& c, const lef::RuleTable& table, const std::vector<InfoEntry>& order,
                 const lef::LayerTable& layers, const lef::ViaTable& vias, bool anal, std::string& out) {
  std::vector<const lef::NonDefaultRule*> rules;
  table.for_each([&](const lef::RuleTable::Entry& e) {
    if (e.value) {
      rules.push_back(&*e.value);
    }
  });
  if (rules.empty()) {
    return;
  }
  out += "NONDEFAULTRULES " + std::to_string(rules.size()) + " ;\n";
  for (const lef::NonDefaultRule* rule : rules) {
    std::vector<Target> targets;
    for (const lef::NonDefaultRuleLayer& entry : rule->layers) {
      targets.push_back(target(entry, layers, vias));
      if (!targets.back().found) {
        // Magic dereferences the missing lefInfo here and crashes.
        c.say_native(kMagicCrash, "Non-default rule " + rule->name + " names \"" + entry.name +
                                      "\", which is not a LEF layer or via; Magic 8.3.694 crashes here. The name is "
                                      "written as given.");
      }
    }
    out += "  - " + rule->name;
    if (anal) {
      anal_lines(c, order, targets, out);
    }
    entry_lines(c, *rule, targets, out);
    out += " ;\n";
  }
  out += "END NONDEFAULTRULES\n\n";
}

}  // namespace hefesto::def::write_detail
