// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE and sim name conversion (see hefesto/extflat/names.hpp; capability
// ext-flattening-names "SPICE output name conversion", "Sim output name
// conversion"; Q-NETLIST-16, Q-NETLIST-30).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#include "hefesto/extflat/names.hpp"

namespace hefesto::extflat {

namespace {

bool any_flag(const NameFlags& f) noexcept {
  return f.convert_comma || f.convert_equal || f.convert_brackets || f.trim_global || f.trim_local;
}

bool is_bracket(char c) noexcept {
  return c == '[' || c == ']';
}

// efHNSprintfPrefix: one non-leaf component and its '/'. A ',' without
// convert-comma is deleted; Magic deletes it by stepping its output back,
// which writes before the buffer for a leading ',' of the root component
// (Q-NETLIST-30: guarded in both modes).
void spice_prefix(std::string& out, const std::string& component, const NameFlags& f) {
  for (const char c : component) {
    if (f.convert_equal && c == '=') {
      out += ':';
    } else if (f.convert_brackets && is_bracket(c)) {
      out += '_';
    } else if (c == ',') {
      if (f.convert_comma) {
        out += '|';
      }
    } else {
      out += c;
    }
  }
  out += '/';
}

// Whether a leaf `!` (trim-global) or `#` (trim-local) is deleted: anywhere
// with magic-compat ON, only as the leaf's last character with it OFF
// (Q-NETLIST-30).
bool trims(char c, bool last, const NameFlags& f, bool compat) noexcept {
  const bool flag = (c == '!' && f.trim_global) || (c == '#' && f.trim_local);
  return flag && (compat || last);
}

// One leaf character of EFHNSprintf; `last` tells whether it ends the
// leaf, which matters for `!` and `#` with magic-compat OFF.
void spice_leaf_char(std::string& out, char c, bool last, bool hspice, const NameFlags& f, bool compat) {
  switch (c) {
    case '!':
    case '#':
      if (!trims(c, last, f, compat)) {
        out += c;
      }
      break;
    case '.': out += hspice ? '@' : '.'; break;
    case '=':
      if (f.convert_equal) {
        out += ':';
      }
      break;
    case ',':
      if (f.convert_comma) {
        out += '|';
      }
      break;
    case '[':
    case ']': out += f.convert_brackets ? '_' : c; break;
    default: out += c; break;
  }
}

}  // namespace

std::string name_to_spice(std::span<const std::string> components, SpiceDialect dialect, NameFlags& flags,
                          const compat::CompatSettings& compat) {
  std::string out;
  if (components.empty()) {
    return out;
  }
  for (std::size_t i = 0; i + 1 < components.size(); ++i) {
    spice_prefix(out, components[i], flags);
  }
  const std::string& leaf = components.back();
  if (!any_flag(flags)) {
    return out + leaf;
  }
  const bool hspice = dialect == SpiceDialect::hspice;
  for (std::size_t i = 0; i < leaf.size(); ++i) {
    spice_leaf_char(out, leaf[i], i + 1 == leaf.size(), hspice, flags, compat.magic_compat);
  }
  return out;
}

namespace {

// One character of a sim leaf before its last: ',' becomes '|' or goes,
// a bracket becomes '_' with convert-brackets.
void sim_leaf_char(std::string& out, char c, const NameFlags& flags) {
  if (c == ',') {
    if (flags.convert_comma) {
      out += '|';
    }
  } else if (flags.convert_brackets && is_bracket(c)) {
    out += '_';
  } else {
    out += c;
  }
}

}  // namespace

std::string name_to_sim(std::span<const std::string> components, NameFlags& flags,
                        const compat::CompatSettings& /*compat*/) {
  std::string out;
  if (components.empty()) {
    return out;
  }
  for (std::size_t i = 0; i + 1 < components.size(); ++i) {
    out += components[i];
    out += '/';
  }
  const std::string& leaf = components.back();
  if (!any_flag(flags) || leaf.empty()) {
    return out + leaf;
  }
  for (std::size_t i = 0; i + 1 < leaf.size(); ++i) {
    sim_leaf_char(out, leaf[i], flags);
  }
  const char last = leaf.back();
  if (!((last == '!' && flags.trim_global) || (last == '#' && flags.trim_local))) {
    out += last;
  }
  return out;
}

void add_trim_chars(NameFlags& flags, std::string_view chars) noexcept {
  for (const char c : chars) {
    switch (c) {
      case '!': flags.trim_global = true; break;
      case '#': flags.trim_local = true; break;
      case ',': flags.convert_comma = true; break;
      case '=': flags.convert_equal = true; break;
      case '[':
      case ']': flags.convert_brackets = true; break;
      default: break;
    }
  }
}

namespace {

// ext2spice's base flags added to `flags` (ext2spice.c:1071-1076).
void apply_spice_base(NameFlags& flags, SpiceDialect dialect) noexcept {
  flags.trim_global = true;
  flags.convert_equal = true;
  flags.convert_comma = true;
  if (dialect == SpiceDialect::hspice) {
    flags.trim_local = true;
  }
  if (dialect == SpiceDialect::cdl) {
    flags.trim_global = false;
  }
}

}  // namespace

NameFlags spice_run_flags(NameFlags& session, SpiceDialect dialect, std::string_view trim_chars,
                          const compat::CompatSettings& compat) noexcept {
  if (compat.magic_compat) {
    add_trim_chars(session, trim_chars);  // EFArgs runs first
    apply_spice_base(session, dialect);
    return session;
  }
  NameFlags run = no_name_flags();
  apply_spice_base(run, dialect);
  add_trim_chars(run, trim_chars);
  return run;
}

NameFlags sim_run_flags(NameFlags& session, std::string_view trim_chars,
                        const compat::CompatSettings& compat) noexcept {
  if (compat.magic_compat) {
    add_trim_chars(session, trim_chars);
    return session;
  }
  NameFlags run = no_name_flags();
  add_trim_chars(run, trim_chars);
  return run;
}

}  // namespace hefesto::extflat
