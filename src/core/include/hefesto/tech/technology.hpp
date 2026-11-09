// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The technology database: an immutable, fully finalized technology as the
// rest of Hefesto sees it. Built by the loader (hefesto/tech/loader.hpp) from
// a parsed technology file, or by TechnologyBuilder in tests.
//
// Capabilities: tech-layers-and-planes (planes, types, aliases, type lists,
// names), tech-contacts-and-stacking (contacts, residues, stacked types),
// tech-compose-and-connect (paint/erase result tables, affected planes,
// connectivity), tech-styles-section (raw type-to-style references; the
// display module resolves them against a .dstyle file), tech-file-format
// (tech/version sections).
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::tech {

/// Result of a unique-prefix name lookup ("Name lookup by unique prefix").
enum class LookupError : std::uint8_t { unknown, ambiguous };

/// A type or plane lookup: the number, or why there is none.
template <typename Id>
struct Lookup {
  std::optional<Id> id;
  LookupError error = LookupError::unknown;  // meaningful only when !id

  [[nodiscard]] bool ok() const noexcept { return id.has_value(); }
};

/// One contact or generated stacked type (tech-contacts-and-stacking).
struct ContactInfo {
  TileType type = kSpace;
  TypeMask residues;          // the residue types, one per plane
  PlaneMask planes = 0;       // planes on which the contact has an image
  bool stacked = false;       // a generated stacked type
  TileType component_a = kSpace;  // stacked only: the two component contacts
  TileType component_b = kSpace;
};

/// The `version` section ("version section").
struct TechVersion {
  std::vector<std::string> version_lines;      // `version` statements, in order
  std::vector<std::string> description_lines;  // `description` statements, in order
};

/// One raw type-to-style line of the `styles` section ("Type to style
/// mapping"). The references are resolved against the display-style file by
/// display::resolve_type_styles(); a technology loads without display files.
struct StyleRefLine {
  TileType type = kSpace;
  std::vector<std::string> refs;  // integer style numbers or style long names
  std::uint32_t line = 0;         // source line, for "Invalid style" reports
  /// "Contact style propagation": the stacked types that existed when the
  /// line was read, have `type` as a component and share its home plane.
  TypeMask stacked;
};

/// The `styles` section as read ("styles section syntax", "styletype statement").
struct StylesSection {
  std::string style_family;            // `styletype` family, truncated to 49 characters; empty if none
  std::vector<std::string> search_dirs;  // optional `styletype` directories
  std::vector<StyleRefLine> lines;
};

/// One line of a section kept verbatim for a later milestone (cifoutput,
/// cifinput, drc, extract, lef, wiring, router, plowing, plot, mzrouter).
struct RawLine {
  std::vector<std::string> tokens;
  std::string file;
  std::uint32_t line = 0;
};

/// A section read but not interpreted in M1 ("skipped but preserved").
/// A repeated section appears once per occurrence, in file order.
struct RawSection {
  std::string name;
  std::vector<RawLine> lines;
};

/// The grid values of the first `cifoutput` style that the M1 loader reads
/// for "Grid limit check" and "Post-load adjustments" (the full style
/// semantics belong to the mask-* capabilities, M4).
struct CifGridInfo {
  std::int64_t scale_factor = 1;  // `scalefactor` (centimicrons, or nanometers with the keyword)
  std::int64_t expander = 1;      // 10 when `scalefactor` is given in nanometers
  std::int64_t grid_limit = 0;    // `gridlimit`, same unit as the scale factor
  bool minimum_grid = false;      // `options set-minimum-grid`
};

/// Reduced fraction "internal units = lambda" ("Internal grid scale").
/// A session-wide value held by editor::EditorState, not by the technology.
struct GridScale {
  std::int64_t internal = 1;
  std::int64_t lambda = 1;

  friend bool operator==(const GridScale&, const GridScale&) = default;
};

/// The finalized technology. Immutable and cheap to share
/// (std::shared_ptr<const Technology>); a reload produces a new object.
/// A default-constructed Technology holds only the built-in planes and types.
class Technology {
 public:
  struct Data;  // TECH-private representation (src/core/src/tech/)

  Technology();
  explicit Technology(std::shared_ptr<const Data> data);

  // --- identity (tech-file-format "tech section", "version section") ---
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::string& file_name() const noexcept;  // path it was loaded from; empty if built
  [[nodiscard]] int format_version() const noexcept;            // `format` of the tech section; 0 if absent
  [[nodiscard]] const TechVersion& version() const noexcept;
  /// "Format version gating" item 2: the fringe-halo extraction default
  /// (format >= 34), taken when the `extract` section ends (magic-compat
  /// ON) or after the whole file (OFF), per Q-TECH-32.
  [[nodiscard]] bool fringe_halo_default() const noexcept;

  // --- planes (tech-layers-and-planes) ---
  [[nodiscard]] std::size_t plane_count() const noexcept;  // built-in planes included
  [[nodiscard]] std::string_view plane_name(PlaneId plane, NameForm form = NameForm::long_name) const;
  [[nodiscard]] Lookup<PlaneId> find_plane(std::string_view name) const;

  // --- types (tech-layers-and-planes) ---
  [[nodiscard]] std::size_t type_count() const noexcept;       // number of defined types, incl. stacked
  [[nodiscard]] std::size_t user_type_end() const noexcept;    // U: first number after the user types
  [[nodiscard]] std::string_view type_name(TileType type, NameForm form = NameForm::long_name) const;
  [[nodiscard]] std::optional<PlaneId> home_plane(TileType type) const noexcept;  // none for space
  /// "Single type-name resolution": unique prefix, aliases, `*` forms excluded.
  [[nodiscard]] Lookup<TileType> find_type(std::string_view name) const;
  /// "Type-list language": e.g. "metal1,via1/metal1,~(ndiff)". Diagnostics name the bad element.
  [[nodiscard]] diag::Outcome<TypeMask> parse_type_list(std::string_view text) const;
  /// "Locked types" at load time; runtime locking is editor state (layout-paint-semantics).
  [[nodiscard]] bool locked_by_default(TileType type) const noexcept;
  /// Types whose images live on `plane` (DBPlaneTypes, OnPlane(p)).
  [[nodiscard]] TypeMask plane_types(PlaneId plane) const;
  /// Types whose home plane is `plane` (DBHomePlaneTypes, Home(p)).
  [[nodiscard]] TypeMask home_plane_types(PlaneId plane) const;
  /// Planes(t) of "Derived type and plane tables" (DBTypePlaneMaskTbl).
  [[nodiscard]] PlaneMask type_planes(TileType type) const noexcept;
  /// All names of `type` in byte-wise order; alias names (single-type
  /// aliases, stackable aliases) only when `with_aliases`.
  [[nodiscard]] std::vector<std::string> type_names(TileType type, bool with_aliases = false) const;
  /// The multi-type aliases (alias table), by name.
  [[nodiscard]] std::vector<std::pair<std::string, TypeMask>> aliases() const;

  // --- active (locked) layers ("Locked types", tech-command) ---
  /// The current active set (DBActiveLayerBits).
  [[nodiscard]] const TypeMask& active_types() const noexcept;
  /// The tech-file active set fixed when `types` ended (DBTechActiveLayerBits).
  [[nodiscard]] const TypeMask& tech_active_types() const noexcept;

  // --- contacts (tech-contacts-and-stacking) ---
  [[nodiscard]] bool is_contact(TileType type) const noexcept;
  [[nodiscard]] const ContactInfo* contact(TileType type) const noexcept;  // nullptr if not a contact
  [[nodiscard]] std::span<const ContactInfo> contacts() const noexcept;  // user contacts then stacked types
  /// "Residue on a plane": the residue of `contact` on `plane`, or space.
  [[nodiscard]] TileType residue_on(TileType contact, PlaneId plane) const noexcept;
  /// Res(t) for a user type; the full residue set (union of the component
  /// contacts' residues) for a stacked type (DBFullResidueMask).
  [[nodiscard]] TypeMask full_residues(TileType type) const noexcept;
  /// "Contact lookup by planes" (DBTechGetContact): the lowest contact whose
  /// plane set equals Planes(t1) | Planes(t2); std::nullopt if none.
  [[nodiscard]] std::optional<TileType> contact_between(TileType t1, TileType t2) const noexcept;

  // --- paint / erase tables (tech-compose-and-connect, finalized) ---
  /// DBPaintResultTbl[plane][paint][have]: the type left on `plane` when
  /// `paint` is painted over `have`.
  [[nodiscard]] TileType paint_result(PlaneId plane, TileType have, TileType paint) const noexcept;
  /// DBEraseResultTbl[plane][erase][have].
  [[nodiscard]] TileType erase_result(PlaneId plane, TileType have, TileType erase) const noexcept;
  /// DBWriteResultTbl[paint][have] (write = replace).
  [[nodiscard]] TileType write_result(TileType have, TileType paint) const noexcept;
  /// "Affected-plane tables": planes touched by painting / erasing `type`.
  [[nodiscard]] PlaneMask paint_planes(TileType type) const noexcept;
  [[nodiscard]] PlaneMask erase_planes(TileType type) const noexcept;

  // --- connectivity (tech-compose-and-connect "connect section") ---
  [[nodiscard]] const TypeMask& connects_to(TileType type) const noexcept;
  /// NotConn(t) of "Connectivity finalization" (DBNotConnectTbl).
  [[nodiscard]] const TypeMask& not_connected(TileType type) const noexcept;
  /// ContactPlanes(t) (DBConnPlanes) and CrossPlanes(t) (DBAllConnPlanes).
  [[nodiscard]] PlaneMask contact_planes(TileType type) const noexcept;
  [[nodiscard]] PlaneMask cross_planes(TileType type) const noexcept;

  // --- styles (tech-styles-section, raw) ---
  [[nodiscard]] const StylesSection& styles() const noexcept;

  // --- sections kept for later milestones ---
  [[nodiscard]] const std::vector<RawSection>& raw_sections() const noexcept;
  /// The first `cifoutput` style's grid values; std::nullopt when the
  /// technology has no output style (the grid-limit check then passes).
  [[nodiscard]] const std::optional<CifGridInfo>& cif_output_grid() const noexcept;

  [[nodiscard]] const Data& data() const noexcept { return *data_; }

 private:
  std::shared_ptr<const Data> data_;
};

// --- runtime locking ("tech locked|unlocked|revert", tech-command) --------
// Each returns a new technology whose active set and contact erase rules
// ("Contact locking") are changed; the argument is left untouched.

/// `tech locked <list>`: removes `types` from the active set, locks the user
/// contacts among them and every stacked type with a component in `types`.
[[nodiscard]] std::shared_ptr<const Technology> lock_types(const Technology& tech, const TypeMask& types);
/// `tech unlocked <list>`.
[[nodiscard]] std::shared_ptr<const Technology> unlock_types(const Technology& tech, const TypeMask& types);
/// `tech revert` (no list: the whole tech-file active set) and `tech revert <list>`.
[[nodiscard]] std::shared_ptr<const Technology> revert_types(const Technology& tech,
                                                             const std::optional<TypeMask>& types);

/// Programmatic construction, used by the loader's section handlers and by
/// tests of other workstreams that need a small technology without a file.
/// finalize() runs the compose/contact/connect finalization of
/// tech-compose-and-connect in the specified order.
class TechnologyBuilder {
 public:
  TechnologyBuilder();
  ~TechnologyBuilder();
  TechnologyBuilder(TechnologyBuilder&&) noexcept;
  TechnologyBuilder& operator=(TechnologyBuilder&&) noexcept;
  TechnologyBuilder(const TechnologyBuilder&) = delete;
  TechnologyBuilder& operator=(const TechnologyBuilder&) = delete;

  /// Lookups follow `magic_compat` (Q-TECH-13/14/15/16); default ON.
  void set_magic_compat(bool magic_compat);
  void set_name(std::string name);
  /// `planes` line: comma-separated names, long name first.
  [[nodiscard]] diag::Outcome<PlaneId> add_plane(std::string_view names);
  /// `types` line: plane name and comma-separated type names.
  [[nodiscard]] diag::Outcome<TileType> add_type(std::string_view plane, std::string_view names);
  /// `aliases` line: alias name and type list.
  [[nodiscard]] diag::Status add_alias(std::string_view name, std::string_view types);
  /// `contact` line: contact type name and its residue type names.
  [[nodiscard]] diag::Status add_contact(std::string_view type, std::span<const std::string_view> residues);
  /// `stackable` statement (type names and alias names; empty = all contacts).
  [[nodiscard]] diag::Status add_stackable(std::span<const std::string_view> types);
  /// `compose` section line: rule kind (compose, decompose, paint, erase) and types.
  [[nodiscard]] diag::Status add_compose_rule(std::string_view kind, std::span<const std::string_view> types);
  /// `connect` section line: two type lists.
  [[nodiscard]] diag::Status add_connect(std::string_view types_a, std::string_view types_b);

  /// Any section line (tokens as read), for sections without a dedicated
  /// method (e.g. `styles`). Sections must come in registration order.
  [[nodiscard]] diag::Status add_line(std::string_view section, std::span<const std::string_view> tokens);

  /// Finalizes the tables. The builder is left empty.
  [[nodiscard]] diag::Outcome<std::shared_ptr<const Technology>> finalize();

 private:
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace hefesto::tech
