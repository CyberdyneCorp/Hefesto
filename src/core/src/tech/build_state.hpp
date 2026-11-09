// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TECH-private: the mutable technology under construction, the diagnostic
// reporter of the section handlers, and the handler entry points. Each
// handler mirrors one Magic tech client (init / line / final) and returns
// whether the line was accepted (a rejected line fails the load).
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "tech/technology_data.hpp"

namespace hefesto::tech {

using Tokens = std::span<const std::string>;

/// How a name-resolution helper reports a failure.
enum class Noise : std::uint8_t {
  quiet,    // nothing
  warning,  // reported, the line is not rejected because of it
  error,    // reported, the caller rejects the line
};

/// Emits handler diagnostics with the current file/line/section.
class Reporter {
 public:
  explicit Reporter(diag::DiagnosticSink& sink) : sink_(sink) {}

  void set_location(diag::SourceLocation where, std::string section) {
    where_ = std::move(where);
    section_ = std::move(section);
  }
  /// A name lookup after a load (a command): Magic's TechError still prints
  /// TechPrintLine's context, the last file read, its final techLineNumber and
  /// section "(none)". Nothing changes for a technology not read from a file.
  void locate_after_load(const std::string& file, std::uint32_t end_line) {
    if (!file.empty()) {
      where_ = diag::SourceLocation{file, end_line};
      section_ = "(none)";
      stamp_section_ = true;
    }
  }
  [[nodiscard]] const std::string& section() const noexcept { return section_; }
  [[nodiscard]] const diag::SourceLocation& where() const noexcept { return where_; }

  /// TECH-LINE-001: the message of a rejected line.
  void error(std::string text) { emit(kLineError, std::move(text)); }
  /// TECH-LINE-002: a problem that does not fail the load.
  void warning(std::string text) { emit(kLineWarning, std::move(text)); }
  /// TECH-LINE-003: an informational message.
  void info(std::string text) { emit(kLineInfo, std::move(text)); }
  void report(Noise noise, std::string text) {
    if (noise == Noise::error) {
      error(std::move(text));
    } else if (noise == Noise::warning) {
      warning(std::move(text));
    }
  }
  /// Any TECH- code at the current location.
  void emit(std::string_view code, std::string text);

  [[nodiscard]] diag::DiagnosticSink& sink() noexcept { return sink_; }

  static constexpr std::string_view kLineError = "TECH-LINE-001";
  static constexpr std::string_view kLineWarning = "TECH-LINE-002";
  static constexpr std::string_view kLineInfo = "TECH-LINE-003";

 private:
  diag::DiagnosticSink& sink_;
  diag::SourceLocation where_;
  std::string section_;
  bool stamp_section_ = false;  // emit() adds the section itself (no SectionStampSink)
};

/// The technology under construction.
struct BuildState {
  Technology::Data d = Technology::Data::builtin();
  Reporter* rep = nullptr;
  bool magic_compat = true;

  [[nodiscard]] Reporter& r() const noexcept { return *rep; }
};

// --- type names and type lists (type_names.cpp) ----------------------------

/// DBTechNameType: a type number, kAmbiguous or kNotFound.
[[nodiscard]] int name_type(const Technology::Data& d, std::string_view name);
/// DBTechNoisyNameType.
[[nodiscard]] int noisy_name_type(const Technology::Data& d, std::string_view name, Reporter* rep, Noise noise);
/// DBTechNameTypeExact.
[[nodiscard]] int name_type_exact(const Technology::Data& d, std::string_view name);
/// DBTechNamePlane / DBTechNoisyNamePlane.
[[nodiscard]] int name_plane(const Technology::Data& d, std::string_view name);
[[nodiscard]] int noisy_name_plane(const Technology::Data& d, std::string_view name, Reporter* rep, Noise noise);

/// A parsed type list: its types and plane set (DBTechNameMask0). With
/// magic-compat OFF, `errors` tells that a plane qualifier was bad
/// (Q-TECH-16), which the caller treats as a rejected line.
struct TypeList {
  TypeMask types;
  PlaneMask planes = 0;
  bool errors = false;
};
[[nodiscard]] TypeList name_mask(const Technology::Data& d, std::string_view text, Reporter* rep, Noise noise);
/// DBTechTypesToPlanes.
[[nodiscard]] PlaneMask types_to_planes(const Technology::Data& d, const TypeMask& mask);
/// DBTypeShortName.
[[nodiscard]] std::string type_short_name(const Technology::Data& d, TileType type);
/// DBFullResidueMask.
[[nodiscard]] TypeMask full_residues(const Technology::Data& d, TileType type);
/// DBPlaneToResidue.
[[nodiscard]] TileType plane_to_residue(const Technology::Data& d, TileType type, int plane);

// --- section handlers ----------------------------------------------------
// Handlers are called with at least one token.

void tech_init(BuildState& s);
bool tech_line(BuildState& s, Tokens tokens);
bool version_line(BuildState& s, Tokens tokens);
void planes_init(BuildState& s);
bool planes_line(BuildState& s, Tokens tokens);
void types_init(BuildState& s);
bool types_line(BuildState& s, Tokens tokens);
void types_final(BuildState& s);
bool aliases_line(BuildState& s, Tokens tokens);
void styles_init(BuildState& s);
bool styles_line(BuildState& s, Tokens tokens);

// contacts.cpp
/// dbTechMatchResidues(contactsOnly): user contacts whose residue set is `residues`.
[[nodiscard]] TypeMask match_residues(const Technology::Data& d, const TypeMask& residues);
/// DBTechFindStacking: the stacked type built from {a, b}, or -1.
[[nodiscard]] int find_stacking(const Technology::Data& d, TileType a, TileType b);
void contact_init(BuildState& s);
bool contact_line(BuildState& s, Tokens tokens);
void contact_final(BuildState& s);

// compose.cpp
void compose_init(BuildState& s);
bool compose_line(BuildState& s, Tokens tokens);
void compose_final(BuildState& s);
/// DBLockContact / DBUnlockContact.
void lock_contact(Technology::Data& d, TileType contact);
void unlock_contact(Technology::Data& d, TileType contact);

/// dbComposeEraseContact (finalization step 3b for one pair).
void erase_contact(Technology::Data& d, TileType image, TileType erase);

// connect.cpp
void connect_init(BuildState& s);
bool connect_line(BuildState& s, Tokens tokens);
void connect_final(BuildState& s);

// technology.cpp
/// Fills Data::contact_infos from the contact list (each contact once).
void publish_contacts(Technology::Data& d);
/// Section dispatch by section number (tech/reader.hpp SectionId): the
/// init, line and final procedures of the M1-interpreted sections; other
/// sections have none (the loader keeps them raw).
void section_init(BuildState& s, int section);
void section_final(BuildState& s, int section);
[[nodiscard]] bool section_line(BuildState& s, int section, Tokens tokens);

}  // namespace hefesto::tech
