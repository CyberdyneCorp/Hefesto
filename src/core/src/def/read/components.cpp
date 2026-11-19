// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DefReadComponents and DefReadLocation (capability def-read "COMPONENTS
// section", "Component placement transform", "Unplaced components"): one
// use per component whose master is loaded or found as a `.mag` file,
// placed with the DEF lower-left convention.
// - Q-LEFDEF-83 (fix): a duplicate instance name is placed and renamed with
//   the `.mag` instance-id rule.
// - Q-LEFDEF-85 (replicate): UNPLACED goes to the origin with a warning; no
//   placement keyword keeps the identity transform.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defRead.c:1301-1424 and
// 2175-2393, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFR (design D2).

#include <array>
#include <string>

#include "def/read/reader.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::def::detail {

namespace {

enum Property : std::size_t {
  kFixed,
  kCover,
  kPlaced,
  kUnplaced,
  kSource,
  kWeight,
  kForeign,
  kRegion,
  kGenerate,
  kProperty,
  kEeqMaster,
};

constexpr std::array<std::string_view, 11> kProperties{
    "FIXED", "COVER", "PLACED", "UNPLACED", "SOURCE", "WEIGHT", "FOREIGN", "REGION", "GENERATE", "PROPERTY",
    "EEQMASTER",
};

constexpr std::array<std::string_view, 2> kStatementKeys{"-", "END"};

constexpr std::string_view kDuplicateUse = "DB-USE-002";

// The box DefReadLocation places by: FIXED_BBOX when the master has a
// fixed bounding box and the property holds four dimensions, else its
// bounding box.
db::Rect placement_box(const db::CellDef& master) {
  if (master.flags().fixed_bbox) {
    if (const auto* value = master.properties().find("FIXED_BBOX")) {
      const auto* dims = std::get_if<db::DimensionList>(value);
      if (dims != nullptr && dims->values.size() == 4) {
        return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
      }
    }
  }
  return master.bbox();
}

// Magic replaces '/' and ',' in instance names.
std::string use_name(Reader& r, std::string name) {
  for (char& c : name) {
    if (c == '/' || c == ',') {
      r.warning("Character in instance name converted to underscore.");
      c = '_';
    }
  }
  return name;
}

}  // namespace

std::optional<db::Transform> Reader::read_location(const db::Rect& bbox, bool unplaced, std::string_view use_name) {
  float x = 0.0F;
  float y = 0.0F;
  std::size_t orient = 0;
  if (unplaced) {
    warning("Unplaced component \"" + std::string{use_name} + "\" will be put at origin.");
  } else {
    const bool open = take().starts_with('(');
    const auto px = open ? lef::parse_number(take()) : std::nullopt;
    const auto py = px ? lef::parse_number(take()) : std::nullopt;
    if (!py || !take().starts_with(')')) {
      error("Cannot parse location: must be ( X Y ) orient");
      return std::nullopt;
    }
    const std::string token = take();
    const auto found = lef::lookup_full(token, kOrientations);
    if (!found) {
      error("Unknown macro orientation \"" + token + "\".");
      return std::nullopt;
    }
    x = *px;
    y = *py;
    orient = *found;
  }
  const db::Transform base = orientation_transform(orient);
  const db::Rect moved = db::apply(base, bbox);
  const auto tx = scaled(x);
  const auto ty = scaled(y);
  if (!tx || !ty) {
    return std::nullopt;  // a location beyond the int range (Magic's cast is undefined)
  }
  db::Transform t = db::compose(db::translation(-moved.x0, -moved.y0), base);
  return db::compose(db::translation(*tx, *ty), t);
}

// One component statement after its `-`.
class ComponentReader {
 public:
  explicit ComponentReader(Reader& reader) : r_(reader) {}

  void run() {
    const auto name = r_.next();
    if (!name || name->empty()) {
      r_.error("Bad component statement:  Need use and macro names");
      r_.skip_statement();
      return;
    }
    id_ = use_name(r_, *name);
    master_ = find_master(r_.take());
    if (master_ == nullptr) {
      return;
    }
    properties();
    place();
  }

 private:
  // DBCellLookDef, else a `.mag` file read into a new definition.
  db::CellDef* find_master(const std::string& name) {
    db::CellLibrary& cells = r_.context().cells;
    if (db::CellDef* loaded = cells.find(name)) {
      return loaded;
    }
    db::CellDef* made = nullptr;
    if (!name.empty()) {
      auto created = cells.create(name, r_.context().tech.plane_count());
      made = created.ok() ? created.value() : nullptr;
    }
    const auto& read = r_.context().options.read_master;
    if (made != nullptr && read && read(*made)) {
      db::recompute_bbox(*made, r_.magic_compat());
      return made;
    }
    r_.error("Cell " + name + " is not defined.  Maybe you have not read the corresponding LEF file?");
    r_.skip_statement();
    if (made != nullptr) {
      (void)cells.remove(*made);  // DBCellDeleteDef
    }
    return nullptr;
  }

  void properties() {
    while (const auto token = r_.next()) {
      if (token->starts_with(';')) {
        return;
      }
      if (token->starts_with('+')) {
        property(r_.take());
      }
    }
  }

  void property(const std::string& keyword) {
    const auto key = lef::lookup_full(keyword, kProperties);
    if (!key) {
      r_.message("Unknown component property \"" + keyword + "\" in COMPONENT definition; ignoring.");
      return;
    }
    if (*key <= kUnplaced) {
      const auto t = r_.read_location(placement_box(*master_), *key == kUnplaced, id_);
      if (t) {
        transform_ = *t;  // a failed location keeps the transform the use had
      }
      return;
    }
    (void)r_.take();  // SOURCE, WEIGHT, FOREIGN, REGION, GENERATE, PROPERTY, EEQMASTER
  }

  // DBPlaceCell; a duplicate id is renamed (Q-LEFDEF-83).
  void place() {
    db::CellDef& parent = r_.design();
    db::PlaceOptions options;
    options.id = id_;
    options.transform = transform_;
    auto placed = db::place(parent, *master_, options);
    if (!placed.ok() && placed.error().code == kDuplicateUse) {
      r_.fix_warning("Duplicate instance-id for cell " + master_->name() + " (" + id_ + ") will be renamed");
      options.id.reset();
      placed = db::place(parent, *master_, options);
    }
    if (!placed.ok()) {
      r_.error(placed.error().text);
    }
  }

  Reader& r_;
  std::string id_;
  db::CellDef* master_ = nullptr;
  db::Transform transform_;
};

void Reader::components(std::int32_t total) {
  (void)design();
  std::int32_t processed = 0;
  while (const auto token = next()) {
    const auto key = lef::lookup_full(*token, kStatementKeys);
    if (!key) {
      message("Unknown keyword \"" + *token + "\" in COMPONENT definition; ignoring.");
      skip_statement();
      continue;
    }
    if (*key == 1) {
      if (end_matches("COMPONENTS")) {
        break;
      }
      error("Component END statement missing.");
      continue;
    }
    progress();
    ++processed;
    ComponentReader{*this}.run();
  }
  count_check(processed, total, "  Processed " + std::to_string(processed) + " subcell instances total.",
              "subcells");
}

}  // namespace hefesto::def::detail
