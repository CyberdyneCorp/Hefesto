// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefReadNonDefaultRules and DefReadBlockages (capability def-read
// "NONDEFAULTRULES section", "BLOCKAGES section"), and the section count
// check every section ends with ("Section count checks").
// - Q-LEFDEF-75 (fix): the layer flag starts cleared, names are looked up
//   without adding entries to the LEF tables, and an unknown name is
//   reported.
// - Q-LEFDEF-76 (fix): a blockage without a LAYER (a placement blockage)
//   is reported and not painted.
// - Q-LEFDEF-68 (fix): a malformed blockage RECT skips its statement.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:780-984 and
// 1781-1874, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <array>
#include <string>

#include "def/read/reader.hpp"
#include "hefesto/db/cell.hpp"

namespace hefesto::def::detail {

namespace {

enum RuleProperty : std::size_t {
  kHardSpacing,
  kLayer,
  kVia,
  kViaRule,
  kMinCuts,
  kProperty,
  kWidth,
  kDiagWidth,
  kSpacing,
  kWireExt,
  kDone,
};

constexpr std::array<std::string_view, 11> kRuleProperties{
    "HARDSPACING", "LAYER", "VIA", "VIARULE", "MINCUTS", "PROPERTY", "WIDTH", "DIAGWIDTH", "SPACING", "WIREEXT", ";",
};

constexpr std::array<std::string_view, 2> kStatementKeys{"-", "END"};
constexpr std::array<std::string_view, 2> kBlockageProperties{"RECT", "LAYER"};

}  // namespace

void Reader::count_check(std::int32_t processed, std::int32_t total, std::string ok_text, std::string_view noun) {
  if (processed == total) {
    report(std::move(ok_text));
    return;
  }
  warning("Number of " + std::string{noun} + " read (" + std::to_string(processed) +
          ") does not match the number declared (" + std::to_string(total) + ").");
}

// One `- <name> ... ;` of NONDEFAULTRULES.
class RuleReader {
 public:
  explicit RuleReader(Reader& reader) : r_(reader) {}

  void run() {
    std::string token = r_.take();
    lef::NonDefaultRule set;
    set.name = token;
    while (!token.starts_with(';') && !(token.empty() && r_.ended())) {
      if (!token.starts_with('+')) {
        token = r_.take();
        if (!in_layer_) {
          continue;
        }
      }
      if (token.starts_with('+')) {
        in_layer_ = false;
        rule_ = std::nullopt;
        token = r_.take();
      }
      token = property(set, token);
    }
    const std::string name = set.name;
    r_.context().rules_table.insert(name, std::move(set));  // a name read again replaces its rules
  }

 private:
  std::string property(lef::NonDefaultRule& set, const std::string& keyword) {
    const auto key = lef::lookup_full(keyword, kRuleProperties);
    if (!key) {
      r_.message("Unknown non-default rule property \"" + keyword + "\" in NONDEFAULTRULE definition; ignoring.");
      return keyword;
    }
    switch (*key) {
      case kDone:
      case kHardSpacing:
        set.hard_spacing = set.hard_spacing || *key == kHardSpacing;
        has_layer_ = false;
        return keyword;
      case kLayer:
      case kVia:
      case kViaRule:
        return target(set, *key == kLayer);
      case kMinCuts:
      case kProperty:
        has_layer_ = false;
        (void)r_.take();
        return r_.take();
      case kDiagWidth:
        if (!in_layer_) {
          r_.message("Layer value specified without layer.");
        }
        return r_.take();
      default:
        return value(set, static_cast<RuleProperty>(*key));
    }
  }

  // LAYER, VIA or VIARULE <name>: a new rule, first in the list.
  std::string target(lef::NonDefaultRule& set, bool layer) {
    std::string name = r_.take();
    r_.context().layers.note_key(name);  // HashFind(&LefInfo, token) (Q-LEFDEF-135)
    const LefEntry entry = r_.lookup(name);
    lef::NonDefaultRuleLayer rule;
    rule.name = name;
    rule.target = entry.via != nullptr ? lef::RuleTarget::via : (entry.layer != nullptr ? lef::RuleTarget::layer
                                                                                         : lef::RuleTarget::none);
    has_layer_ = !entry.empty();
    if (entry.empty()) {
      r_.fix_warning("Unknown layer \"" + name + "\" in NONDEFAULTRULE definition.");  // Q-LEFDEF-75
    }
    set.layers.insert(set.layers.begin(), rule);
    rule_ = 0;
    in_layer_ = layer;
    return name;
  }

  // WIDTH, SPACING or WIREEXT <value>.
  std::string value(lef::NonDefaultRule& set, RuleProperty key) {
    static constexpr std::array<std::string_view, 3> kNames{"WIDTH", "SPACING", "WIREEXT"};
    static constexpr std::array<std::string_view, 3> kWhat{"width", "spacing", "extension"};
    const std::size_t which = key == kWidth ? 0 : (key == kSpacing ? 1 : 2);
    if (!in_layer_) {
      r_.message(std::string{kNames[which]} + " specified without layer.");
    }
    std::string token = r_.take();
    const auto number = lef::parse_number(token);
    if (!rule_) {
      r_.message("No rule for non-default " + std::string{kWhat[which]} + ".");
    } else if (!has_layer_) {
      r_.message("No layer for non-default " + std::string{kWhat[which]} + ".");
    } else if (number) {  // Magic uses an undefined value when it does not parse
      store(set.layers.front(), which, *number);
    }
    return token;
  }

  void store(lef::NonDefaultRuleLayer& rule, std::size_t which, float v) {
    if (which == 2) {
      r_.warning("Non-default extension at via not implemented.");
      rule.extension = r_.doubled(v).value_or(rule.extension);
    } else if (which == 0) {
      rule.width = r_.scaled(v).value_or(rule.width);
    } else {
      rule.spacing = r_.scaled(v).value_or(rule.spacing);
    }
  }

  Reader& r_;
  bool in_layer_ = false;   // Q-LEFDEF-75: Magic's flag starts uninitialised
  bool has_layer_ = false;  // lefl != NULL
  std::optional<std::size_t> rule_;
};

void Reader::nondefault_rules(std::int32_t total) {
  (void)design();
  std::int32_t processed = 0;
  while (const auto token = next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      message("Unknown keyword \"" + *token + "\" in NONDEFAULTRULES definition; ignoring.");
      skip_statement();
      continue;
    }
    if (*key == 1) {
      if (end_matches("NONDEFAULTRULES")) {
        break;
      }
      error("Non-default rule END statement missing.");
      continue;
    }
    ++processed;
    RuleReader{*this}.run();
  }
  count_check(processed, total, "  Processed " + std::to_string(processed) + " non-default rules total.",
              "non-default rules");
}

// One `- ... ;` of BLOCKAGES: the layer is the statement's own (Q-LEFDEF-76).
void Reader::blockage() {
  std::optional<tech::TileType> layer;
  bool has_layer = false;
  while (const auto token = next()) {
    if (token->starts_with(';')) {
      return;
    }
    const auto key = lef::lookup_full(*token, kBlockageProperties);
    if (!key) {
      message("Unknown blockage property \"" + *token + "\" in BLOCKAGES definition; ignoring.");
    } else if (*key == 1) {
      layer = read_layer(true);
      has_layer = true;
    } else if (!blockage_rect(layer, has_layer)) {
      return;
    }
  }
}

// RECT: painted on the statement's layer; false when the statement ended.
bool Reader::blockage_rect(std::optional<tech::TileType> layer, bool has_layer) {
  bool semicolon = false;
  const auto rect = read_rect(layer.has_value() || !has_layer, oscale_, semicolon);
  if (!rect) {
    if (!semicolon) {
      skip_statement();  // Q-LEFDEF-68
    }
    return false;
  }
  if (!has_layer) {
    fix_warning("Blockage without a layer (a placement blockage) is not painted.");
  } else if (layer) {
    paint(design(), *layer, *rect);
  }
  return !semicolon;
}

void Reader::blockages(std::int32_t total) {
  (void)design();
  std::int32_t processed = 0;
  while (const auto token = next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      message("Unknown keyword \"" + *token + "\" in BLOCKAGES definition; ignoring.");
      skip_statement();
      continue;
    }
    if (*key == 1) {
      if (end_matches("BLOCKAGES")) {
        break;
      }
      error("Blockage END statement missing.");
      continue;
    }
    progress();
    ++processed;
    blockage();
  }
  if (processed == total) {
    report("  Processed " + std::to_string(processed) + " blockage" + (processed > 1 ? "s" : "") + ".");
  } else {
    warning("Number of blockages read (" + std::to_string(processed) + ") does not match the number declared (" +
            std::to_string(total) + ").");
  }
}

}  // namespace hefesto::def::detail
