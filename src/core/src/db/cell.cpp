// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cell definitions and the global cell namespace (layout-hierarchy "Cell
// definitions and the cell namespace", "Renaming a cell definition",
// "Deleting a cell definition"; layout-properties "Typed property store").
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBcellname.c
// (DBCellNewDef, DBCellLookDef, DBCellRenameDef, DBCellDeleteDef) and
// database/DBprop.c (DBPropPut). The namespace is Magic's string hash table
// with 64 initial buckets, so iteration follows Magic's order. Magic is
// Copyright (C) 1985, 1990 Regents of the University of California
// (permissive license, THIRD_PARTY_NOTICES.md).

#include "hefesto/db/cell.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::db {

namespace {

constexpr std::size_t kCellDefBuckets = 64;  // NCELLDEFBUCKETS
constexpr std::string_view kUnnamed = "(UNNAMED)";

}  // namespace

CellDef::CellDef(std::string name, std::size_t plane_count) : name_(std::move(name)) {
  planes_.resize(plane_count);
}

CellDef::~CellDef() = default;

void set_modified_flag(CellDef& def) {
  if (!def.flags().modified) {
    if (ChangeRecorder* r = def.recorder()) {
      r->modified_changed(def, false);
    }
  }
  def.flags().modified = true;
}

void mark_modified(CellDef& def) {
  set_modified_flag(def);
  def.flags().needs_new_timestamp = true;
}

void set_property(CellDef& def, std::string_view name, std::optional<PropertyValue> value) {
  if (def.flags().no_edit) {
    return;
  }
  if (name == "FIXED_BBOX") {
    def.flags().fixed_bbox = value.has_value();
  }
  if (name == "GDS_FILE") {
    def.flags().vendor_gds = value.has_value();
  }
  if (value) {
    def.properties().put(name, std::move(*value));
  } else {
    (void)def.properties().remove(name);
  }
}

void clear_properties(CellDef& def) {
  def.properties().clear();
  def.flags().fixed_bbox = false;
  def.flags().vendor_gds = false;
}

bool is_subcircuit(const CellDef& def) {
  return std::any_of(def.labels().begin(), def.labels().end(), [](const Label& l) { return l.is_port(); });
}

std::vector<std::string> CellDef::linked_use_ids() const {
  std::vector<std::string> ids;
  use_ids_.for_each([&](const util::MagicHashTable<bool>::Entry& e) {
    if (e.value) {
      ids.push_back(e.key);
    }
  });
  return ids;
}

struct CellLibrary::Impl {
  util::MagicHashTable<CellDef*> table{kCellDefBuckets};
  std::vector<std::unique_ptr<CellDef>> owned;
  ChangeRecorder* recorder = nullptr;  // every owned def points at this slot
};

CellLibrary::CellLibrary() : impl_(std::make_unique<Impl>()) {}
CellLibrary::~CellLibrary() = default;
CellLibrary::CellLibrary(CellLibrary&&) noexcept = default;
CellLibrary& CellLibrary::operator=(CellLibrary&&) noexcept = default;

diag::Outcome<CellDef*> CellLibrary::create(std::string_view name, std::size_t plane_count) {
  std::string key{name.empty() ? kUnnamed : name};
  if (key.size() > 4 && key.ends_with(".mag")) {
    key.resize(key.size() - 4);
  }
  auto& entry = impl_->table.find_or_insert(key);
  if (entry.value && *entry.value != nullptr) {
    return diag::make(codes::kCellExists, "cell \"" + key + "\" already exists");
  }
  auto def = std::make_unique<CellDef>(key, plane_count);
  CellDef* raw = def.get();
  raw->recorder_slot_ = &impl_->recorder;
  entry.value = raw;
  impl_->owned.push_back(std::move(def));
  return raw;
}

CellDef* CellLibrary::find(std::string_view name) const {
  const auto* entry = impl_->table.find(std::string{name});
  return entry != nullptr && entry->value ? *entry->value : nullptr;
}

diag::Status CellLibrary::rename(CellDef& def, std::string_view new_name, bool force) {
  if (def.flags().internal) {
    return diag::make(codes::kCellInternal, "Error:  Attempt to rename internal cell \"" + def.name() + "\"");
  }
  if (find(new_name) != nullptr) {
    return diag::make(codes::kCellExists, "Cannot rename; cell \"" + std::string{new_name} + "\" already exists!");
  }
  if (def.flags().vendor_gds) {
    if (!force) {
      return diag::make(codes::kCellVendor, "Error:  Attempt to rename read-only cell \"" + def.name() + "\"");
    }
    for (const char* reserved : {"GDS_FILE", "GDS_START", "GDS_END"}) {
      set_property(def, reserved, std::nullopt);
    }
  }
  const ScopedRecorderDetach unrecorded(*this);
  (void)rename_definition(def, new_name);
  for (CellUse* use : def.instances()) {
    if (use->parent != nullptr) {
      mark_modified(*use->parent);
    }
  }
  mark_modified(def);  // Q-MODEL-15 (fix)
  return diag::Done{};
}

diag::Status CellLibrary::rename_definition(CellDef& def, std::string_view new_name) {
  if (find(new_name) != nullptr) {
    return diag::make(codes::kCellExists, "Magic error: there is already a cell named \"" + std::string{new_name} + "\"");
  }
  impl_->table.clear_value(def.name());
  impl_->table.insert(std::string{new_name}, &def);
  def.name_ = std::string{new_name};
  return diag::Done{};
}

diag::Status CellLibrary::check_removable(const CellDef& def) const {
  if (def.flags().internal) {
    return diag::make(codes::kCellInternal, "Attempt to delete internal cell \"" + def.name() + "\"");
  }
  for (const CellUse* use : def.instances()) {
    if (use->parent != nullptr && !use->parent->flags().internal) {
      return diag::make(codes::kCellInUse, "Cell has non-top-level dependency in use \"" + use->id + "\"");
    }
  }
  return diag::Done{};
}

diag::Status CellLibrary::remove(CellDef& def) {
  if (auto check = check_removable(def); !check) {
    return check;
  }
  if (ChangeRecorder* r = recorder()) {
    r->cell_removed(def);
  }
  const ScopedRecorderDetach unrecorded(*this);
  while (!def.instances().empty()) {
    (void)remove_use(*def.instances().front());
  }
  // Uses of other cells inside `def` disappear with it.
  while (!def.uses().empty()) {
    (void)remove_use(*def.uses().front());
  }
  impl_->table.remove(def.name());
  auto& owned = impl_->owned;
  owned.erase(std::remove_if(owned.begin(), owned.end(), [&](const auto& p) { return p.get() == &def; }),
              owned.end());
  return diag::Done{};
}

void CellLibrary::for_each(const std::function<void(CellDef&)>& visit) const {
  impl_->table.for_each([&](const auto& entry) {
    if (entry.value && *entry.value != nullptr) {
      visit(**entry.value);
    }
  });
}

std::size_t CellLibrary::size() const noexcept {
  return impl_->owned.size();
}

void CellLibrary::set_recorder(ChangeRecorder* recorder) noexcept {
  impl_->recorder = recorder;
}

ChangeRecorder* CellLibrary::recorder() const noexcept {
  return impl_->recorder;
}

}  // namespace hefesto::db
