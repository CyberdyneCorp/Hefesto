// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF NONDEFAULTRULE section (capability lef-read "LEF NONDEFAULTRULE
// section"): LefReadNonDefaultRule (lefRead.c:1861-2056).
//
// - Q-LEFDEF-9 (fix): a duplicate name tries `<name>_0`, `<name>_1`, ...
//   (Magic retries `_0` forever); "Renaming this rule" names the name kept.
// - Q-LEFDEF-10 (compat): magic-compat ON, HARDSPACING, USEVIA, USEVIARULE,
//   MINCUTS and PROPERTY leave their `;` unread, so it is an unknown
//   keyword that swallows the next statement, and the body of a VIA whose
//   name is known is read as rule statements; OFF, every statement is
//   consumed through its `;` and a known VIA's body is skipped to its END.
// - A name looked up without an entry keeps its place in the table's hash
//   order (HashFind), through LayerAccess::reserve.
// - A value that is not a number leaves the rule unchanged (Magic stores an
//   uninitialised float; X-2 tier 1).
//
// Owner: LEFR (design D2).

#include <array>
#include <string>

#include "lef/read/reader.hpp"

namespace hefesto::lef::detail {

namespace {

// nondef_property_keys, in its order.
enum class Key : std::uint8_t {
  hardspacing,
  layer,
  end,
  via,
  usevia,
  useviarule,
  mincuts,
  property,
  width,
  diagwidth,
  spacing,
  wireextension,
};

constexpr std::array<std::string_view, 12> kKeys{
    "HARDSPACING", "LAYER", "END",   "VIA",       "USEVIA",  "USEVIARULE",
    "MINCUTS",     "PROPERTY", "WIDTH", "DIAGWIDTH", "SPACING", "WIREEXTENSION",
};

/// The texts of one WIDTH, SPACING or WIREEXTENSION statement.
struct ValueTexts {
  std::string_view without_layer;
  Dialect dialect;  // Magic reports two of them as DEF messages
  std::string_view what;
  db::Coord NonDefaultRuleLayer::*field;
  bool half_units;
};

ValueTexts value_texts(Key key) {
  switch (key) {
    case Key::width:
      return {"WIDTH specified without layer.", Dialect::lef, "width", &NonDefaultRuleLayer::width, false};
    case Key::spacing:
      return {"SPACING specified without layer.", Dialect::def, "spacing", &NonDefaultRuleLayer::spacing, false};
    default:
      return {"WIREEXT specified without layer.", Dialect::def, "extension", &NonDefaultRuleLayer::extension, true};
  }
}

/// The body of one NONDEFAULTRULE section.
class RuleBody {
 public:
  RuleBody(Reader& reader, std::string name) : reader_(reader) { rule_.name = std::move(name); }

  NonDefaultRule run() {
    while (const auto keyword = reader_.token()) {
      statement(*keyword);
      if (done_) {
        break;
      }
    }
    return std::move(rule_);
  }

 private:
  void statement(const Token& keyword);
  void end_statement();
  void via_statement();
  void use_statement(Key key);
  void value_statement(Key key);
  void diag_statement();
  void ignored_pair();
  void add_rule(const std::string& name, RuleTarget target);
  [[nodiscard]] RuleTarget target_of(std::string_view name) const;
  /// Q-LEFDEF-10 OFF: consume the statement's `;`.
  void end_of_statement() {
    if (!reader_.magic_compat()) {
      reader_.skip_statement();
    }
  }

  Reader& reader_;
  NonDefaultRule rule_;
  bool in_layer_ = false;
  bool done_ = false;
  bool has_target_ = false;  // Magic's lefl != NULL
};

void RuleBody::statement(const Token& keyword) {
  const auto index = lookup_full(keyword.text, kKeys);
  if (!index) {
    reader_.info("Unknown keyword \"" + keyword.text + "\" in NONDEFAULTRULES definition; ignoring.");
    reader_.skip_statement();
    return;
  }
  const auto key = static_cast<Key>(*index);
  switch (key) {
    case Key::end:
      end_statement();
      break;
    case Key::hardspacing:
      has_target_ = false;
      rule_.hard_spacing = true;
      end_of_statement();
      break;
    case Key::via:
      via_statement();
      break;
    case Key::usevia:
    case Key::useviarule:
    case Key::layer:
      use_statement(key);
      break;
    case Key::mincuts:
    case Key::property:
      ignored_pair();
      break;
    case Key::diagwidth:
      diag_statement();
      break;
    default:
      value_statement(key);
      break;
  }
}

// END n: leaves layer mode, or ends the section; the name is not checked.
void RuleBody::end_statement() {
  (void)reader_.token();
  if (in_layer_) {
    in_layer_ = false;
  } else {
    done_ = true;
  }
  has_target_ = false;
}

RuleTarget RuleBody::target_of(std::string_view name) const {
  if (reader_.target().layers.find(name)) {
    return RuleTarget::layer;
  }
  const auto* via = reader_.target().vias.find(std::string{name});
  return (via != nullptr && via->value) ? RuleTarget::via : RuleTarget::none;
}

void RuleBody::add_rule(const std::string& name, RuleTarget target) {
  rule_.layers.insert(rule_.layers.begin(), NonDefaultRuleLayer{name, target, 0, 0, 0});
  has_target_ = target != RuleTarget::none;
}

// VIA n ... END n: a new via is read as a VIA definition; a known one's
// body depends on Q-LEFDEF-10. Either way a rule names it.
void RuleBody::via_statement() {
  const auto name = reader_.token();
  if (!name) {
    done_ = true;
    return;
  }
  const std::string end_name = truncate_name(name->text);
  RuleTarget target = target_of(name->text);
  if (target == RuleTarget::none) {
    reader_.via_definition(end_name);
    target = RuleTarget::via;
  } else if (!reader_.magic_compat()) {
    reader_.skip_section(end_name);
  }
  add_rule(name->text, target);
}

// LAYER n (enters layer mode), USEVIA n, USEVIARULE n.
void RuleBody::use_statement(Key key) {
  const auto name = reader_.token();
  if (!name) {
    done_ = true;
    return;
  }
  const RuleTarget target = target_of(name->text);
  if (target == RuleTarget::none) {
    reader_.target().layers.reserve(name->text);
  }
  add_rule(name->text, target);
  if (key == Key::layer) {
    in_layer_ = true;
  } else {
    end_of_statement();
  }
}

// MINCUTS a b, PROPERTY a b: the two tokens are ignored.
void RuleBody::ignored_pair() {
  has_target_ = false;
  (void)reader_.token();
  (void)reader_.token();
  end_of_statement();
}

void RuleBody::value_statement(Key key) {
  const ValueTexts texts = value_texts(key);
  if (!in_layer_) {
    reader_.info(texts.without_layer, texts.dialect);
  }
  const auto value = reader_.read_value(texts.half_units);
  if (rule_.layers.empty()) {
    reader_.info("No rule for non-default " + std::string{texts.what} + ".");
  } else if (!has_target_) {
    reader_.info("No layer for non-default " + std::string{texts.what} + ".");
  } else if (value) {
    rule_.layers.front().*texts.field = *value;
  }
  reader_.skip_statement();
}

void RuleBody::diag_statement() {
  if (!in_layer_) {
    reader_.info("Layer value specified without layer.");
  }
  (void)reader_.token();
  reader_.skip_statement();
}

}  // namespace

void Reader::nondefault_rule(const std::string& name) {
  const auto exists = [this](const std::string& candidate) {
    const auto* entry = target_.rules.find(candidate);
    return entry != nullptr && entry->value.has_value();
  };
  std::string kept = name;
  if (exists(name)) {
    warning("Nondefault rule \"" + name + "\" was already defined.");
    std::size_t index = 0;
    do {
      kept = name + "_" + std::to_string(index++);
    } while (exists(kept));
    warning("Renaming this rule \"" + kept + "\"");
  }
  RuleBody body{*this, kept};
  NonDefaultRule rule = body.run();
  target_.rules.insert(kept, std::move(rule));
}

}  // namespace hefesto::lef::detail
