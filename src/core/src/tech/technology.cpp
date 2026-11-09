// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// tech::Technology (read access to a finalized technology), runtime
// locking ("tech locked|unlocked|revert") and TechnologyBuilder, which
// drives the same section handlers as the file loader.
//
// The locking functions are ported from Magic VLSI 8.3.694
// commands/CmdTZ.c (CmdTech, TECH_LOCK/TECH_UNLOCK/TECH_REVERT),
// Copyright (C) 1985, 1990 Regents of the University of California
// (see THIRD_PARTY_NOTICES.md).

#include "hefesto/tech/technology.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include "hefesto/tech/loader.hpp"

#include "tech/build_state.hpp"
#include "tech/reader.hpp"

namespace hefesto::tech {

namespace {

const std::string kEmpty;
const TypeMask kNoTypes;

std::size_t clamp_type(TileType t) noexcept {
  return std::min<std::size_t>(t, kMaxTileTypes - 1);
}

template <typename Id>
Lookup<Id> to_lookup(int value) {
  if (value >= 0) {
    return Lookup<Id>{static_cast<Id>(value), LookupError::unknown};
  }
  return Lookup<Id>{std::nullopt, value == kAmbiguous ? LookupError::ambiguous : LookupError::unknown};
}

TypeMask all_but_space_and_drc() {
  TypeMask mask;
  for (std::size_t t = kMagnet; t < kMaxTileTypes; ++t) {
    mask.set(static_cast<TileType>(t));
  }
  return mask;
}

}  // namespace

// --- publication -----------------------------------------------------------

/// Fills the public contact view (user contacts, then stacked types, each
/// once) from the contact list.
void publish_contacts(Technology::Data& d) {
  d.contact_infos.clear();
  TypeMask seen;
  for (const TileType c : d.contact_list) {
    if (seen.test(c)) {
      continue;  // a redeclared contact is listed twice (Q-TECH-17)
    }
    seen.set(c);
    ContactInfo info;
    info.type = c;
    info.residues = d.residues[c];
    info.planes = d.image_planes[c];
    info.stacked = c >= d.num_user;
    if (info.stacked) {
      bool first = true;
      for (std::size_t r = kFirstUserType; r < d.num_user; ++r) {
        if (d.residues[c].test(static_cast<TileType>(r))) {
          (first ? info.component_a : info.component_b) = static_cast<TileType>(r);
          first = false;
        }
      }
    }
    d.contact_infos.push_back(info);
  }
}

// --- Technology ------------------------------------------------------------

Technology::Technology() : data_(builtin_technology()->data_) {}

Technology::Technology(std::shared_ptr<const Data> data) : data_(std::move(data)) {}

const std::string& Technology::name() const noexcept { return data_->name; }
const std::string& Technology::file_name() const noexcept { return data_->file_name; }
int Technology::format_version() const noexcept { return data_->format_version; }
const TechVersion& Technology::version() const noexcept { return data_->version; }
bool Technology::fringe_halo_default() const noexcept { return data_->fringe_halo; }

std::size_t Technology::plane_count() const noexcept { return data_->num_planes(); }

std::string_view Technology::plane_name(PlaneId plane, NameForm form) const {
  if (plane >= plane_count()) {
    return kEmpty;
  }
  if (form == NameForm::short_name) {
    if (const std::string* primary = primary_name(data_->plane_names, plane)) {
      return *primary;
    }
  }
  return data_->plane_long[plane];
}

Lookup<PlaneId> Technology::find_plane(std::string_view name) const {
  return to_lookup<PlaneId>(name_plane(*data_, name));
}

std::size_t Technology::type_count() const noexcept { return data_->num_types; }
std::size_t Technology::user_type_end() const noexcept { return data_->num_user; }

std::string_view Technology::type_name(TileType type, NameForm form) const {
  if (type >= kMaxTileTypes) {
    return kEmpty;
  }
  if (form == NameForm::short_name) {
    if (const std::string* primary = primary_name(data_->type_names, type)) {
      return *primary;
    }
  }
  return data_->type_long[type];
}

std::optional<PlaneId> Technology::home_plane(TileType type) const noexcept {
  if (type >= kMaxTileTypes || data_->type_plane[type] < 0) {
    return std::nullopt;
  }
  return static_cast<PlaneId>(data_->type_plane[type]);
}

Lookup<TileType> Technology::find_type(std::string_view name) const {
  return to_lookup<TileType>(name_type(*data_, name));
}

diag::Outcome<TypeMask> Technology::parse_type_list(std::string_view text) const {
  diag::CollectingSink sink;
  Reporter rep(sink);
  const TypeList list = name_mask(*data_, text, &rep, Noise::warning);
  const auto diagnostics = sink.take();
  if (list.errors && !diagnostics.empty()) {
    return diagnostics.front();
  }
  if (list.types.empty() && !diagnostics.empty()) {
    return diagnostics.front();
  }
  return list.types;
}

bool Technology::locked_by_default(TileType type) const noexcept {
  return type >= kMagnet && type < kMaxTileTypes && !data_->tech_active.test(type);
}

TypeMask Technology::plane_types(PlaneId plane) const {
  return plane < kMaxPlanes ? data_->plane_types[plane] : TypeMask{};
}

TypeMask Technology::home_plane_types(PlaneId plane) const {
  return plane < kMaxPlanes ? data_->home_plane_types[plane] : TypeMask{};
}

PlaneMask Technology::type_planes(TileType type) const noexcept {
  return data_->type_planes[clamp_type(type)];
}

std::vector<std::string> Technology::type_names(TileType type, bool with_aliases) const {
  std::vector<std::string> names;
  for (const auto& entry : data_->type_names) {
    if (entry.value == type && (with_aliases || !entry.alias)) {
      names.push_back(entry.name);
    }
  }
  return names;
}

std::vector<std::pair<std::string, TypeMask>> Technology::aliases() const {
  return {data_->aliases.begin(), data_->aliases.end()};
}

const TypeMask& Technology::active_types() const noexcept { return data_->active; }
const TypeMask& Technology::tech_active_types() const noexcept { return data_->tech_active; }

bool Technology::is_contact(TileType type) const noexcept {
  return type < kMaxTileTypes && data_->is_contact[type];
}

const ContactInfo* Technology::contact(TileType type) const noexcept {
  for (const auto& c : data_->contact_infos) {
    if (c.type == type) {
      return &c;
    }
  }
  return nullptr;
}

std::span<const ContactInfo> Technology::contacts() const noexcept { return data_->contact_infos; }

TileType Technology::residue_on(TileType contact, PlaneId plane) const noexcept {
  return contact < kMaxTileTypes ? plane_to_residue(*data_, contact, plane) : kSpace;
}

TypeMask Technology::full_residues(TileType type) const noexcept {
  return type < kMaxTileTypes ? tech::full_residues(*data_, type) : TypeMask{};
}

std::optional<TileType> Technology::contact_between(TileType t1, TileType t2) const noexcept {
  const PlaneMask planes = type_planes(t1) | type_planes(t2);
  for (std::size_t t = kFirstUserType; t < data_->num_types; ++t) {
    if (data_->is_contact[t] && data_->image_planes[t] == planes) {
      return static_cast<TileType>(t);
    }
  }
  return std::nullopt;
}

TileType Technology::paint_result(PlaneId plane, TileType have, TileType paint) const noexcept {
  if (have >= kMaxTileTypes || paint >= kMaxTileTypes) {
    return have;
  }
  return data_->paint_entry(have, paint, plane);
}

TileType Technology::erase_result(PlaneId plane, TileType have, TileType erase) const noexcept {
  if (have >= kMaxTileTypes || erase >= kMaxTileTypes) {
    return have;
  }
  return data_->erase_entry(have, erase, plane);
}

TileType Technology::write_result(TileType /*have*/, TileType paint) const noexcept { return paint; }

PlaneMask Technology::paint_planes(TileType type) const noexcept {
  return data_->paint_planes[clamp_type(type)];
}

PlaneMask Technology::erase_planes(TileType type) const noexcept {
  return data_->erase_planes[clamp_type(type)];
}

const TypeMask& Technology::connects_to(TileType type) const noexcept {
  return type < kMaxTileTypes ? data_->connect[type] : kNoTypes;
}

const TypeMask& Technology::not_connected(TileType type) const noexcept {
  return type < kMaxTileTypes ? data_->not_connect[type] : kNoTypes;
}

PlaneMask Technology::contact_planes(TileType type) const noexcept {
  return data_->conn_planes[clamp_type(type)];
}

PlaneMask Technology::cross_planes(TileType type) const noexcept {
  return data_->all_conn_planes[clamp_type(type)];
}

const StylesSection& Technology::styles() const noexcept { return data_->styles; }

const std::vector<RawSection>& Technology::raw_sections() const noexcept { return data_->raw_sections; }

const std::optional<CifGridInfo>& Technology::cif_output_grid() const noexcept { return data_->cif_grid; }

// --- runtime locking -------------------------------------------------------

namespace {

using Data = Technology::Data;

std::shared_ptr<const Technology> wrap(Data d) {
  publish_contacts(d);
  return std::make_shared<const Technology>(std::make_shared<const Data>(std::move(d)));
}

// The contact adjustment after `tech revert`.
void adjust_contacts_after_revert(Data& d) {
  for (std::size_t c = kFirstUserType; c < d.num_user; ++c) {
    if (!d.is_contact[c]) {
      continue;
    }
    if (d.active.test(static_cast<TileType>(c))) {
      unlock_contact(d, static_cast<TileType>(c));
    } else {
      lock_contact(d, static_cast<TileType>(c));
    }
  }
  for (std::size_t g = d.num_user; g < d.num_types; ++g) {
    const auto stacked = static_cast<TileType>(g);
    if (d.active.contains(d.residues[g])) {
      d.active.set(stacked);
      unlock_contact(d, stacked);
    } else if (d.active.intersects(d.residues[g])) {
      d.active.clear_type(stacked);
      lock_contact(d, stacked);
    }
  }
}

}  // namespace

std::shared_ptr<const Technology> lock_types(const Technology& tech, const TypeMask& types) {
  Data d = tech.data();
  d.active &= ~types;
  for (std::size_t c = kFirstUserType; c < d.num_user; ++c) {
    if (types.test(static_cast<TileType>(c)) && d.is_contact[c]) {
      lock_contact(d, static_cast<TileType>(c));
    }
  }
  for (std::size_t g = d.num_user; g < d.num_types; ++g) {
    if (types.intersects(d.residues[g])) {
      d.active.clear_type(static_cast<TileType>(g));
      lock_contact(d, static_cast<TileType>(g));
    }
  }
  return wrap(std::move(d));
}

std::shared_ptr<const Technology> unlock_types(const Technology& tech, const TypeMask& types) {
  Data d = tech.data();
  d.active |= types;
  for (std::size_t c = kFirstUserType; c < d.num_user; ++c) {
    if (types.test(static_cast<TileType>(c)) && d.is_contact[c]) {
      unlock_contact(d, static_cast<TileType>(c));
    }
  }
  for (std::size_t g = d.num_user; g < d.num_types; ++g) {
    if (d.active.contains(d.residues[g])) {
      d.active.set(static_cast<TileType>(g));
      unlock_contact(d, static_cast<TileType>(g));
    }
  }
  d.active &= all_but_space_and_drc();
  return wrap(std::move(d));
}

std::shared_ptr<const Technology> revert_types(const Technology& tech, const std::optional<TypeMask>& types) {
  Data d = tech.data();
  if (types) {
    d.active &= ~*types;
    d.active |= *types & d.tech_active;
  } else {
    d.active = d.tech_active;
  }
  adjust_contacts_after_revert(d);
  return wrap(std::move(d));
}

// --- TechnologyBuilder -----------------------------------------------------

struct TechnologyBuilder::State {
  diag::CollectingSink sink;
  Reporter rep{sink};
  BuildState build;
  int section = -1;  // the open section; sections are opened in registration order
  bool failed = false;

  State() {
    build.rep = &rep;
    build.d.name.clear();
  }

  void open(int next);
  diag::Status line(int section_id, std::span<const std::string> tokens);
};

namespace {

void run_init(BuildState& s, int section) {
  switch (section) {
    case kSecTech: tech_init(s); break;
    case kSecPlanes: planes_init(s); break;
    case kSecTypes: types_init(s); break;
    case kSecStyles: styles_init(s); break;
    case kSecContact: contact_init(s); break;
    case kSecCompose: compose_init(s); break;
    case kSecConnect: connect_init(s); break;
    default: break;
  }
}

void run_final(BuildState& s, int section) {
  switch (section) {
    case kSecTypes: types_final(s); break;
    case kSecContact: contact_final(s); break;
    case kSecCompose: compose_final(s); break;
    case kSecConnect: connect_final(s); break;
    default: break;
  }
}

bool run_line(BuildState& s, int section, Tokens tokens) {
  switch (section) {
    case kSecTech: return tech_line(s, tokens);
    case kSecVersion: return version_line(s, tokens);
    case kSecPlanes: return planes_line(s, tokens);
    case kSecTypes: return types_line(s, tokens);
    case kSecStyles: return styles_line(s, tokens);
    case kSecContact: return contact_line(s, tokens);
    case kSecAliases: return aliases_line(s, tokens);
    case kSecCompose: return compose_line(s, tokens);
    case kSecConnect: return connect_line(s, tokens);
    default: return true;
  }
}

}  // namespace

void section_init(BuildState& s, int section) { run_init(s, section); }
void section_final(BuildState& s, int section) { run_final(s, section); }
bool section_line(BuildState& s, int section, Tokens tokens) { return run_line(s, section, tokens); }

void TechnologyBuilder::State::open(int next) {
  // Sections up to `next` that have not been opened run init and final with no lines.
  while (section < next) {
    if (section >= 0) {
      run_final(build, section);
    }
    ++section;
    run_init(build, section);
  }
}

diag::Status TechnologyBuilder::State::line(int section_id, std::span<const std::string> tokens) {
  if (section_id < section) {
    return diag::make("TECH-LINE-001", "TechnologyBuilder: section " +
                                           std::string{section_table()[static_cast<std::size_t>(section_id)].name} +
                                           " given after a later section");
  }
  open(section_id);
  (void)sink.take();
  if (tokens.empty() || run_line(build, section_id, tokens)) {
    return diag::Done{};
  }
  failed = true;
  const auto diagnostics = sink.take();
  return diagnostics.empty() ? diag::make("TECH-LINE-001", "rejected line") : diagnostics.front();
}

TechnologyBuilder::TechnologyBuilder() : state_(std::make_unique<State>()) {}
TechnologyBuilder::~TechnologyBuilder() = default;
TechnologyBuilder::TechnologyBuilder(TechnologyBuilder&&) noexcept = default;
TechnologyBuilder& TechnologyBuilder::operator=(TechnologyBuilder&&) noexcept = default;

void TechnologyBuilder::set_magic_compat(bool magic_compat) {
  state_->build.magic_compat = magic_compat;
  state_->build.d.magic_compat = magic_compat;
}

void TechnologyBuilder::set_name(std::string name) { state_->build.d.name = std::move(name); }

diag::Outcome<PlaneId> TechnologyBuilder::add_plane(std::string_view names) {
  const std::vector<std::string> tokens{std::string{names}};
  if (auto status = state_->line(kSecPlanes, tokens); !status) {
    return status.error();
  }
  return static_cast<PlaneId>(state_->build.d.num_planes() - 1);
}

diag::Outcome<TileType> TechnologyBuilder::add_type(std::string_view plane, std::string_view names) {
  const std::vector<std::string> tokens{std::string{plane}, std::string{names}};
  if (auto status = state_->line(kSecTypes, tokens); !status) {
    return status.error();
  }
  return static_cast<TileType>(state_->build.d.num_types - 1);
}

diag::Status TechnologyBuilder::add_alias(std::string_view name, std::string_view types) {
  const std::vector<std::string> tokens{std::string{name}, std::string{types}};
  return state_->line(kSecAliases, tokens);
}

diag::Status TechnologyBuilder::add_contact(std::string_view type, std::span<const std::string_view> residues) {
  std::vector<std::string> tokens{std::string{type}};
  tokens.insert(tokens.end(), residues.begin(), residues.end());
  return state_->line(kSecContact, tokens);
}

diag::Status TechnologyBuilder::add_stackable(std::span<const std::string_view> types) {
  std::vector<std::string> tokens{"stackable"};
  tokens.insert(tokens.end(), types.begin(), types.end());
  return state_->line(kSecContact, tokens);
}

diag::Status TechnologyBuilder::add_compose_rule(std::string_view kind, std::span<const std::string_view> types) {
  std::vector<std::string> tokens{std::string{kind}};
  tokens.insert(tokens.end(), types.begin(), types.end());
  return state_->line(kSecCompose, tokens);
}

diag::Status TechnologyBuilder::add_connect(std::string_view types_a, std::string_view types_b) {
  const std::vector<std::string> tokens{std::string{types_a}, std::string{types_b}};
  return state_->line(kSecConnect, tokens);
}

diag::Status TechnologyBuilder::add_line(std::string_view section, std::span<const std::string_view> tokens) {
  const int id = find_section(section);
  if (id < 0) {
    return diag::make("TECH-SECTION-001", "Unrecognized section name: " + std::string{section});
  }
  const std::vector<std::string> owned(tokens.begin(), tokens.end());
  return state_->line(id, owned);
}

diag::Outcome<std::shared_ptr<const Technology>> TechnologyBuilder::finalize() {
  state_->open(kSecConnect);
  run_final(state_->build, kSecConnect);
  const bool failed = state_->failed;
  Data data = std::move(state_->build.d);
  state_ = std::make_unique<State>();
  if (failed) {
    return diag::make("TECH-LOAD-001", "Error in building technology (a rejected line)");
  }
  if (data.name.empty()) {
    data.name = "built";
  }
  return wrap(std::move(data));
}

}  // namespace hefesto::tech
