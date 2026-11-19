// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The reader of the tech `lef` section (capability lef-tech-section,
// requirements "Lef section line grammar", "Magic type resolution in lef
// lines", "Keyword to LEF class mapping", "LEF name aliasing and canonical
// names", "Default routing parameters" and "Default contact area"; design
// D5).
//
// Deviations from Magic, both modes (undefined behaviour in LefTechLine):
// - an `ignore` line never changes a name already bound to a non-ignore
//   record: Magic reads its uninitialized isObstruction and mtype there.
//   Hefesto reports LEF-TECH-008 and leaves the name unchanged;
// - "LEF name ... already used for magic type ..." names the obstruction
//   type when the record has no normal type (Magic indexes its type-name
//   table with -1).
//
// Ported from Magic VLSI 8.3.694 lef/lefTech.c (LefTechLine), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFTECH (design D2).

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/keywords.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/tech/technology.hpp"
#include "tech/build_state.hpp"

namespace hefesto::lef::tech_detail {

namespace {

// LefTechLine's keywords, in Magic's table order (LEFTECH_*).
enum Keyword : std::uint8_t { kObs, kLayer, kRoute, kRouting, kMaster, kCut, kContact, kOverlap, kBound, kIgnore };

constexpr std::array<std::string_view, 10> kKeywords = {"obstruction", "layer",   "route",   "routing", "masterslice",
                                                        "cut",         "contact", "overlap", "bound",   "ignore"};

constexpr db::Coord kDefaultWidth = 3;    // DEFAULT_WIDTH (lefInt.h)
constexpr db::Coord kDefaultSpacing = 4;  // DEFAULT_SPACING
constexpr db::Rect kNullArea{0, 0, 0, 0};        // GeoNullRect (utils/geometry.c)
constexpr db::Rect kSentinelArea{-1, -1, -1, -1};  // "to be filled"

// Adds the `lef` section to every diagnostic located on one of its lines,
// so the magic rendering prints TechPrintLine's context.
class SectionStamp final : public diag::DiagnosticSink {
 public:
  explicit SectionStamp(diag::DiagnosticSink& inner) : inner_(inner) {}

  void emit(const diag::Diagnostic& diagnostic) override {
    const auto* where = std::get_if<diag::SourceLocation>(&diagnostic.location);
    if (where == nullptr || where->line == 0 || diagnostic.related.has_value()) {
      inner_.emit(diagnostic);
      return;
    }
    diag::Diagnostic stamped = diagnostic;
    stamped.related = diag::ObjectRef{diag::ObjectKind::section, "lef"};
    inner_.emit(stamped);
  }

 private:
  diag::DiagnosticSink& inner_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

// The magic types of one line: the primary type (lowest type number) and,
// for a two-type obstruction line, the secondary via-obstruction type.
struct LineTypes {
  tech::TileType primary = tech::kSpace;
  tech::TileType secondary = tech::kSpace;
  bool contact = false;  // DBIsContact(primary)
};

}  // namespace

class SectionReader {
 public:
  SectionReader(LayerTable& table, const tech::Technology& tech, const drc::RuleSet* rules, bool magic_compat,
                diag::DiagnosticSink& sink)
      : table_(table), tech_(tech), rules_(rules), magic_compat_(magic_compat), stamp_(sink) {}

  // LefTechLine.
  void line(const tech::RawLine& line) {
    const std::vector<std::string>& t = line.tokens;
    if (t.empty()) {
      return;
    }
    where_ = diag::SourceLocation{line.file, line.line};
    const cmd::LookupResult found = cmd::lookup(t[0], kKeywords);
    if (found.match != cmd::Match::found) {
      error("LEF-TECH-001", "Unknown LEF section keyword: " + t[0] + ".  Line ignored.");
      return;
    }
    const auto option = static_cast<Keyword>(found.index);
    if (option == kIgnore) {
      bind_names(t, 1, option, LineTypes{});
      return;
    }
    if (t.size() < 3) {
      error("LEF-TECH-002", "No LEF layer names present!");
      return;
    }
    if (const std::optional<LineTypes> types = resolve(t[1], option)) {
      bind_names(t, 2, checked_option(option, *types), *types);
    }
  }

 private:
  void error(std::string_view code, std::string text) {
    stamp_.emit(diag::make(code, std::move(text), where_));
  }

  [[nodiscard]] std::string type_name(tech::TileType type) const {
    return std::string{tech_.type_name(type)};  // DBTypeLongNameTbl
  }

  // DBTechNoisyNameMask, then the first two user types in type order.
  [[nodiscard]] std::optional<LineTypes> resolve(const std::string& name, Keyword option) {
    tech::Reporter reporter(stamp_);
    reporter.set_location(where_, "lef");
    const tech::TypeList list = tech::name_mask(tech_.data(), name, &reporter, tech::Noise::error);
    LineTypes types;
    int count = 0;
    for (tech::TileType t = tech::kFirstUserType; t < tech_.user_type_end(); ++t) {
      if (!list.types.test(t)) {
        continue;
      }
      ++count;
      if (count == 1) {
        types.primary = t;
      } else if (count == 2) {
        types.secondary = t;
      }
    }
    if (!count_ok(name, count, option)) {
      return std::nullopt;
    }
    types.contact = tech_.is_contact(types.primary);
    return types;
  }

  // Magic's loop stops counting at the second type, so with magic-compat ON
  // three or more types count as two (an obstruction line takes the first
  // two); with it OFF they are rejected, as the spec intends.
  bool count_ok(const std::string& name, int count, Keyword option) {
    if (count == 0) {
      error("LEF-TECH-003", "Bad magic layer type \"" + name + "\" in LEF layer definition.");
      return false;
    }
    if (count > 2 && !magic_compat_) {
      error("LEF-TECH-005", "Too many types in LEF layer definition.");
      return false;
    }
    if (count >= 2 && option != kObs) {
      error("LEF-TECH-004", "Can only define multiple types for via obstruction layers.");
      return false;
    }
    return true;
  }

  // `layer` becomes `cut` or `route`; the consistency messages of a contact
  // type used as a route and a non-contact type used as a cut.
  Keyword checked_option(Keyword option, const LineTypes& types) {
    if (option == kLayer) {
      return types.contact ? kCut : kRoute;
    }
    const bool cut_keyword = option == kCut || option == kContact;
    if (types.contact && !cut_keyword && option != kObs) {
      error("LEF-TECH-006", "Attempt to define cut type " + type_name(types.primary) + " as " +
                                std::string{kKeywords[option]} + ".");
    } else if (!types.contact && cut_keyword) {
      error("LEF-TECH-007", "Attempt to define non-cut type " + type_name(types.primary) + " as a cut.");
    }
    return option;
  }

  // The names of the line: new names share one record, created at the first
  // of them (its canonical name); existing names are completed.
  void bind_names(const std::vector<std::string>& t, std::size_t first, Keyword option, const LineTypes& types) {
    std::optional<std::size_t> created;
    for (std::size_t i = first; i < t.size(); ++i) {
      auto& entry = table_.names_.find_or_insert(t[i]);
      if (entry.value) {
        complete(table_.records_[*entry.value], t[i], option, types);
        continue;
      }
      if (!created) {
        table_.records_.push_back(new_record(option, types));
        table_.records_.back().canonical_name = t[i];
        created = table_.records_.size() - 1;
      }
      entry.value = *created;
    }
  }

  // A name already in the table (an ignore record is never changed).
  void complete(LayerRecord& r, const std::string& name, Keyword option, const LineTypes& types) {
    if (r.layer_class == LayerClass::ignore) {
      return;
    }
    if (option == kObs && r.obstruction_type == tech::kSpace) {
      set_obstruction(r, types);
    } else if (option != kObs && option != kIgnore && r.type == tech::kSpace) {
      r.type = types.primary;
    } else {
      const tech::TileType shown = r.type != tech::kSpace ? r.type : r.obstruction_type;
      error("LEF-TECH-008", "LEF name " + name + " already used for magic type " + type_name(shown));
    }
  }

  static void set_obstruction(LayerRecord& r, const LineTypes& types) {
    r.obstruction_type = types.primary;
    r.obstruction_types = tech::TypeMask::of(types.primary);
    if (r.layer_class == LayerClass::cut && types.secondary != tech::kSpace) {
      r.obstruction_types.set(types.secondary);
    }
  }

  // The record of a line's first new name, with the defaults per class.
  [[nodiscard]] LayerRecord new_record(Keyword option, const LineTypes& types) const {
    LayerRecord r;
    if (option == kIgnore) {
      return r;  // LayerClass::ignore, no types
    }
    const Keyword effective = option == kObs ? (types.contact ? kContact : kRoute) : option;
    r.layer_class = class_of(effective);
    if (option == kObs) {
      set_obstruction(r, types);
    } else {
      r.type = types.primary;
    }
    if (effective == kCut || effective == kContact) {
      r.contact_area = effective == kCut ? std::optional{kNullArea} : unfilled(kSentinelArea);
    } else if (r.layer_class == LayerClass::route) {
      route_defaults(r, types.primary);
    }
    return r;
  }

  [[nodiscard]] static LayerClass class_of(Keyword option) noexcept {
    switch (option) {
      case kCut:
      case kContact: return LayerClass::cut;
      case kRoute:
      case kRouting: return LayerClass::route;
      case kMaster: return LayerClass::masterslice;
      case kOverlap: return LayerClass::overlap;
      case kBound: return LayerClass::bound;
      default: return LayerClass::ignore;
    }
  }

  // A value the DRC defaults fill later: Magic's sentinel with magic-compat
  // ON, unset with it OFF.
  template <typename T>
  [[nodiscard]] std::optional<T> unfilled(T sentinel) const {
    return magic_compat_ ? std::optional<T>{sentinel} : std::nullopt;
  }

  // "Default routing parameters": 3 and 4 when the DRC style has no width
  // or spacing rule for the type, else filled later.
  void route_defaults(LayerRecord& r, tech::TileType type) const {
    const db::Coord width = rules_ != nullptr ? drc::default_width(*rules_, tech_, type) : 0;
    const db::Coord spacing = rules_ != nullptr ? drc::default_spacing(*rules_, tech_, type, type) : 0;
    r.width = width == 0 ? std::optional{kDefaultWidth} : unfilled(db::Coord{-1});
    r.spacing = spacing == 0 ? std::optional{kDefaultSpacing} : unfilled(db::Coord{-1});
    r.pitch = 0;
    r.direction = RouteDirection::horizontal;
  }

  LayerTable& table_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  const tech::Technology& tech_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  const drc::RuleSet* rules_;
  bool magic_compat_;
  SectionStamp stamp_;
  diag::SourceLocation where_;
};

}  // namespace hefesto::lef::tech_detail

namespace hefesto::lef {

LayerTable LayerTable::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                    const drc::RuleSet* rules, const compat::CompatSettings& compat,
                                    diag::DiagnosticSink& sink) {
  LayerTable table;
  for (tech::TileType t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
    table.contact_types_.set(t, tech.is_contact(t));
  }
  tech_detail::SectionReader reader(table, tech, rules, compat.magic_compat, sink);
  for (const tech::RawLine& line : section.lines) {
    reader.line(line);
  }
  return table;
}

}  // namespace hefesto::lef
